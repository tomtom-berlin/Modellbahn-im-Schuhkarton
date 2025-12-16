// ATmega328 Dekoder für Neopixel-Kette aus max. 16 Neopixel,
// 5 "Aussenleuchten" und 1 Weiche,
// Basisadresse: 1
//
// Basisadresse:     Aussenlicht oder "Tageslicht"
// Basisadresse + 1: Innenbeleuchtung
// Basisadresse + 2: "Tageslicht" aus

/*
  ATmeag328p

  RST, RX, TX, GND für Programmierung an USB-Serial-TTL-Adapter
  an Ausgängen: Strom messen, 100 mA sollten insgesamt nicht überschritten werden, je Ausgang max. 10 mA
                           
        100n          +------------------+
  RST ---||--+-----   | 1             28 | 
  RX         |        | 2             27 | 
  TX         |        | 3             26 | 
            | |   DCC | 4  2          25 | 
        10k | |       | 5             24 |  
             |        | 6             23 | 
             +------  | 7 VCC     GND 22 |
                      | 8 GND     VCC 21 |
                      | 9         VCC 20 |
                      | 10            19 | 
 Güterschuppen-Treppe | 11 5       12 18 | 
           Kandelaber | 12 6       11 17 | Gleisfeld
                      | 13         10 16 | Tankstelle
Neopixel --| 470 |--- | 14 8        9 15 | IRLZ34 G Tageslicht-LED-Streifen
                      +------------------+
const uint8_t AUSSENLICHT_PINS[] = { 9, 6, 5, 10, 11 };  // Reihenfolge: Tageslicht, Kandelaber, Güterschuppen-Treppe, Tankstelle, Gleisfeld


   CV-Plan:
   CV64: PWM für "Tageslicht"
   CV65: für Brückenkandelaber
   CV66: PWM für Laterne über der Tür am Schuppen
   CV67: PWM für Tankstelle
   CV68: PWM für Gleisfeld-Leuchten
   CV69: Neopixel-Farbe, R
   CV70: Neopixel-Farbe, G
   CV71: Neopixel-Farbe, B
*/
// #define _TEST_
// #define _DEBUG_INNEN_
// #define _DEBUG_AUSSEN_
#include <Arduino.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
#include <NmraDcc.h>

NmraDcc Dcc;

#define CALC_U32_CV_VALUE(cv_lsb, cv_msb) ((uint32_t)((Dcc.getCV(cv_msb) * 256) + Dcc.getCV(cv_lsb)))
#define CALC_U8_LSB_VALUE(value) ((unsigned char)(value & 0xff))
#define CALC_U8_MSB_VALUE(value) ((unsigned char)((value >> 8) & 0xff))

struct CVPair {
  uint16_t CV;
  uint8_t Value;
};

enum {
  CV_PWM_TAGESLICHT = 60,
  CV_PWM_KANDELABER,
  CV_PWM_GUETERSCHUPPEN_TREPPE,
  CV_PWM_TANKSTELLE,
  CV_PWM_GLEISFELD,
  CV_COLOR_INNENLICHT_R,
  CV_COLOR_INNENLICHT_G,
  CV_COLOR_INNENLICHT_B,
};

enum {
  TAGESLICHT = 0,
  KANDELABER,
  GUETERSCHUPPEN_TREPPE,
  TANKSTELLE,
  GLEISFELD,
};

const uint8_t DCC_PIN = 2;

const uint8_t NEOPIXEL_PIN_INNEN = 8;

const uint8_t AUSSENLICHT_PINS[] = { 9, 6, 5, 10, 11 };  // Reihenfolge: Tageslicht, Kandelaber, Güterschuppen-Treppe, Tankstelle, Gleisfeld
const unsigned num_aussenlichter = sizeof(AUSSENLICHT_PINS) / sizeof(AUSSENLICHT_PINS[0]);
uint8_t pwm_aussenlichter[num_aussenlichter] = { 48, 128, 128, 128, 128 };

const int LEDS_INNENLICHT = 16;


Adafruit_NeoPixel licht_innen = Adafruit_NeoPixel(LEDS_INNENLICHT, NEOPIXEL_PIN_INNEN, NEO_GRB + NEO_KHZ800);

// Lichter bezogen
uint16_t innenbeleuchtung_status = 0;
uint16_t innenbeleuchtung_status_alt = ~0;
uint8_t neopixel_color_r = 24;
uint8_t neopixel_color_g = 12;
uint8_t neopixel_color_b = 6;
uint8_t licht = 0;

uint16_t baseAddr = DEFAULT_ACCESSORY_DECODER_ADDRESS;

