#include <string.h>

#define HID_GET_DESCRIPTOR 0x06
#define HID_DESCRIPTOR_TYPE_HID 0x21
#define HID_DESCRIPTOR_TYPE_REPORT 0x22

struct __attribute__((packed)) HidDescriptor {
  unsigned char bLength;
  unsigned char bDescriptorType;
  unsigned short bcdHID;
  unsigned char bCountryCode;
  unsigned char bNumDescriptors;
  unsigned char bDescriptorType2;
  unsigned short wDescriptorLength;
};

/*
 * HID Report Descriptor for SoundSlide
 *
 * Report Structure (4 bytes total):
 *   Byte 0: Consumer Control keys (bit flags)
 *           - Bit 0: Volume Up
 *           - Bit 1: Volume Down
 *           - Bit 2: Brightness Up
 *           - Bit 3: Brightness Down
 *           - Bit 4: Microphone Mute (single tap triggers this)
 *           - Bits 5-7: Padding
 *   Byte 1: Mouse scroll wheel (-127 to 127)
 *   Byte 2: Keyboard modifiers (bit flags)
 *           - Bit 0: Left Ctrl
 *           - Bit 1: Left Shift
 *           - Bit 2: Left Alt
 *           - Bit 3: Left GUI (Windows key) - used for Win+L lock
 *           - Bits 4-7: Right modifiers (not used)
 *   Byte 3: Keyboard key code (e.g., 0x0F = 'L' for lock workstation)
 *
 * Tap Actions:
 *   - Single tap: Sends Microphone Mute (Consumer Control)
 *   - Double tap: Sends Win+L (Keyboard) to lock workstation on Windows
 */
const unsigned char hidReportDescriptor[] = {

  // Consumer Control (media keys) - Report ID implicit (single report)
  0x05, 0x0C,        // Usage Page (Consumer Devices)
  0x09, 0x01,        // Usage (Consumer Control)
  0xA1, 0x01,        // Collection (Application)
  0x09, 0xE9,        //   Usage (Volume Up)
  0x09, 0xEA,        //   Usage (Volume Down)
  0x09, 0x6F,        //   Usage (Brightness Up)
  0x09, 0x70,        //   Usage (Brightness Down)
  0x09, 0xF8,        //   Usage (Microphone Mute) - triggered by single tap
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1 bit)
  0x95, 0x05,        //   Report Count (5) - 5 buttons
  0x81, 0x02,        //   Input (Data, Variable, Absolute)
  0x95, 0x03,        //   Report Count (3) - Padding bits to make a byte
  0x81, 0x03,        //   Input (Constant, Variable, Absolute)
  0xC0,              // End Collection

  // Mouse Scroll
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x02,        // Usage (Mouse)
  0xA1, 0x01,        // Collection (Application)
  0x09, 0x01,        //   Usage (Pointer)
  0xA1, 0x00,        //   Collection (Physical)
  0x09, 0x38,        //     Usage (Wheel)
  0x15, 0x81,        //     Logical Min (-127)
  0x25, 0x7F,        //     Logical Max (127)
  0x75, 0x08,        //     Report Size (8)
  0x95, 0x01,        //     Report Count (1)
  0x81, 0x06,        //     Input (Data, Var, Rel)
  0xC0,              //   End Collection
  0xC0,              // End Collection

  // Keyboard (for Win+L lock workstation) - triggered by double tap
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x06,        // Usage (Keyboard)
  0xA1, 0x01,        // Collection (Application)
  // Modifier keys (1 byte)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0xE0,        //   Usage Minimum (Left Ctrl = 0xE0)
  0x29, 0xE7,        //   Usage Maximum (Right GUI = 0xE7)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1 bit)
  0x95, 0x08,        //   Report Count (8 bits for 8 modifiers)
  0x81, 0x02,        //   Input (Data, Variable, Absolute)
  // Key code (1 byte) - simplified to single key
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0x00,        //   Usage Minimum (0)
  0x29, 0x65,        //   Usage Maximum (101 keys)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x65,        //   Logical Maximum (101)
  0x75, 0x08,        //   Report Size (8 bits)
  0x95, 0x01,        //   Report Count (1 key)
  0x81, 0x00,        //   Input (Data, Array)
  0xC0               // End Collection

};

// Keyboard modifier bit flags (byte 2 of report)
const unsigned char MODIFIER_LEFT_GUI = 0x08;  // Windows/Command key

// Keyboard key codes (byte 3 of report)
const unsigned char KEY_CODE_L = 0x0F;  // 'L' key for lock workstation

