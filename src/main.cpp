#include <Arduino.h>
#include "Adafruit_TinyUSB.h"


#include "pin_config.h"

#include "TFT_eSPI.h" // https://github.com/Bodmer/TFT_eSPI
TFT_eSPI tft = TFT_eSPI();



// HID report descriptor using TinyUSB's template
// Single Report (no ID) descriptor
uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_GAMEPAD()
};

// USB HID object. For ESP32 these values cannot be changed after this declaration
// desc report, desc len, protocol, interval, use out endpoint
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);

// Report payload defined in src/class/hid/hid.h
// - For Gamepad Button Bit Mask see  hid_gamepad_button_bm_t
// - For Gamepad Hat    Bit Mask see  hid_gamepad_hat_t
hid_gamepad_report_t    gp;

#define USE_GAMEPAD
#define USE_WS


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

  const char HID_SERVICE[] = "1812";
  const char HID_INFORMATION[] = "2A4A";
  const char HID_REPORT_MAP[] = "2A4B";
  const char HID_CONTROL_POINT[] = "2A4C";
  const char HID_REPORT_DATA[] = "2A4D";
  void scanEndedCB(NimBLEScanResults results);

  static NimBLEAdvertisedDevice* advDevice;

  static bool doConnect = false;
  static uint32_t scanTime = 0; /** 0 = scan forever */





/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks: public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) {
    Serial.println("Connected");
    tft.println("Connected");
    /** After connection we should change the parameters if we don't need fast response times.
     *  These settings are 150ms interval, 0 latency, 450ms timout.
     *  Timeout should be a multiple of the interval, minimum is 100ms.
     *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
     *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
     */
    pClient->updateConnParams(120, 120, 0, 60);
  };

  void onDisconnect(NimBLEClient* pClient) {
    Serial.print(pClient->getPeerAddress().toString().c_str());
    tft.print(pClient->getPeerAddress().toString().c_str());

    Serial.println(" Disconnected - Starting scan");
    tft.println(" Disconnected - Starting scan");
    NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
  };

  /** Called when the peripheral requests a change to the connection parameters.
   *  Return true to accept and apply them or false to reject and keep
   *  the currently used parameters. Default will return true.
   */
  bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
    // Failing to accepts parameters may result in the remote device
    // disconnecting.
    return true;
  };

  /********************* Security handled here **********************
   ****** Note: these are the same return values as defaults ********/
  uint32_t onPassKeyRequest() {
    Serial.println("Client Passkey Request");
    tft.println("Client Passkey Request");
    /** return the passkey to send to the server */
    return 123456;
  };

  bool onConfirmPIN(uint32_t pass_key) {
    Serial.print("The passkey YES/NO number: ");
    Serial.println(pass_key);

    tft.print("The passkey YES/NO number: ");
    tft.println(pass_key);
    /** Return false if passkeys don't match. */
    return true;
  };

  /** Pairing process complete, we can check the results in ble_gap_conn_desc */
  void onAuthenticationComplete(ble_gap_conn_desc* desc) {
    if (!desc->sec_state.encrypted) {
      Serial.println("Encrypt connection failed - disconnecting");
      tft.println("Encrypt connection failed - disconnecting");
      /** Find the client with the connection handle provided in desc */
      NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
      return;
    }
  };
};

/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    Serial.println(advertisedDevice->toString().c_str());
    if ((advertisedDevice->getAdvType() == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_HD)
      || (advertisedDevice->getAdvType() == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_LD)
      || (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(NimBLEUUID(HID_SERVICE)))) {
      Serial.print("Advertised HID Device found: ");
      Serial.println(advertisedDevice->toString().c_str());

      tft.print("Advertised HID Device found: ");
      tft.println(advertisedDevice->toString().c_str());

      /** stop scan before connecting */
      NimBLEDevice::getScan()->stop();
      /** Save the device reference in a global for the client to use*/
      advDevice = advertisedDevice;
      /** Ready to connect now */
      doConnect = true;
    }
  };
};


