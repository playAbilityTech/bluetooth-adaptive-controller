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

  const char* ssid = "Bla";
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

    #ifdef USE_GAMEPAD
    usb_hid.begin();

    // wait until device mounted
    while( !TinyUSBDevice.mounted() ) delay(1);
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
}