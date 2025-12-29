// ATtiny85 Dekoder für 1 Weiche,
// Basisadresse: 1
//

/*
   Arduino Sparkfun ATtiny85
              +--------------------+                
              |                    |   Pin             
         |-----                [ ] |    5  PB5  
         | [-]    ATtiny85     [ ] |    4  PB4  Weiche
    USB  | [ ]                 [ ] |    3  PB3  
         | [ ]                 [ ] |    2  PB2  DCC Signal Eingang 
         | [+]                 [ ] |    1  PB1  (interne Ready-LED)
         |-----                [ ] |    0  PB0  
              |  [ ] [ ] [ ]    [+]|                
              +--------------------+                
                +5V GND  Vin                        

   CV-Plan:
   CV60: LSB minimale Pulsdauer für Servo
   CV61: MSB minimale Pulsdauer für Servo
   CV62: LSB maximale Pulsdauer für Servo
   CV63: MSB maximale Pulsdauer für Servo
   CV64: Servo-Speed (Schrittweite * DEADBAND µs)
*/
// #define _DEBUG_SERVO_
// #define _TEST_

#include <Arduino.h>
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
};

const uint8_t READY_PIN = 1;  // Digispark eingebaute LED
const uint8_t DCC_PIN = 2;

const uint32_t SERVO_PERIOD = 20000U;             // µs 1/50 Hz
const uint32_t ABS_MIN_PULSE_WIDTH = 1000UL;       // µs
const uint32_t ABS_MAX_PULSE_WIDTH = 2000UL;      // µs
const uint32_t DEFAULT_MIN_PULSE_WIDTH = 1210UL;  // µs  beim Testen ermittelt
const uint32_t DEFAULT_MAX_PULSE_WIDTH = 1790UL;  // µs  beim Testen ermittelt
const unsigned int SERVO_DEADBAND = 5;            // Deadband µs (MG996R)
uint8_t SERVO_SPEED = 2;                          // Multiplicator for steps
const uint32_t DETACH_TIME = 20000;               // 20 Sek. nach Stillstand wird der Servo deaktiviert

const uint8_t SERVO_PIN = 0;

// Servo bezogen
uint32_t servo_min_pulse_width = DEFAULT_MIN_PULSE_WIDTH;
uint32_t servo_max_pulse_width = DEFAULT_MAX_PULSE_WIDTH;
uint32_t servo_attached = false;  // zum Beenden der Pulse
uint32_t servo_detach_timer = 0UL;
uint32_t servo_timer = 0UL;
uint8_t servo_speed = SERVO_SPEED;
unsigned long current_servo_pulse_width = 0;  // Current servo pulse width
unsigned long target_servo_pulse_width = 0;   // Target Servo pulse width

// uint16_t baseAddr = 100;
uint16_t baseAddr = DEFAULT_ACCESSORY_DECODER_ADDRESS;

CVPair FactoryDefaultCVs[] = {
  { CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_ACCESSORY_DECODER_ADDRESS },
  { CV_ACCESSORY_DECODER_ADDRESS_MSB, 0 },
  { CV_MINPULSE_LSB, CALC_U8_LSB_VALUE(DEFAULT_MIN_PULSE_WIDTH) },
  { CV_MINPULSE_MSB, CALC_U8_MSB_VALUE(DEFAULT_MIN_PULSE_WIDTH) },
  { CV_MAXPULSE_LSB, CALC_U8_LSB_VALUE(DEFAULT_MAX_PULSE_WIDTH) },
  { CV_MAXPULSE_MSB, CALC_U8_MSB_VALUE(DEFAULT_MAX_PULSE_WIDTH) },
  { CV_SPEED, SERVO_SPEED },
};

#ifdef _TEST_
uint8_t FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);  // 0
#else
uint8_t FactoryDefaultCVIndex = 0;
#endif 

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
        servo_detach_timer = millis() + +DETACH_TIME;
        if (target_servo_pulse_width > current_servo_pulse_width) {
          current_servo_pulse_width += step;
        } else if (target_servo_pulse_width < current_servo_pulse_width) {
          current_servo_pulse_width -= step;
        }
        if (abs(target_servo_pulse_width - current_servo_pulse_width) < (step << 1)) {  //avoid non-working position
          current_servo_pulse_width = target_servo_pulse_width;
        }
        current_servo_pulse_width = constrain(current_servo_pulse_width, servo_min_pulse_width, servo_max_pulse_width);
        if (current_servo_pulse_width == target_servo_pulse_width && servo_detach_timer == 0UL) {
          servo_detach_timer = millis() + DETACH_TIME;
        }
        servo_timer = micros() + SERVO_PERIOD;
        digitalWrite(SERVO_PIN, HIGH);
        stop = micros() + current_servo_pulse_width - 2;  // 2 cycles for loop
        while (stop - 1 > micros()) {
          ;
        }
      }
      digitalWrite(SERVO_PIN, LOW);
    }
  }
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
  if(relAddr == 1) {
      target_servo_pulse_width = R == 0 ? servo_min_pulse_width : servo_max_pulse_width;
      servo_timer = 0UL;
      servo_attached = true;
      servo_detach_timer = 0UL;
  }
}

void read_cvs() {
  servo_min_pulse_width = constrain(CALC_U32_CV_VALUE(CV_MINPULSE_LSB, CV_MINPULSE_MSB), ABS_MIN_PULSE_WIDTH, ABS_MAX_PULSE_WIDTH);
  servo_max_pulse_width = constrain(CALC_U32_CV_VALUE(CV_MAXPULSE_LSB, CV_MAXPULSE_MSB), ABS_MIN_PULSE_WIDTH, ABS_MAX_PULSE_WIDTH);
  servo_speed = constrain(Dcc.getCV(CV_SPEED), 1, 10);
}


#ifdef _TEST_
uint32_t test_timer = 0UL;
#endif

void setup() {
  delay(1500);
  Serial.begin(115200);
  pinMode(READY_PIN, OUTPUT);
  digitalWrite(READY_PIN, LOW);
  delay(50);
  digitalWrite(READY_PIN, HIGH);


  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);

  // Configure the DCC CV Programing ACK pin for an output
  pinMode(DCC_PIN, INPUT);
  Dcc.pin(0, DCC_PIN, 0);

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.initAccessoryDecoder(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE | CV29_EXT_ADDRESSING | FLAGS_AUTO_FACTORY_DEFAULT, 0);
  baseAddr = Dcc.getAddr();
  read_cvs();

  delay(500);  // 0.3 Sekunden warten auf Einpendeln
  target_servo_pulse_width = (servo_min_pulse_width + servo_max_pulse_width) / 2;
  servo_timer = 0UL;
  servo_attached = true;
  servo_detach_timer = 0UL;
#ifdef _TEST_
  test_timer = millis() + 30000;
#endif
#ifdef _DEBUG_SERVO_
  Serial.print("Servo gerade: ");
  Serial.print(servo_min_pulse_width, DEC);
  Serial.print(" µs, abzweigend: ");
  Serial.print(servo_max_pulse_width, DEC);
  Serial.println(" µs");
#endif
  digitalWrite(READY_PIN, servo_attached);
}

void loop() {
#ifdef _TEST_
  if (millis() > test_timer) {
    test_timer = millis() + 30000;
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
  servo_steuern();
  if (servo_detach_timer != 0UL && servo_detach_timer <= millis()) {
    servo_attached = false;
  }
  digitalWrite(READY_PIN, servo_attached);
}