/*
 * HidEndpoint handles sending HID reports to the host.
 *
 * Report format (4 bytes):
 *   [0] Consumer keys (Volume Up/Down, Brightness Up/Down, Mic Mute)
 *   [1] Scroll wheel
 *   [2] Keyboard modifiers (Left GUI for Win key)
 *   [3] Keyboard key code ('L' for lock)
 *
 * Usage:
 *   - reportKey(KEY_VOLUME_UP/DOWN, count): Volume control
 *   - reportKey(KEY_BRIGHTNESS_UP/DOWN, count): Brightness control
 *   - reportKey(KEY_MIC_MUTE, 1): Microphone mute (single tap)
 *   - reportKey(KEY_LOCK_WORKSTATION, 1): Win+L lock (double tap)
 *   - reportScroll(steps): Mouse wheel scrolling
 */
class HidEndpoint : public usbd::UsbEndpoint {
public:
  int key = 0;
  int count = 0;
  int scroll = 0;
  unsigned char txBuffer[4];  // Expanded to 4 bytes for keyboard support

  void init() {
    txBufferPtr = txBuffer;
    txBufferSize = sizeof(txBuffer);
    usbd::UsbEndpoint::init();
  }

  void reportKey(int key, int count) {
    if (count) {
      this->key = key;
      this->count = count * 2; // one for key down and one for key up
      sendReport();
    }
  }

  void reportScroll(int steps) {
    this->key = 0;
    this->count = 0;
    this->scroll = steps;
    sendReport();
  }

  void sendReport() {
    // Clear the buffer
    txBuffer[0] = 0;  // Consumer keys
    txBuffer[1] = 0;  // Scroll
    txBuffer[2] = 0;  // Keyboard modifiers
    txBuffer[3] = 0;  // Keyboard key code

    if (count > 0) {
      bool keyDown = !(count & 1);  // Even count = key down, odd = key up

      if (key == KEY_LOCK_WORKSTATION) {
        // Keyboard report for Win+L (lock workstation)
        // Double tap triggers this action
        if (keyDown) {
          txBuffer[2] = MODIFIER_LEFT_GUI;  // Windows key modifier
          txBuffer[3] = KEY_CODE_L;         // 'L' key
        }
        // else: key up, all zeros (release all keys)
      } else {
        // Consumer control keys (Volume, Brightness, Mic Mute)
        if (keyDown) {
          txBuffer[0] = (1 << key);
        }
      }
      count--;
    }

    txBuffer[1] = scroll;
    scroll = 0;

    startTx(sizeof(txBuffer));
  }

  void txComplete() {
    if (count) {
      sendReport();
    }
  }

};

class HidInterface : public usbd::UsbInterface {
public:
  HidEndpoint hidEndpoint;

  virtual UsbEndpoint* getEndpoint(int index) { return index == 0 ? &hidEndpoint : NULL; }

  const char* getLabel() { return "SoundSlide HID"; }

  void checkDescriptor(InterfaceDescriptor* interfaceDescriptor) {
    interfaceDescriptor->bInterfaceClass = 0x03;
    interfaceDescriptor->bInterfaceSubclass = 0x00;
    interfaceDescriptor->bInterfaceProtocol = 0x00;
  };

  int getClassDescriptorLength() { return sizeof(HidDescriptor); }

  void checkClassDescriptor(unsigned char* buffer) {
    HidDescriptor* hidDescriptor = (HidDescriptor*)buffer;
    hidDescriptor->bLength = sizeof(HidDescriptor);
    hidDescriptor->bDescriptorType = HID_DESCRIPTOR_TYPE_HID;
    hidDescriptor->bcdHID = 0x0110; // HID Class Specification 1.10
    hidDescriptor->bCountryCode = 0; // Not Supported
    hidDescriptor->bNumDescriptors = 1; // Number of HID class descriptors to follow
    hidDescriptor->bDescriptorType2 = HID_DESCRIPTOR_TYPE_REPORT;
    hidDescriptor->wDescriptorLength = sizeof(hidReportDescriptor);
  }

  void setup(SetupData* setup) {
    usbd::UsbEndpoint* endpoint = device->getControlEndpoint();
    if (
      setup->bRequest == HID_GET_DESCRIPTOR &&
      setup->wValue == (HID_DESCRIPTOR_TYPE_REPORT << 8) | 0 &&
      setup->wIndex == 0
      ) {
      memcpy(endpoint->txBufferPtr, hidReportDescriptor, sizeof(hidReportDescriptor));
      endpoint->startTx(sizeof(hidReportDescriptor));
    }
    else {
      endpoint->stall();
    }
  }


};