#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

Adafruit_USBD_HID usb_hid;

hid_gamepad_report_t gp = {};

BLEUart bleuart;

void setup() {
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  Serial.begin(115200);

  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  while (!TinyUSBDevice.mounted()) delay(10);

  memset(&gp, 0, sizeof(gp));
  gp.buttons = 1;
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(100);
  
  gp.buttons = 0;
  usb_hid.sendReport(0, &gp, sizeof(gp));

  Bluefruit.begin();
  Bluefruit.setName("playAbility");

  bleuart.begin();

  startAdv();
}

void startAdv(void) {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {
  #ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
  #endif

  if (!TinyUSBDevice.mounted()) return;

  if (bleuart.available()) {
    uint8_t buf[64];
    int count = bleuart.read(buf, sizeof(buf));

    if (count > 0 && buf[0] == '!') {
      processCommand(buf, count);
      usb_hid.sendReport(0, &gp, sizeof(gp));
    }
  }
}

void processCommand(uint8_t* buf, int count) {
  switch (buf[1]) {
    case 'A':
      if (count >= 4) {
        gp.x = buf[2];
        gp.y = buf[3];
      }
      break;
    case 'Z':
      if (count >= sizeof(hid_gamepad_report_t) + 2) {
        memcpy(&gp, buf + 2, sizeof(hid_gamepad_report_t));
      }
      break;
    default:
      return;
  }

  usb_hid.sendReport(0, &gp, sizeof(gp));
}