/** Notification / Indication receiving handler callback */
// Notification from 4c:75:25:xx:yy:zz: Service = 0x1812, Characteristic = 0x2a4d, Value = 1,0,0,0,0,
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length == 7) {

    // 16 + 24 + 8 +

    int16_t x = pData[2] << 4 | (pData[3] >> 4);
    x = (x >> 11) == 0 ? x : -1 ^ 0xFFF | x;

    int16_t y = (pData[3] & 0x00ff) << 8 | (pData[4]);
    y = (y >> 11) == 0 ? y : -1 ^ 0xFFF | y;
    // int16_t x = pData[2] | ((pData[3] & 0xf0) << 8);
    int8_t wheel = pData[5];

    // Serial.printf("%02x\n", pData[0]);

    // if (pData[0]&1) {
    //  // Serial.printf("pressButton\n");
    //  Gamepad.pressButton(0);
    // } else {
    //  // Serial.printf("releaseButton\n");
    //  Gamepad.releaseButton(0);
    // }

    #ifdef USE_GAMEPAD
      Gamepad.send(x, y, 0, 0, 0, 0, 0, pData[0] | pData[1] << 8);
    #endif

    // tft.clean()
    tft.fillScreen(TFT_BLACK);
    // digitalWrite(TFT_LEDA_PIN, 0);
    // tft.setTextFont(1);
    tft.setCursor(0,0);
    // tft.setTextColor(TFT_GREEN, TFT_BLACK);

    // .printf("HELLO\n");
    tft.printf("%02x%02x\n", pData[0], pData[1]);

    
    // BLE Trackball Mouse from Amazon returns 6 bytes per HID report
    // hid_mouse_report_t report;
    // memset(&report, 0, sizeof(report));
    // report.buttons = pData[0];
    // report.x = (int8_t) * (int16_t*)&pData[1];
    // report.y = (int8_t) * (int16_t*)&pData[3];
    // report.wheel = (int8_t)pData[5];
    // Mouse.sendReport(&report);
  }
  else if (length == 5) {
    // https://github.com/wakwak-koba/ESP32-NimBLE-Mouse
    // returns 5 bytes per HID report
    // hid_mouse_report_t report;
    // memset(&report, 0, sizeof(report));
    // memcpy(&report, pData, (sizeof(report) < length) ? sizeof(report) : length);
    // Mouse.sendReport(&report);
  }
}

/** Callback to process the results of the last scan or restart it */
void scanEndedCB(NimBLEScanResults results) {
  Serial.println("Scan Ended");
  tft.println("Scan Ended");
}


/** Create a single global instance of the callback class to be used by all clients */
static ClientCallbacks clientCB;


/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer() {
  NimBLEClient* pClient = nullptr;

  /** Check if we have a client we should reuse first **/
  if (NimBLEDevice::getClientListSize()) {
    /** Special case when we already know this device, we send false as the
    *  second argument in connect() to prevent refreshing the service database.
    *  This saves considerable time and power.
    */
    pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
    if (pClient) {
      if (!pClient->connect(advDevice, false)) {
        Serial.println("Reconnect failed");
        return false;
      }
      Serial.println("Reconnected client");
    }
    /** We don't already have a client that knows this device,
     *  we will check for a client that is disconnected that we can use.
     */
    else {
      pClient = NimBLEDevice::getDisconnectedClient();
    }
  }

  /** No client to reuse? Create a new one. */
  if (!pClient) {
    if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
      Serial.println("Max clients reached - no more connections available");
      return false;
    }

    pClient = NimBLEDevice::createClient();

    Serial.println("New client created");

    pClient->setClientCallbacks(&clientCB, false);
    /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
     *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
     *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
     *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
     */
    pClient->setConnectionParams(12, 12, 0, 51);
    /** Set how long we are willing to wait for the connection to complete (seconds), default is 30. */
    pClient->setConnectTimeout(5);


    if (!pClient->connect(advDevice)) {
      /** Created a client but failed to connect, don't need to keep it as it has no data */
      NimBLEDevice::deleteClient(pClient);
      Serial.println("Failed to connect, deleted client");
      return false;
    }
  }

  if (!pClient->isConnected()) {
    if (!pClient->connect(advDevice)) {
      Serial.println("Failed to connect");
      return false;
    }
  }

  Serial.print("Connected to: ");
  Serial.println(pClient->getPeerAddress().toString().c_str());
  Serial.print("RSSI: ");
  Serial.println(pClient->getRssi());

  tft.print("Connected to: ");
  tft.println(pClient->getPeerAddress().toString().c_str());
  tft.print("RSSI: ");
  tft.println(pClient->getRssi());

  /** Now we can read/write/subscribe the charateristics of the services we are interested in */
  NimBLERemoteService* pSvc = nullptr;
  NimBLERemoteCharacteristic* pChr = nullptr;
  NimBLERemoteDescriptor* pDsc = nullptr;

  pSvc = pClient->getService(HID_SERVICE);
  if (pSvc) {     /** make sure it's not null */
    // This returns the HID report descriptor like this
    // HID_REPORT_MAP 0x2a4b Value: 5,1,9,2,A1,1,9,1,A1,0,5,9,19,1,29,5,15,0,25,1,75,1,
    // Copy and paste the value digits to http://eleccelerator.com/usbdescreqparser/
    // to see the decoded report descriptor.
    // pChr = pSvc->getCharacteristic(HID_REPORT_MAP);
    // if (pChr) {     /** make sure it's not null */
    //  Serial.print("HID_REPORT_MAP ");
    //  tft.print("HID_REPORT_MAP ");
    //  if (pChr->canRead()) {
    //    std::string value = pChr->readValue();
    //    Serial.print(pChr->getUUID().toString().c_str());
    //    Serial.print(" Value: ");
    //    tft.print(pChr->getUUID().toString().c_str());
    //    tft.print(" Value: ");
    //    uint8_t* p = (uint8_t*)value.data();
    //    for (size_t i = 0; i < value.length(); i++) {
    //      Serial.print(p[i], HEX);
    //      Serial.print(',');

    //      tft.print(p[i], HEX);
    //      tft.print(',');
    //    }
    //    Serial.println();
    //    tft.println();
    //  }
    // }
    // else {
    //  Serial.println("HID REPORT MAP char not found.");
    //  tft.println("HID REPORT MAP char not found.");
    // }

    // Subscribe to characteristics HID_REPORT_DATA.
    // One real device reports 2 with the same UUID but
    // different handles. Using getCharacteristic() results
    // in subscribing to only one.
    std::vector<NimBLERemoteCharacteristic*>* charvector;
    charvector = pSvc->getCharacteristics(true);
    for (auto& it : *charvector) {
      if (it->getUUID() == NimBLEUUID(HID_REPORT_DATA)) {
        Serial.println(it->toString().c_str());
        tft.println(it->toString().c_str());
        if (it->canNotify()) {
          if (!it->subscribe(true, notifyCB)) {
            /** Disconnect if subscribe failed */
            Serial.println("subscribe notification failed");
            tft.println("subscribe notification failed");
            pClient->disconnect();
            return false;
          }
        }
      }
    }

  }
  Serial.println("Done with this device!");
  tft.println("Done with this device!");
  return true;
}

