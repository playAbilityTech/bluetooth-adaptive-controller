#include "Adafruit_TinyUSB.h"

// HID report descriptor using TinyUSB's template
// Single Report (no ID) descriptor
uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_GAMEPAD()
};

// USB HID object. For ESP32 these values cannot be changed after this declaration
// desc report, desc len, protocol, interval, use out endpoint
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 1, false);

// Report payload defined in src/class/hid/hid.h
// - For Gamepad Button Bit Mask see  hid_gamepad_button_bm_t
// - For Gamepad Hat    Bit Mask see  hid_gamepad_hat_t
hid_gamepad_report_t    gp;

  typedef struct { 
  int8_t  x;        ///< Delta x  movement of left analog-stick
  int8_t  y;         ///< Delta y  movement of left analog-stick
  int8_t  z;         ///< Delta z  movement of right analog-joystick
  int8_t  rz;        ///< Delta Rz movement of right analog-joystick
  int8_t  rx;        ///< Delta Rx movement of analog left trigger
  int8_t  ry;        ///< Delta Ry movement of analog right trigger
  uint8_t hat;       ///< Buttons mask for currently pressed buttons in the DPad/hat
  uint32_t buttons;  ///< Buttons mask for currently pressed buttons
} t_gamepad;



#include "pin_config.h"

#include <NimBLEDevice.h>

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLEServer* pServer = NULL;
NimBLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

//BLE
 class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(NimBLEServer* pServer) {
      deviceConnected = false;
    }
  };

  class MyCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++)
        Serial.print(rxValue[i]);
      Serial.println();

      if (rxValue.length() > 0 && rxValue[0] == '!') {
        switch (rxValue[1]) {
          case 'A':
            {
              char* token;
              const char s[2] = ";";
              int8_t x = -1;
              int8_t y = -1;
              char* data = (char*)rxValue.c_str();
              token = strtok(data+2, s);
              if (token) {
                x = atoi(token);
                token = strtok(NULL, s);
                if (token) {
                  y = atoi(token);
                      gp.x = x;
                      gp.y = y;
                      usb_hid.sendReport(0, &gp, sizeof(gp));
                    Serial.printf("x=%d, y=%d\n", x, y);
                    char str[100];
                    memset(str, 0, 100);
                    sprintf(str, "x=%d, y=%d\r\n", x, y);
                    pTxCharacteristic->setValue((uint8_t*)str, strlen(str));
                    pTxCharacteristic->notify();
                }
              }
            }
            break;
          case 'Z':
            {
              uint8_t *ptr = (uint8_t*)rxValue.c_str();
              t_gamepad* gp = (t_gamepad*)(ptr + 2);
              usb_hid.sendReport(0, &gp, sizeof(gp));
             
            }
            break;
          default:
            break;
          }
        rxValue.append("\n");
        pTxCharacteristic->setValue((uint8_t*)rxValue.c_str(), strlen(rxValue.c_str()));
        pTxCharacteristic->notify();
      }
    }
  };

//Tooling

void inputTest(){
   if ( !usb_hid.ready() ) return;
   //Joystick 2 UP
   Serial.println("Joystick 2 UP");
   gp.z  = 0;
   gp.rz = 127;
   usb_hid.sendReport(0, &gp, sizeof(gp));
   delay(500);

   // Joystick 2 DOWN
   Serial.println("Joystick 2 DOWN");
   gp.z  = 0;
   gp.rz = -127;
   usb_hid.sendReport(0, &gp, sizeof(gp));
   delay(500);
}
  
void setupBLE() {
    Serial.println("Start BLE");
    // Create the BLE Device
    NimBLEDevice::init("playAbility");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

    // Create the BLE Server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX,
      NIMBLE_PROPERTY::NOTIFY
    );

    BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX,
      NIMBLE_PROPERTY::WRITE
    );

    pRxCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    // pAdvertising->setAppearance(0x7<<6); // glasses
    pAdvertising->setAppearance(0x01F << 6 | 0x06); // LEDs 

    // Start advertising
    pServer->getAdvertising()->start();
    Serial.println("Waiting a client connection to notify...");
}

void setup() {
  
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);

  usb_hid.begin();
  while( !TinyUSBDevice.mounted() ) delay(1);

  setupBLE();
}

void loop() {

  if (digitalRead(0) == 0) {
    ESP.restart();
  }
  //inputTest();
  
}

