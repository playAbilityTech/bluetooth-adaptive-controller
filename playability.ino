#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>

// HID report descriptor using TinyUSB's template
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_GAMEPAD()
};

// USB HID object
Adafruit_USBD_HID usb_hid;

// Report payload
hid_gamepad_report_t gp = {};

// BLE Service
BLEUart bleuart;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("Gamepad Controller - Initialization Started");

  // Manual begin() is required on core without built-in support e.g. mbed rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  // Set up USB HID
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  // Wait for USB to be ready
  while (!TinyUSBDevice.mounted()) delay(10);

  Serial.println("USB HID initialized");

  // Send a single button press (Button 1)
  memset(&gp, 0, sizeof(gp));  // Clear the gamepad report
  gp.buttons = 1;  // Set the first button (Button 1)
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(100);  // Short delay
  
  // Release the button
  gp.buttons = 0;
  usb_hid.sendReport(0, &gp, sizeof(gp));

  Serial.println("Sent initial button press");

  // Setup BLE
  Bluefruit.begin();
  Bluefruit.setName("playAbility");
  Serial.println("BLE initialized");

  // Configure and start BLE Uart service
  bleuart.begin();
  Serial.println("BLE UART service started");

  // Set up and start advertising
  startAdv();

  Serial.println("Gamepad Controller - Initialization Completed");
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
  Serial.println("BLE advertising started");
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

  // Optionally, you can add a periodic report send here
  // static uint32_t last_report_time = 0;
  // if (millis() - last_report_time > 10) { // Send report every 10ms
  //   usb_hid.sendReport(0, &gp, sizeof(gp));
  //   last_report_time = millis();
  // }
}

void processCommand(uint8_t* buf, int count) {
  Serial.print("Received BLE data: ");
  Serial.write(buf, count);
  Serial.println();

  switch (buf[1]) {
    case 'A': // Analog stick
      if (count >= 4) {
        gp.x = buf[2];
        gp.y = buf[3];
        Serial.printf("Analog stick: X=%d, Y=%d\n", gp.x, gp.y);
      }
      break;
    case 'B': // Buttons
      if (count >= 6) {
        gp.buttons = (buf[2] << 24) | (buf[3] << 16) | (buf[4] << 8) | buf[5];
        Serial.printf("Buttons: 0x%08X\n", gp.buttons);
      }
      break;
    case 'H': // Hat
      if (count >= 3) {
        gp.hat = buf[2];
        Serial.printf("Hat: 0x%02X\n", gp.hat);
      }
      break;
    case 'T': // Triggers
      if (count >= 4) {
        gp.rx = buf[2];
        gp.ry = buf[3];
        Serial.printf("Triggers: RX=%d, RY=%d\n", gp.rx, gp.ry);
      }
      break;
    case 'Z': // Full update
      if (count >= sizeof(hid_gamepad_report_t) + 2) {
        memcpy(&gp, buf + 2, sizeof(hid_gamepad_report_t));
        Serial.printf("Full gamepad update: X=%d, Y=%d, Z=%d, RZ=%d, RX=%d, RY=%d, Hat=0x%02X, Buttons=0x%08X\n", 
                      gp.x, gp.y, gp.z, gp.rz, gp.rx, gp.ry, gp.hat, gp.buttons);
      }
      break;
    default:
      Serial.println("Unknown command received");
      return;
  }

  usb_hid.sendReport(0, &gp, sizeof(gp));
}

// Test function (you can call this from loop() if needed)
void inputTest() {
  if (!usb_hid.ready()) return;

  // Reset all inputs
  memset(&gp, 0, sizeof(gp));
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);

  // Test joystick 1
  Serial.println("Testing Joystick 1");
  gp.x = 127;  // Right
  gp.y = 0;
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);
  gp.x = -127; // Left
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);
  gp.x = 0;
  gp.y = -127; // Up
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);
  gp.y = 127;  // Down
  usb_hid.sendReport(0, &gp, sizeof(gp));
  delay(1000);

  // Test buttons
  Serial.println("Testing Buttons");
  for (int i = 0; i < 16; i++) {
    gp.buttons = (1U << i);
    usb_hid.sendReport(0, &gp, sizeof(gp));
    delay(500);
  }

  // Reset all inputs
  memset(&gp, 0, sizeof(gp));
  usb_hid.sendReport(0, &gp, sizeof(gp));
}