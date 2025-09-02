# USB BOS/WebUSB/MS OS 2.0 Analyzer

A comprehensive USB descriptor analysis tool for validating BOS descriptors, WebUSB platform capabilities, and Microsoft OS 2.0 descriptor sets. Essential for USB gadget developers working with modern Windows driver binding and WebUSB functionality.

## Features

- **BOS Descriptor Analysis**: Complete parsing and validation of Binary Device Object Store descriptors
- **WebUSB Support**: Platform capability detection, URL descriptor fetching with automatic vendor code extraction
- **MS OS 2.0 Validation**: Full descriptor set parsing including Configuration/Function Subsets, Compatible ID, and Registry Properties
- **Color-coded Output**: Red for errors, orange for warnings, with detailed validation messages
- **Comprehensive Error Detection**: Length validation, structure verification, and spec compliance checking

## Use Cases

- Validate USB gadget descriptor implementations
- Debug WebUSB landing page issues
- Verify MS OS 2.0 descriptors for automatic WinUSB driver binding
- Analyze composite device descriptor hierarchies
- Troubleshoot USB enumeration problems

## Building

### Prerequisites

Install libusb-1.0 development package:

```bash
# Ubuntu/Debian
sudo apt install libusb-1.0-0-dev

# RHEL/CentOS/Fedora
sudo yum install libusb1-devel

# Arch Linux
sudo pacman -S libusb
```

### Compile

```bash
make build    # Build with dependency check
# or
make          # Build without dependency check
```

### Install (Optional)

```bash
make install  # Install to /usr/local/bin
```

## Usage

```bash
./usb_bos_webusb_msos20_analyzer <VID> <PID>
```

### Examples

```bash
# Using hex values
./usb_bos_webusb_msos20_analyzer 0x361d 0x0202

# Using decimal values  
./usb_bos_webusb_msos20_analyzer 13917 514

# After system install
usb_bos_webusb_msos20_analyzer 0x1234 0x5678
```

## Sample Output

```
=== BOS Descriptor Analysis ===
Total BOS length: 61 bytes

BOS Header:
  bLength: 5
  bDescriptorType: 0x0f (BOS)
  wTotalLength: 61
  bNumDeviceCaps: 2

Device Capability 0 (offset 5):
  bLength: 28
  bDescriptorType: 0x10 (DEVICE_CAPABILITY)
  bDevCapabilityType: 0x05
  Platform Capability:
    bReserved: 0
    UUID: 3408b638-09a9-47a0-8bfd-a0768815b665
    Type: WebUSB Platform Capability
    WebUSB Data:
      bcdVersion: 0x0100
      bVendorCode: 0x01
      iLandingPage: 1 (Present)

=== MS OS 2.0 Descriptor Analysis ===
Offset 0: Set Header (len=10, winver=0x06030000, total=176)
Offset 10: Configuration Subset Header (len=8, config=0, total=166)  
Offset 18: Function Subset Header (len=8, interface=0, subset=158)
Offset 26: Compatible ID Feature (len=20, compat='WINUSB  ', subcompat='        ')
Offset 46: Registry Property Feature (len=130, datatype=1, namelen=42)
  Property Name: DeviceInterfaceGUID
  Property Data: {12345678-1234-1234-1234-123456789abc}

✓ Descriptor appears to be well-formed
```

## Validation Features

### BOS Descriptor Validation
- Header structure verification
- Length consistency checks  
- Platform capability parsing
- UUID recognition (WebUSB, MS OS 2.0)

### WebUSB Analysis
- Capability data extraction
- Vendor code validation
- Landing page index detection
- Automatic URL descriptor fetching

### MS OS 2.0 Comprehensive Validation
- Descriptor hierarchy verification
- Length calculation validation
- Compatible ID verification (WINUSB expected)
- Registry property parsing (UTF-16LE)
- Configuration subset validation for composite devices
- Reserved field checking

## Common Issues Detected

- **Length Mismatches**: Calculated vs reported descriptor lengths
- **Invalid GUIDs**: Malformed or missing device interface GUIDs  
- **Encoding Errors**: UTF-16LE string encoding issues
- **Composite Device Errors**: Missing Configuration/Function subset headers
- **Vendor Code Conflicts**: Overlapping vendor codes between WebUSB/MS OS 2.0
- **Null Termination**: Missing null terminators in registry properties

## Technical Background

This tool addresses common USB descriptor validation challenges:

- **MS OS 2.0 Complexity**: Microsoft's specification has subtle requirements for composite devices
- **WebUSB Integration**: Proper vendor code management alongside MS OS 2.0
- **Registry Properties**: Critical for automatic driver binding, easy to get wrong
- **Composite Device Requirements**: Configuration and Function subset headers often missed

## Requirements

- Linux system with libusb-1.0
- USB device with BOS descriptor support (USB 2.1+)
- Root/sudo access may be required for device access

## Contributing

This tool was developed to solve real-world USB gadget development challenges. Contributions for additional descriptor types or validation improvements are welcome.

## References

- [Microsoft OS 2.0 Descriptors Specification](https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-os-2-0-descriptors-specification)
- [WebUSB Specification](https://wicg.github.io/webusb/)
- [Chrome WebUSB Development Guide](https://developer.chrome.com/docs/capabilities/build-for-webusb)
- [Working Composite Device Example](https://github.com/pololu/libusbp/blob/b4666d1/test/firmware/wixel/main.c)

## License

This tool is provided as-is for USB development and debugging purposes.