CVPair FactoryDefaultCVs[] = {
  { CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_ACCESSORY_DECODER_ADDRESS },
  { CV_ACCESSORY_DECODER_ADDRESS_MSB, 0 },
  { CV_PWM_TAGESLICHT, pwm_aussenlichter[TAGESLICHT] },
  { CV_PWM_KANDELABER, pwm_aussenlichter[KANDELABER] },
  { CV_PWM_GUETERSCHUPPEN_TREPPE, pwm_aussenlichter[GUETERSCHUPPEN_TREPPE] },
  { CV_PWM_TANKSTELLE, pwm_aussenlichter[TANKSTELLE] },
  { CV_PWM_GLEISFELD, pwm_aussenlichter[GLEISFELD] },
  { CV_COLOR_INNENLICHT_R, 32 },
  { CV_COLOR_INNENLICHT_G, 24 },
  { CV_COLOR_INNENLICHT_B, 16 },
};

#ifdef _TEST_
uint8_t FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);
#else
uint8_t FactoryDefaultCVIndex = 0;
#endif


void innenlicht_aus(int pixel) {
  licht_innen.setPixelColor(pixel, licht_innen.Color(0, 0, 0));
}

void innenlicht_an(int pixel) {
  if (pixel < 12) {
    licht_innen.setPixelColor(pixel, licht_innen.Color(neopixel_color_r, neopixel_color_g, neopixel_color_b));
  } else {
    licht_innen.setPixelColor(pixel, licht_innen.Color(neopixel_color_r / 3, neopixel_color_g / 3, neopixel_color_b / 3));
  }
}

void innenbeleuchtung_steuern() {
  if (innenbeleuchtung_status_alt != innenbeleuchtung_status) {
    for (int raum = 0; raum < LEDS_INNENLICHT; raum++) {
      uint16_t status = innenbeleuchtung_status & (1 << raum);
#ifdef _DEBUG_INNEN_
      Serial.print(" Status: Raum ");
      Serial.print(raum);
      Serial.print(" ist ");
      Serial.println(status & innenbeleuchtung_status ? "an" : "aus");
#endif
      if (status) {
        innenlicht_an(raum);
      } else {
        innenlicht_aus(raum);
      }
    }
#ifdef _DEBUG_INNEN_
    Serial.println("------------------");
#endif
    innenbeleuchtung_status_alt = innenbeleuchtung_status;
    licht_innen.show();
  }
}

/* stellt einen 16-Bit-Wert für die Lichter in den Gebäuden her 
   Bit  0-11: Industriebau (immer mind. 2 Lichter in Etagen 1 und 2 an)
              Bit-Reihenfolge zu Raum:
              +----+----+----+----+
              |  3 |  4 | 7  |  8 |
              +----+----+----+----+
              |  2 |  5 | 6  |  9 |
              +----+----+----+----+
              |  1 |         | 10 |
              +----+         +----+
              |  0 |         | 11 |
              +----+         +----+

   Bit 12-13: Güterschuppen Boden (immer an)
   Bit 14-15: Keller
              +----+----+
              | 14 | 15 |
              +----+----+
              | 13 | 12 |
              +----+----+

*/

// die Ausgänge der Einzel-LED sind LOW-aktiv, ca. 1.5 mA je LED, der Streifen ist HIGH-aktiv, ca. 90 mA
void set_aussenlicht(uint8_t an, uint8_t id) {
  if (id == TAGESLICHT) {
    if (an) {
      digitalWrite(AUSSENLICHT_PINS[id], LOW);
    } else {
      analogWrite(AUSSENLICHT_PINS[id], pwm_aussenlichter[id]);
    }
  } else {
    if (an) {
      analogWrite(AUSSENLICHT_PINS[id], pwm_aussenlichter[id]);
    } else {
      digitalWrite(AUSSENLICHT_PINS[id], HIGH);
    }
  }
}

void innenbeleuchtung() {
  uint16_t immer_an = 0b1100000000000000;
  uint16_t immer_an_msk = 0b1100011000000110;
  uint16_t alles_aus = 0b0000000000000000;
  if (!licht) {
    innenbeleuchtung_status = alles_aus;
  } else {
    innenbeleuchtung_status = immer_an | (uint16_t)((random() % 512 + 1) & immer_an_msk);
    innenbeleuchtung_status |= (uint16_t)((random() % 4096 + 1) & ~immer_an_msk);
  }
  innenbeleuchtung_status_alt = ~innenbeleuchtung_status;
#ifdef _DEBUG_INNEN_
  Serial.print(" 0b");
  Serial.println(innenbeleuchtung_status, BIN);
#endif
}

