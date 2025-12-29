#include "BLEDevice.h"
#include <HardwareSerial.h>
#include "ble_characteristics.h"

#define _DEBUG_

/*
# ESP32-C3 (7 Pins) super mini vs XIAO
#    +---+---+---+                 +---+---+---+
#  2 |   |   |   | 5V            5 |   |   |   | 5V
#  3 |   +---+   | GND           6 |   +---+   | GND
#  4 |           | 3.3V          7 |           | 3.3V
#  5 |           | 10            8 |           | 4
#  6 | XIAO ESP  | 9             9 | ESP super | 3
#  7 |           | 8            10 |   mini    | 2
# 21 |TX       RX| 20           20 |RX       TX| 1
#    +-----------+              21 |           | 0
#                                  +-----------+

# XIAO RP2040
#    +---+---+---+
# 26 |   |   |   | 5V
# 27 |   +---+   | GND
# 28 |           | 3.3V
# 29 |           | 3
#  6 |   XIAO    | 4
#  7 |  RP 2040  | 2
#  0 |TX       RX| 1
#    +-----------+

*/
/* Specify the Service UUID of Server */
static BLEUUID serviceUUID(SERVICE_UUID);
/* Specify the Characteristic UUID of Server */
static BLEUUID charUUID(CHARACTERISTIC_UUID);

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

static uint8_t UART_CH = 1;
static uint8_t TX_PIN = 1;   //-->(9 an RPi RP2040, 1 an XIAO)
static uint8_t RX_PIN = 20;  //-->(8 an RPi RP2040, 0 an XIAO)
static uint8_t CONNECTED_PIN = 6;
static uint8_t LED_PIN = 5;
static uint8_t RESET_PIN = 4;

uint32_t restart_button_begin = 0UL;
bool restart_button_state;

String in_value = "", out_value;

HardwareSerial Serial2(UART_CH);

static char LAYOUT_NAME[] = "Mini-Rangierpuzzle";

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
#ifdef _DEBUG_
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.print("data: ");
  Serial.println((char*)pData);
#endif
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    doConnect = true;
    doScan = true;
    digitalWrite(CONNECTED_PIN, LOW);
    digitalWrite(LED_PIN, HIGH);
#ifdef _DEBUG_
    Serial.println("onDisconnect");
#endif
  }
};

/* Start connection to the BLE Server */
bool connectToServer() {
#ifdef _DEBUG_
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());
#endif

  BLEClient* pClient = BLEDevice::createClient();
#ifdef _DEBUG_
  Serial.println(" - Created client");
#endif

  pClient->setClientCallbacks(new MyClientCallback());

  /* Connect to the remote BLE Server */
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
#ifdef _DEBUG_
  Serial.println(" - Connected to server");
#endif
  /* Obtain a reference to the service we are after in the remote BLE server */
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
#ifdef _DEBUG_
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
#endif
    pClient->disconnect();
    return false;
  }
#ifdef _DEBUG_
  Serial.println(" - Found our service");
#endif

  /* Obtain a reference to the characteristic in the service of the remote BLE server */
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
#ifdef _DEBUG_
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
#endif
    pClient->disconnect();
    return false;
  }
#ifdef _DEBUG_
  Serial.println(" - Found our characteristic");
#endif
  /* Read the value of the characteristic */
  /* Initial value is 'Hello, World!' */
  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
#ifdef _DEBUG_
    Serial.print("The characteristic value was: ");
    Serial.println(value.c_str());
#endif
  }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }

  connected = true;
  return true;
}
/* Scan for BLE servers and find the first one that advertises the service we are looking for. */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  /* Called for each advertising BLE server. */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
#ifdef _DEBUG_
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
#endif

    /* We have found a device, let us now see if it contains the service we are looking for. */
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = false;
    }
  }
};


void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, TX_PIN, RX_PIN);

  pinMode(CONNECTED_PIN, OUTPUT);
  digitalWrite(CONNECTED_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(RESET_PIN, INPUT_PULLUP);
#ifdef _DEBUG_
  Serial.println("Starting Arduino BLE Client application...");
#endif
  BLEDevice::init("ESP32-BLE-Client");

  /* Retrieve a Scanner and set the callback we want to use to be informed when we
     have detected a new device.  Specify that we want active scanning and start the
     scan to run for 5 seconds. */
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(10, false);
}

void loop() {
  /* If the flag "doConnect" is true, then we have scanned for and found the desired
     BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
     connected we set the connected flag to be true. */
  restart_button_state = digitalRead(RESET_PIN);
  if (!restart_button_state) {
      if(restart_button_begin == 0UL) {
        restart_button_begin = millis() + 3000;
        delay(100);  // Debounce
      }
      if(restart_button_begin <= millis()) {
        esp_restart();
      }
  } else {
    restart_button_begin = 0UL;
  }
  if (doConnect == true) {
    if (connectToServer()) {
#ifdef _DEBUG_
      Serial.println("We are now connected to the BLE Server.");
#endif
      digitalWrite(LED_PIN, LOW);
      digitalWrite(CONNECTED_PIN, HIGH);
    } else {
#ifdef _DEBUG_
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
#endif
      esp_restart();
    }
    doConnect = false;
    doScan = false;
  }

  /* If we are connected to a peer BLE Server, update the characteristic each time we are reached
     with the current time since boot */
  if (connected) {
    in_value = "";
    while (in_value.substring(0, 3) != "<<<" && in_value.substring(-3, 3) != ">>>") {
      in_value = pRemoteCharacteristic->readValue();
    }
    if (in_value != ">>>OK<<<") {
#ifdef _DEBUG_
      Serial.println("The command (characteristic) is: " + in_value);
#endif
      Serial2.print(in_value.c_str());
      if (in_value == "<<<L?>>>") {
        /* Loco scan requested */
        delay(1000);
        out_value = "";
        while (!Serial2.available()) {
          ;
        }
        while (Serial2.available()) {
          out_value = Serial2.readString();
        }
#ifdef _DEBUG_
        Serial.println("The response is: " + out_value);
#endif
        /* Set the characteristic's value to be the array of bytes that is actually a string */
        pRemoteCharacteristic->writeValue(out_value.c_str(), out_value.length());
      } else {
        out_value = ">>>OK<<<";
#ifdef _DEBUG_
        Serial.println("The response (new characteristic) is: " + out_value);
#endif
        /* Set the characteristic's value to be the array of bytes that is actually a string */
        pRemoteCharacteristic->writeValue(out_value.c_str(), out_value.length());
      }
      /* You can see this value updated in the Server's Characteristic */
    }
  } else {
    if (doScan) {
      BLEDevice::getScan()->start(10, false);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    }
  }

  delay(50); /* Delay a second between loops */
}