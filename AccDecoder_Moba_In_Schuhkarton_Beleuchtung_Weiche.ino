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
 Güterschuppen-Treppe | 11 5       12 18 | ---| 4k7 |--- Servo
           Kandelaber | 12 6       11 17 | Gleisfeld
                      | 13         10 16 | Tankstelle
Neopixel --| 470 |--- | 14 8        9 15 | IRLZ34 G Tageslicht-LED-Streifen
                      +------------------+
const uint8_t AUSSENLICHT_PINS[] = { 9, 6, 5, 10, 11 };  // Reihenfolge: Tageslicht, Kandelaber, Güterschuppen-Treppe, Tankstelle, Gleisfeld


   CV-Plan:
   CV60: LSB minimale Pulsdauer für Servo
   CV61: MSB minimale Pulsdauer für Servo
   CV62: LSB maximale Pulsdauer für Servo
   CV63: MSB maximale Pulsdauer für Servo
   CV64: Servo-Speed (Schrittweite * DEADBAND µs)
   CV65: PWM für "Tageslicht"
   CV66: für Brückenkandelaber
   CV67: PWM für Laterne über der Tür am Schuppen
   CV68: PWM für Tankstelle
   CV69: PWM für Gleisfeld-Leuchten
   CV70: Neopixel-Farbe, R
   CV71: Neopixel-Farbe, G
   CV72: Neopixel-Farbe, B
*/
// #define _TEST_
// #define _DEBUG_INNEN_
// #define _DEBUG_AUSSEN_
// #define _DEBUG_SERVO_

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
  CV_MINPULSE_LSB = 60,
  CV_MINPULSE_MSB,
  CV_MAXPULSE_LSB,
  CV_MAXPULSE_MSB,
  CV_SPEED,
  CV_PWM_TAGESLICHT,
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
uint8_t pwm_aussenlichter[num_aussenlichter] = { 64, 128, 128, 128, 128 };

const uint32_t SERVO_PERIOD = 20000U;             // µs 1/50 Hz
const uint32_t ABS_MIN_PULSE_WIDTH = 1000UL;      // µs
const uint32_t ABS_MAX_PULSE_WIDTH = 2000UL;      // µs
const uint32_t DEFAULT_MIN_PULSE_WIDTH = 1200UL;  // µs  beim Testen ermittelt
const uint32_t DEFAULT_MAX_PULSE_WIDTH = 1800UL;  // µs  beim Testen ermittelt
const unsigned int SERVO_DEADBAND = 5;            // Deadband µs (MG996R)
uint8_t SERVO_SPEED = 20;                         // Multiplikator für Schritte (Schrittweite = DEADBAND * SERVO_SPEED)
const uint32_t DETACH_TIME = 20000;               // 20 Sek. nach Stillstand wird der Servo deaktiviert
const uint8_t SERVO_PIN = 12;


const int LEDS_INNENLICHT = 16;


Adafruit_NeoPixel licht_innen = Adafruit_NeoPixel(LEDS_INNENLICHT, NEOPIXEL_PIN_INNEN, NEO_GRB + NEO_KHZ800);

// Lichter bezogen
uint32_t pwm_timer = 0UL;
uint32_t innenlicht_refresh_timer = 0UL;
uint16_t innenbeleuchtung_status = 0;
uint16_t innenbeleuchtung_status_alt = ~0;
uint8_t neopixel_color_r = 24;
uint8_t neopixel_color_g = 12;
uint8_t neopixel_color_b = 6;
uint8_t tageslicht_pwm = 0;
bool tag = true;
uint8_t tageslicht = 0;
uint32_t tag_timer = 0UL;
uint32_t aussenlicht_timer = 0UL;
uint8_t lichter_id = 0;
const uint32_t WAIT_PWM_CHG = 80;
const uint32_t AUSSENLICHT_DELAY = 333;

// Servo bezogen
uint32_t servo_min_pulse_width = DEFAULT_MIN_PULSE_WIDTH;
uint32_t servo_max_pulse_width = DEFAULT_MAX_PULSE_WIDTH;
uint32_t servo_attached = false;  // zum Beenden der Pulse
uint32_t servo_detach_timer = 0UL;
uint32_t servo_timer = 0UL;
uint8_t servo_speed = SERVO_SPEED;
unsigned long current_servo_pulse_width = 0;  // Current servo pulse width
unsigned long target_servo_pulse_width = 0;   // Target Servo pulse width

uint16_t baseAddr = DEFAULT_ACCESSORY_DECODER_ADDRESS + 100;

