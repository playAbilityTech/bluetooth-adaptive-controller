#include <Arduino.h>

#define USE_GAMEPAD
// #define USE_WS
#define USE_BLE


#include "Adafruit_TinyUSB.h"
#ifdef USE_GAMEPAD
  // HID report descriptor using TinyUSB's template
  // Single Report (no ID) descriptor
  uint8_t const desc_hid_report[] ={TUD_HID_REPORT_DESC_GAMEPAD()};
  Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);
#endif 
hid_gamepad_report_t    gp;

#include "pin_config.h"

#include "TFT_eSPI.h" // https://github.com/Bodmer/TFT_eSPI
TFT_eSPI tft = TFT_eSPI();


#ifdef USE_WS
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESPAsyncWebServer.h>
  AsyncWebServer server(80);
  AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

  const char* ssid = "bla";
  const char* password = "blablabla";

  const char* PARAM_MESSAGE = "message";


  void notFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found");
  }

  void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      //client connected
      Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
      client->printf("Hello Client %u :)", client->id());
      client->ping();
    }
    else if (type == WS_EVT_DISCONNECT) {
    //client disconnected
      Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
    }
    else if (type == WS_EVT_ERROR) {
    //error was received from the other end
      Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
    }
    else if (type == WS_EVT_PONG) {
    //pong message was received (in response to a ping request maybe)
      Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char*)data : "");
    }
    else if (type == WS_EVT_DATA) {
    //data packet
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len) {
        //the whole message is in a single frame and we got all of it's data
        Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
        if (info->opcode == WS_TEXT) {
          data[len] = 0;
          Serial.printf("%s\n", (char*)data);
        }
        else {
          for (size_t i = 0; i < info->len; i++) {
            Serial.printf("%02x ", data[i]);
          }
          Serial.printf("\n");
        }
        if (info->opcode == WS_TEXT){
          char* token;
          const char s[2] = ";";
          int8_t x = -1;
          int8_t y = -1;
          token = strtok((char*)data, s);
          if (token) {
            x = atoi(token);
            token = strtok(NULL, s);
            if (token) {
              y = atoi(token);
              #ifdef USE_GAMEPAD
              gp.x = x;
              gp.y = y;
              usb_hid.sendReport(0, &gp, sizeof(gp));
              #endif
              // client->text("I got your text message");
              Serial.printf("x=%d, y=%d\n", x, y);
              client->printf("x=%d, y=%d\n", x, y);
            }
          }
        } else {
          client->binary("I got your binary message");
        }
      }
      else {
      //message is comprised of multiple frames or the frame is split into multiple packets
        if (info->index == 0) {
          if (info->num == 0)
            Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
        }

        Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
        if (info->message_opcode == WS_TEXT) {
          data[len] = 0;
          Serial.printf("%s\n", (char*)data);
        }
        else {
          for (size_t i = 0; i < len; i++) {
            Serial.printf("%02x ", data[i]);
          }
          Serial.printf("\n");
        }

        if ((info->index + len) == info->len) {
          Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
          if (info->final) {
            Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
            if (info->message_opcode == WS_TEXT)
              client->text("I got your text message");
            else
              client->binary("I got your binary message");
          }
        }
      }
    }
  }
#endif

