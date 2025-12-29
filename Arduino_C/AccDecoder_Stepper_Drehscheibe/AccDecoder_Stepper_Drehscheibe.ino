// Include the Arduino Stepper Library
#include <Arduino.h>
#include <Wire.h>
#include <Stepper.h>

// #define __DEBUG__
// #define __RESET__
#define ARDUINO_SAMD_ZERO
#include <NmraDcc.h>

const uint8_t DCC_PIN = 2;
const uint8_t ENABLE = D1;
const uint8_t CW_ENDSTOP = D6;
const uint8_t CCW_ENDSTOP = D5;
const int FAST = 400;
const int SLOW = 10;
// Number of steps per output rotation
const int stepsPerRevolution = 200;
const int STEPPER_FAST_SPEED = 200;  // rpm
const int STEPPER_SLOW_SPEED = 50;   // rpm
int ist_gleis, soll_gleis;


// Create Instance of Stepper library
Stepper turntable_stepper(stepsPerRevolution, D10, D7, D9, D8);

NmraDcc Dcc;

uint16_t baseAddr = 1;

/*
 NMRA DCC related 
*/

// This function is called whenever a normal DCC Turnout Packet is received and we're in Output Addressing Mode
void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t R, uint8_t D) {
	// Addr &= 0x7ff;
	uint16_t relAddr = Addr - baseAddr;  // relative address starting with 0
#ifdef __DEBUG__
	Serial.print("Addr: ");
	Serial.print(Addr);
	Serial.print(",    baseAddr: ");
	Serial.print(baseAddr);
	Serial.print(",   relAddr: ");
	Serial.println(relAddr);
#endif
	if (relAddr == 0) {
		soll_gleis = R == 0 ? 1 : 2;
	}
}

void turn_to_track(int gleis) {
	if (gleis != 1 && gleis != 2) {
		return;
	}
	int direction;
	uint8_t endstop;
	if (gleis == 1) {
		endstop = CCW_ENDSTOP;
		direction = -1;
	} else if (gleis == 2) {
		endstop = CW_ENDSTOP;
		direction = 1;
	}
#ifdef __DEBUG__
	Serial.print("ENABLE ist ");
	Serial.print((digitalRead(ENABLE)==LOW? "frei":"besetzt"));
#endif
	if (digitalRead(ENABLE) == LOW) {
#ifdef __DEBUG__
		Serial.print(",  Drehe zu Gleis ");
		Serial.print(gleis);
#endif
		if (digitalRead(endstop) != LOW) {  // ist am jeweiligen, Anschlag nichts tun
			turntable_stepper.setSpeed(STEPPER_FAST_SPEED);
			digitalWrite(ENABLE, HIGH);
			while (digitalRead(endstop) != LOW) {
				turntable_stepper.step(FAST * direction);
			}
			while (digitalRead(endstop) == LOW) {
				turntable_stepper.step(-SLOW * direction);
			}
			turntable_stepper.setSpeed(STEPPER_SLOW_SPEED);
			while (digitalRead(endstop) != LOW) {
				turntable_stepper.step(SLOW * direction);
			}
			digitalWrite(ENABLE, LOW);
			ist_gleis = gleis;
		}
	}
#ifdef __DEBUG__
	Serial.println();
#endif
}

void homing() {
#ifdef __DEBUG__
	Serial.print("ENABLE ist ");
	Serial.print((digitalRead(ENABLE)==LOW? "frei":"besetzt"));
#endif
	if (digitalRead(ENABLE) == LOW) {
#ifdef __DEBUG__
	Serial.print(",  Home zu Gleis ");
	Serial.print(1);
#endif
		digitalWrite(ENABLE, HIGH);
		delay(1000);
		turntable_stepper.setSpeed(STEPPER_FAST_SPEED);
		if (digitalRead(CW_ENDSTOP) == LOW) {  // ist am rechten Anschlag
			while (digitalRead(CCW_ENDSTOP) == LOW) {
				turntable_stepper.step(FAST);
			}
		}
		while (digitalRead(CCW_ENDSTOP) != LOW) {
			turntable_stepper.step(-FAST);
		}
		while (digitalRead(CCW_ENDSTOP) == LOW) {
			turntable_stepper.step(SLOW);
		}
		turntable_stepper.setSpeed(STEPPER_SLOW_SPEED);
		while (digitalRead(CCW_ENDSTOP) != LOW) {
			turntable_stepper.step(-SLOW);
		}
		digitalWrite(ENABLE, LOW);
		ist_gleis = 1;
	}
#ifdef __DEBUG__
	Serial.println();
#endif
}

void setup() {
	Serial.begin(115200);
	delay(2000);
#ifdef ARDUINO_SAMD_ZERO
	Serial.println("Running on SAMD");
#endif
	pinMode(CW_ENDSTOP, INPUT_PULLUP);
	pinMode(CCW_ENDSTOP, INPUT_PULLUP);
	pinMode(ENABLE, OUTPUT);
	digitalWrite(ENABLE, LOW);
	// set the speed in rpm:
	turntable_stepper.setSpeed(STEPPER_FAST_SPEED);
	// initialize the serial port:
	homing();

#ifdef digitalPinToInterrupt
  Dcc.pin(DCC_PIN, 0);
#else
  Dcc.pin(0, DCC_PIN, 1);
#endif
  Dcc.initAccessoryDecoder(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE | CV29_EXT_ADDRESSING | FLAGS_AUTO_FACTORY_DEFAULT, 0);
	soll_gleis = 1;
}

void loop() {
#ifdef __DEBUG__
#ifdef __TEST__
	soll_gleis = ist_gleis == 1 ? 2 : 1;
	delay(5000);
#endif
#endif
	Dcc.process();
	if (ist_gleis != soll_gleis) {
		turn_to_track(soll_gleis);
	}
}