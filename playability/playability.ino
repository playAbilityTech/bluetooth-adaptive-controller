#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>
#include "hid_descriptors.h"

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

Adafruit_USBD_HID usb_hid;

hid_gamepad_report_t gp = {};

BLEUart bleuart;

uint8_t current_descriptor = 1;

// Add these constants near the top of the file
#define GAMEPAD_VID 0x045e
#define GAMEPAD_PID 0x02a9
#define POKKEN_VID 0x0F0D
#define POKKEN_PID 0x0092

// Add these near the top of your file
#define USB_MANUFACTURER "playAbility"
#define USB_PRODUCT "Adaptive controller"


// Add this near the top of the file, after including "hid_descriptors.h"
extern const uint8_t* desc_hid_reports[];
extern const uint16_t desc_hid_report_sizes[];

void setup() {
  // Set VID and PID based on the current descriptor
  if (current_descriptor == 0) {
    // Gamepad descriptor
    TinyUSBDevice.setID(GAMEPAD_VID, GAMEPAD_PID);
  } else if (current_descriptor == 1) {
    // Pokken descriptor
    TinyUSBDevice.setID(POKKEN_VID, POKKEN_PID);
  }

  // Set additional USB device information
  TinyUSBDevice.setManufacturerDescriptor(USB_MANUFACTURER);
  TinyUSBDevice.setProductDescriptor(USB_PRODUCT);


  // You can also set USB version (default is 2.0)
  // TinyUSBDevice.setVersion(0x0210); // USB 2.1

  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  Serial.begin(115200);

  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_reports[current_descriptor], desc_hid_report_sizes[current_descriptor]);
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

// Add this new structure to match the Pokken controller layout
typedef struct {
  uint16_t buttons; // 14 buttons + 2 padding bits
  uint8_t hat;
  uint8_t leftStickX;
  uint8_t leftStickY;
  uint8_t rightStickX;
  uint8_t rightStickY;
  uint8_t padding;
} pokken_report_t;

pokken_report_t pokken_report = {};

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
        if (current_descriptor == 1) { // Pokken descriptor
          mapGamepadToPokken();
        }
      }
      break;
    case 'M': // New command to change descriptor
      if (count >= 3) {
        changeDescriptor(buf[2] - '0'); // Convert ASCII to integer
      }
      break;
    default:
      return;
  }

  if (current_descriptor == 0) {
    usb_hid.sendReport(0, &gp, sizeof(gp));
  } else if (current_descriptor == 1) {
    usb_hid.sendReport(0, &pokken_report, sizeof(pokken_report));
  }
}

void mapGamepadToPokken() {
  // Map buttons
  pokken_report.buttons = gp.buttons & 0x3FFF; // Only use the first 14 bits

  // Map hat switch
  pokken_report.hat = gp.hat;

  // Map analog sticks
  pokken_report.leftStickX = map(gp.x, -128, 127, 0, 255);
  pokken_report.leftStickY = map(gp.y, -128, 127, 0, 255);
  pokken_report.rightStickX = map(gp.z, -128, 127, 0, 255);
  pokken_report.rightStickY = map(gp.rz, -128, 127, 0, 255);

  // Clear padding
  pokken_report.padding = 0;
}

void changeDescriptor(uint8_t index) {
    const uint8_t NUM_DESCRIPTORS = 2;
    
    if (index < NUM_DESCRIPTORS) {
        current_descriptor = index;
        usb_hid.setReportDescriptor(desc_hid_reports[current_descriptor], desc_hid_report_sizes[current_descriptor]);
        
        // Set new VID and PID based on the new descriptor
        if (current_descriptor == 0) {
            TinyUSBDevice.setID(GAMEPAD_VID, GAMEPAD_PID);
        } else if (current_descriptor == 1) {
            TinyUSBDevice.setID(POKKEN_VID, POKKEN_PID);
        }
        
        // Reinitialize USB
        USBDevice.detach();
        delay(1000); // Wait for detach to complete
        USBDevice.attach();
    }
}

