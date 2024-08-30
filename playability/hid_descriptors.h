#ifndef HID_DESCRIPTORS_H
#define HID_DESCRIPTORS_H

#include <stdint.h>

extern const uint8_t desc_hid_report_gamepad[];
extern const uint8_t desc_hid_report_pokken[];
// Add more descriptor declarations as needed

extern const uint8_t* desc_hid_reports[];
extern const uint16_t desc_hid_report_sizes[];

#endif // HID_DESCRIPTORS_H