#endif


void setup() {
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);

  Serial.begin(115200);
  Serial.println("Start !!!");

  // tft.init();
  // tft.setRotation(1);
  // tft.fillScreen(TFT_BLACK);
  // digitalWrite(TFT_LEDA_PIN, 0);
  // tft.setTextFont(1);
  // tft.setTextColor(TFT_GREEN, TFT_BLACK);

  // tft.printf("HELLO\n");

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
      ESP.restart();
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
    NimBLEDevice::init("");

    /** Set the IO capabilities of the device, each option will trigger a different pairing method.
     *  BLE_HS_IO_KEYBOARD_ONLY    - Passkey pairing
     *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

    /** 2 different ways to set security - both calls achieve the same result.
     *  no bonding, no man in the middle protection, secure connections.
     *
     *  These are the default values, only shown here for demonstration.
     */
    NimBLEDevice::setSecurityAuth(true, false, true);
    //NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

    /** Optional: set the transmit power, default is 3db */
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */

    /** Optional: set any devices you don't want to get advertisments from */
    // NimBLEDevice::addIgnored(NimBLEAddress ("aa:bb:cc:dd:ee:ff"));

    /** create new scan */
    NimBLEScan* pScan = NimBLEDevice::getScan();

    /** create a callback that gets called when advertisers are found */
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());

    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(45);
    pScan->setWindow(15);

    /** Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    pScan->setActiveScan(true);
    /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
     *  Optional callback for when scanning stops.
     */
    pScan->start(scanTime, scanEndedCB);
  #endif
}

void loop() {
  // Serial.println("LED ON");
  // digitalWrite(38, LOW);
  // delay(100);

  // Serial.println("LED OFF");
  // digitalWrite(38, HIGH);
  // delay(100);

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


  if (digitalRead(0) == 0) {
    ESP.restart();
  }

  #ifdef USE_BLE
    if (!doConnect) return;



    doConnect = false;

    /** Found a device we want to connect to, do it now */
    if (connectToServer()) {
      // Serial.println("Success! we should now be getting notifications!");
      digitalWrite(38, LOW);
      // delay(100);
    }
    else {
      Serial.println("Failed to connect, starting scan");
      NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
    }
  #endif
}