CVPair FactoryDefaultCVs[] = {
  { CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_ACCESSORY_DECODER_ADDRESS + 100 },
  { CV_ACCESSORY_DECODER_ADDRESS_MSB, 0 },
  { CV_MINPULSE_LSB, CALC_U8_LSB_VALUE(DEFAULT_MIN_PULSE_WIDTH) },
  { CV_MINPULSE_MSB, CALC_U8_MSB_VALUE(DEFAULT_MIN_PULSE_WIDTH) },
  { CV_MAXPULSE_LSB, CALC_U8_LSB_VALUE(DEFAULT_MAX_PULSE_WIDTH) },
  { CV_MAXPULSE_MSB, CALC_U8_MSB_VALUE(DEFAULT_MAX_PULSE_WIDTH) },
  { CV_SPEED, SERVO_SPEED },
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
    innenbeleuchtung_status_alt = innenbeleuchtung_status;
    licht_innen.show();
  }
}

void lichter_steuern(bool ausschalten = false) {
  if (millis() >= aussenlicht_timer && aussenlicht_timer != 0UL) {
    if (lichter_id < num_aussenlichter) {
      if (lichter_id != TAGESLICHT) {
        aussenlicht_timer = millis() + AUSSENLICHT_DELAY;
        uint8_t pwm = tag ? 0 : pwm_aussenlichter[lichter_id];
        if (ausschalten) {
          pwm = 0;
        }
        analogWrite(AUSSENLICHT_PINS[lichter_id], 255 - pwm);  // Low-Aktiv
#ifdef _DEBUG_AUSSEN_
        Serial.print(" Licht Pin: ");
        Serial.print(AUSSENLICHT_PINS[lichter_id]);
        Serial.print(" soll-PWM ");
        Serial.println(pwm);
#endif
      }
      lichter_id++;
    } else {
      aussenlicht_timer = 0UL;
    }
  }
}