/*
 NMRA DCC related 
*/
void notifyCVResetFactoryDefault() {
  // Make FactoryDefaultCVIndex non-zero and equal to num CV's to be reset
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);
};

void setFactoryDefaults() {
  if (Dcc.isSetCVReady()) {
    FactoryDefaultCVIndex--;  // Decrement first as initially it is the size of the array
    Dcc.setCV(FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
  }
}

// This function is callicht_innen whenever a normal DCC Turnout Packet is received and we're in Output Addressing Mode
void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t R, uint8_t D) {
  Addr &= 0x7ff;
  uint16_t relAddr = Addr - baseAddr;  // relative address starting with 0
#ifdef _TEST_
  Serial.print("Addr ");
  Serial.print(Addr);
  Serial.print(", R=");
  Serial.print(R);
  Serial.print(", D=");
  Serial.println(D);
#endif
  switch (relAddr) {
    case 0:
      if (licht) {
        for (int i = 0; i < num_aussenlichter; i++) {
          set_aussenlicht(R, i);
        }
      }
      break;
    case 1:
      innenbeleuchtung();
      break;
    case 2:
      licht = R;
      innenbeleuchtung();
      if (licht) {
        for (int i = 0; i < num_aussenlichter; i++) {
          set_aussenlicht(R, i);
        }
      } else {
        for (int i = 0; i < num_aussenlichter; i++) {
          digitalWrite(AUSSENLICHT_PINS[i], i != TAGESLICHT);
        }
      }
      break;
  }
}

void read_cvs() {
  pwm_aussenlichter[TAGESLICHT] = constrain(Dcc.getCV(CV_PWM_TAGESLICHT), 0, 255);
  pwm_aussenlichter[KANDELABER] = constrain(Dcc.getCV(CV_PWM_KANDELABER), 0, 255);
  pwm_aussenlichter[GUETERSCHUPPEN_TREPPE] = constrain(Dcc.getCV(CV_PWM_GUETERSCHUPPEN_TREPPE), 0, 255);
  pwm_aussenlichter[TANKSTELLE] = constrain(Dcc.getCV(CV_PWM_TANKSTELLE), 0, 255);
  pwm_aussenlichter[GLEISFELD] = constrain(Dcc.getCV(CV_PWM_GLEISFELD), 0, 255);
  neopixel_color_r = constrain(Dcc.getCV(CV_COLOR_INNENLICHT_R), 0, 255);
  neopixel_color_g = constrain(Dcc.getCV(CV_COLOR_INNENLICHT_G), 0, 255);
  neopixel_color_b = constrain(Dcc.getCV(CV_COLOR_INNENLICHT_B), 0, 255);
}


#ifdef _TEST_
uint32_t test_timer = 0UL;
int cycle = 0;
int r = 1;
#endif

void setup() {
  srandom(time(NULL));
  Serial.begin(115200);
  licht_innen.begin();


  for (int i = 0; i < num_aussenlichter; i++) {
    pinMode(AUSSENLICHT_PINS[i], OUTPUT);
    digitalWrite(AUSSENLICHT_PINS[i], !(i == TAGESLICHT));  // die Ausgänge der Einzel-LED sind LOW-aktiv, ca. 1.5 mA je LED, der Streifen ist HIGH-aktiv, ca. 90 mA
  }
  // Configure the DCC CV Programing ACK pin for an output
  pinMode(DCC_PIN, INPUT);
  Dcc.pin(0, DCC_PIN, 0);

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.initAccessoryDecoder(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE | CV29_EXT_ADDRESSING | FLAGS_AUTO_FACTORY_DEFAULT, 0);
  baseAddr = Dcc.getAddr();
  read_cvs();

  delay(500);  // 0.3 Sekunden warten auf Einpendeln
  licht = 0;
  innenbeleuchtung();
#ifdef _TEST_
  test_timer = millis() + 3000;
#endif
#ifdef _DEBUG_INNEN_
  Serial.print("Color( R=");
  Serial.print(neopixel_color_r, DEC);
  Serial.print(", G=");
  Serial.print(neopixel_color_g, DEC);
  Serial.print(", B=");
  Serial.print(neopixel_color_b, DEC);
  Serial.println(" )");
#endif
}

void loop() {
#ifdef _TEST_
  if (millis() > test_timer) {
    test_timer = millis() + 3000;
    notifyDccAccTurnoutOutput(cycle + 1, r, 1);
    Serial.println((!licht ? "Tag" : "Nacht"));
    (++r) %= 2;
    cycle += r;
    cycle %= 3;
  }
#else
  Dcc.process();
#endif
  if (FactoryDefaultCVIndex) {
    setFactoryDefaults();
  }
  innenbeleuchtung_steuern();
}