#ifdef USE_BLE
#include <NimBLEDevice.h>

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

  static NimBLEServer* pServer = NULL;
  NimBLECharacteristic* pTxCharacteristic;
  bool deviceConnected = false;
  bool oldDeviceConnected = false;
  uint8_t txValue = 0;

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
          case 'B':
            switch (rxValue[2]) {
              case '1':
                if (rxValue[3] == '1')
                  gp.buttons |= GAMEPAD_BUTTON_0;
                else
                  gp.buttons &= ~(GAMEPAD_BUTTON_0);
                break;
              case '2':
                if (rxValue[3] == '1')
                  gp.buttons |= GAMEPAD_BUTTON_1;
                else
                  gp.buttons &= ~(GAMEPAD_BUTTON_1);
                break;
              case '3':
                if (rxValue[3] == '1')
                  gp.buttons |= GAMEPAD_BUTTON_2;
                else
                  gp.buttons &= ~(GAMEPAD_BUTTON_2);
                break;
              case '4':
                if (rxValue[3] == '1')
                  gp.buttons |= GAMEPAD_BUTTON_3;
                else
                  gp.buttons &= ~(GAMEPAD_BUTTON_3);
                break;
              case '5':
                if (rxValue[3] == '1')
                  gp.hat = GAMEPAD_HAT_UP;
                else
                  gp.hat = GAMEPAD_HAT_CENTERED;
                break;
              case '6':
                if (rxValue[3] == '1')
                  gp.hat = GAMEPAD_HAT_DOWN;
                else
                  gp.hat = GAMEPAD_HAT_CENTERED;
                break;
              case '7':
                if (rxValue[3] == '1')
                  gp.hat = GAMEPAD_HAT_LEFT;
                else
                  gp.hat = GAMEPAD_HAT_CENTERED;
                break;
              case '8':
                if (rxValue[3] == '1')
                  gp.hat = GAMEPAD_HAT_RIGHT;
                else
                  gp.hat = GAMEPAD_HAT_CENTERED;
                break;
              default:
                break;
              }
              #ifdef USE_GAMEPAD
                usb_hid.sendReport(0, &gp, sizeof(gp));
              #endif
            break;
          case 'C':
            // set_all_pixel(rxValue[2], rxValue[3], rxValue[4], 0);
            break;
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
                    #ifdef USE_GAMEPAD
                      gp.x = x;
                      gp.y = y;
                      usb_hid.sendReport(0, &gp, sizeof(gp));
                    #endif
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
          default:
            break;
          }
      }
    }
  };
#endif

void setup() {
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);

  Serial.begin(115200);
  Serial.println("Start !!!");

  #ifdef USE_GAMEPAD
    usb_hid.begin();

    // wait until device mounted
    while( !TinyUSBDevice.mounted() ) delay(1);
  #endif

  #ifdef USE_WS
    Serial.printf("WiFi start!\n");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      delay(2000);
      // ESP.restart();
      // return;
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(200, "text/plain", "Hello, world");
    });

    // Send a GET request to <IP>/get?message=<message>
    server.on("/get", HTTP_GET, [](AsyncWebServerRequest* request) {
      String message;
      if (request->hasParam(PARAM_MESSAGE)) {
        message = request->getParam(PARAM_MESSAGE)->value();
      }
      else {
        message = "No message sent";
      }
      request->send(200, "text/plain", "Hello, GET: " + message);
    });

    server.onNotFound(notFound);

    ws.onEvent(onEvent);
    server.addHandler(&ws);

    server.begin();
  #endif

#ifdef USE_BLE
    Serial.println("Start BLE");
    // Create the BLE Device
    NimBLEDevice::init("Play-Ability");
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
#endif

}

void loop() {
  // Serial.println("LED ON");
  // digitalWrite(38, LOW);
  // delay(100);

  // Serial.println("LED OFF");
  // digitalWrite(38, HIGH);
  // delay(100);

  if (digitalRead(0) == 0) {
    ESP.restart();
  }


  #ifdef USE_GAMEPAD

    if ( !usb_hid.ready() ) return;
    // Joystick 2 UP
    Serial.println("Joystick 2 UP");
    gp.z  = 0;
    gp.rz = 127;
    usb_hid.sendReport(0, &gp, sizeof(gp));
    delay(2000);
    
    // Joystick 2 DOWN
    Serial.println("Joystick 2 DOWN");
    gp.z  = 0;
    gp.rz = -127;
    usb_hid.sendReport(0, &gp, sizeof(gp));
    delay(2000);
  #endif
  delay(100);
}