void aussenbeleuchtung_steuern() {
  uint8_t pwm = tageslicht_pwm;
  bool erledigt = false;
  if (!tageslicht) {
    analogWrite(AUSSENLICHT_PINS[TAGESLICHT], 0);
    lichter_steuern();
    return;
  }
  if (lichter_id < num_aussenlichter || (millis() >= tag_timer && tag_timer != 0UL)) {
#ifdef _DEBUG_AUSSEN_
    Serial.print(tag_timer);
    Serial.print(" <--> ");
    Serial.print(millis());
    Serial.print(" gelesen an Pin ");
    Serial.print(AUSSENLICHT_PINS[TAGESLICHT]);
    Serial.print(": ");
    Serial.println(pwm);
#endif
    tag_timer = millis() + WAIT_PWM_CHG;
    if (tag) {
      if (pwm < pwm_aussenlichter[TAGESLICHT]) {
        pwm = constrain(pwm + 1, 0, pwm_aussenlichter[TAGESLICHT]);
        if (pwm > (pwm_aussenlichter[TAGESLICHT] / 2) && !erledigt) {
          erledigt = true;
          aussenlicht_timer = millis();
          lichter_id = 0;
        }
      }
    } else {
      if (pwm > 0) {
        pwm = constrain(pwm - 1, 0, pwm_aussenlichter[TAGESLICHT]);
        if (pwm < (pwm_aussenlichter[TAGESLICHT] / 2) && !erledigt) {
          erledigt = true;
          aussenlicht_timer = millis();
          lichter_id = 0;
        }
      }
    }
    analogWrite(AUSSENLICHT_PINS[TAGESLICHT], pwm);
    tageslicht_pwm = pwm;
    if (aussenlicht_timer == 0L && (pwm == 0 || pwm == pwm_aussenlichter[TAGESLICHT])) {
      tag_timer = 0UL;
    }
    lichter_steuern();
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

void innenbeleuchtung() {
  uint16_t immer_an = 0b1100000000000000;
  uint16_t immer_an_msk = 0b1100011000000110;
  uint16_t alles_aus = 0b0000000000000000;
  if (!tageslicht) {
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

void aussenbeleuchtung() {
  tag = !tag;
  tag_timer = millis();
  lichter_id = 0;
}

// Weiche
void servo_steuern() {
  unsigned long stop;
  long step = SERVO_DEADBAND * servo_speed;
  if (servo_attached) {
    if (micros() >= servo_timer) {
#ifdef _DEBUG_SERVO_
      Serial.print("Servo Pos: ");
      Serial.print(current_servo_pulse_width);
      Serial.print(" µs, Schrittweite: ");
      Serial.print(step);
      Serial.println(" µs");
#endif
      if (target_servo_pulse_width != current_servo_pulse_width) {
        servo_detach_timer = millis() + DETACH_TIME;
        if (target_servo_pulse_width > current_servo_pulse_width) {
          current_servo_pulse_width += step;
        } else if (target_servo_pulse_width < current_servo_pulse_width) {
          current_servo_pulse_width -= step;
        }
        current_servo_pulse_width = constrain(current_servo_pulse_width, servo_min_pulse_width, servo_max_pulse_width);
        if (abs(target_servo_pulse_width - current_servo_pulse_width) < (SERVO_DEADBAND << 2)) {  //Bewegung < Deadband vermeiden
          current_servo_pulse_width = target_servo_pulse_width;
        }
      }
      servo_timer = micros() + SERVO_PERIOD;
      digitalWrite(SERVO_PIN, HIGH);
      stop = micros() + current_servo_pulse_width - 2;  // 2 cycles for loop
      while (stop - 1 > micros()) {
        ;
      }
    }
  }
  digitalWrite(SERVO_PIN, LOW);
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

// void notifyDccMsg(DCC_MSG * msg) {
//   Serial.print(msg->PreambleBits);
//   Serial.print(" ");
//   Serial.print(msg->Size);
//   Serial.print(" ");
//   for(int i = 0; i<sizeof(msg->Data); i++) {
//     Serial.print(msg->Data[i], BIN);
//   }
//   Serial.println();
// }

// This function is called whenever a normal DCC Turnout Packet is received and we're in Output Addressing Mode
void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t R, uint8_t D) {
  Addr &= 0x7ff;
  uint16_t relAddr = Addr - baseAddr;  // relative address starting with 0
  Serial.print(Addr);
  Serial.print(" ");
  Serial.println(relAddr);
  switch (relAddr) {
    case 0:
      tageslicht = 1;  // Aussenlicht an
      aussenbeleuchtung();
      break;
    case 1:
      innenbeleuchtung();  // Innenlicht wechseln
      break;
    case 2:
      tageslicht = 0;  // alles aus
      tag = 0;
      delay(250);
      innenbeleuchtung();
      break;
    case 3:  // Weiche
      target_servo_pulse_width = R == 0 ? servo_min_pulse_width : servo_max_pulse_width;
      servo_timer = micros();
      servo_attached = true;
      servo_detach_timer = 0UL;
      break;
  }
}

void read_cvs() {
  servo_min_pulse_width = constrain(CALC_U32_CV_VALUE(CV_MINPULSE_LSB, CV_MINPULSE_MSB), ABS_MIN_PULSE_WIDTH, ABS_MAX_PULSE_WIDTH);
  servo_max_pulse_width = constrain(CALC_U32_CV_VALUE(CV_MAXPULSE_LSB, CV_MAXPULSE_MSB), ABS_MIN_PULSE_WIDTH, ABS_MAX_PULSE_WIDTH);
  servo_speed = constrain(Dcc.getCV(CV_SPEED), 1, 10);
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



  delay(300);  // 0.3 Sekunden warten auf Einpendeln
  tageslicht_pwm = tag ? 0 : pwm_aussenlichter[TAGESLICHT];
  innenbeleuchtung();
  tag_timer = millis();

  target_servo_pulse_width = (servo_min_pulse_width + servo_max_pulse_width) / 2;
  servo_timer = 0UL;
  servo_attached = true;
  servo_detach_timer = 0UL;
#ifdef _TEST_
  test_timer = millis() + 10000;
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
#ifdef _DEBUG_SERVO_
  Serial.print("Servo gerade: ");
  Serial.print(servo_min_pulse_width, DEC);
  Serial.print(" µs, abzweigend: ");
  Serial.print(servo_max_pulse_width, DEC);
  Serial.println(" µs");
#endif
}

void loop() {
#ifdef _TEST_
  if (millis() > test_timer) {
    test_timer = millis() + 10000;
    tag = !tag;
    tag_timer = millis();
    innenbeleuchtung();
    target_servo_pulse_width = target_servo_pulse_width == servo_min_pulse_width ? servo_max_pulse_width : servo_min_pulse_width;
    servo_timer = 0UL;
    servo_detach_timer = 0UL;
    servo_attached = true;
  }
#else
  Dcc.process();
#endif
  if (FactoryDefaultCVIndex) {
    setFactoryDefaults();
  }
  innenbeleuchtung_steuern();
  aussenbeleuchtung_steuern();
  servo_steuern();
  // if (current_servo_pulse_width != target_servo_pulse_width) {
  //     servo_detach_timer = millis() + DETACH_TIME;
  // }
  // if (servo_detach_timer <= millis()) {
  //   servo_attached = false;
  // }
}
