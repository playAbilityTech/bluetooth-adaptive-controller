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

  // Process BLE commands
  if (bleuart.available()) {
    uint8_t buf[64];
    int count = bleuart.read(buf, sizeof(buf));

    if (count > 0 && buf[0] == '!') {
      processCommand(buf, count);
      // Send the updated gamepad report after processing the command
      usb_hid.sendReport(0, &gp, sizeof(gp));
    }
  }

  // Call inputTest() function
  inputTest();

  // Add a delay to avoid running the test too frequently
  delay(5000);  // Wait for 5 seconds before running the test again
}
void processCommand(uint8_t* buf, int count) {
  switch (buf[1]) {
    case 'A':
      if (count >= 4) {
        gp.x = buf[2];
        gp.y = buf[3];
      }
      break;
    case 'B':
      if (count >= 6) {
        gp.buttons = (buf[2] << 24) | (buf[3] << 16) | (buf[4] << 8) | buf[5];
      }
      break;
    case 'H':
      if (count >= 3) {
        gp.hat = buf[2];
      }
      break;
    case 'T':
      if (count >= 4) {
        gp.rx = buf[2];
        gp.ry = buf[3];
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

void inputTest() {
  if (!usb_hid.ready()) return;

  memset(&gp, 0, sizeof(gp));
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);

  gp.x = 127;
  gp.y = 0;
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);
  gp.x = -127;
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);
  gp.x = 0;
  gp.y = -127;
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);
  gp.y = 127;
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);

  for (int i = 0; i < 16; i++) {
    gp.buttons = (1U << i);
    usb_hid.sendReport(0, &gp, sizeof(gp));
    delay(500);
  }

  memset(&gp, 0, sizeof(gp));
  usb_hid.sendReport(0, &gp, sizeof(gp));
}