#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// ANSI color codes
#define COLOR_RED     "\033[31m"
#define COLOR_ORANGE  "\033[33m"
#define COLOR_RESET   "\033[0m"

#define MS_OS_20_SET_HEADER_DESCRIPTOR          0x00
#define MS_OS_20_SUBSET_HEADER_CONFIGURATION    0x01
#define MS_OS_20_SUBSET_HEADER_FUNCTION         0x02
#define MS_OS_20_FEATURE_COMPATIBLE_ID          0x03
#define MS_OS_20_FEATURE_REG_PROPERTY           0x04

// BOS Descriptor Types
#define USB_DT_BOS                              0x0F
#define USB_DT_DEVICE_CAPABILITY                0x10

// Device Capability Types  
#define USB_PLAT_DEV_CAP_TYPE                   0x05

// WebUSB Constants
#define WEBUSB_GET_URL                          2
#define WEBUSB_URL_DESCRIPTOR_TYPE              3
#define WEBUSB_URL_SCHEME_HTTP                  0
#define WEBUSB_URL_SCHEME_HTTPS                 1
#define WEBUSB_URL_SCHEME_NONE                  255

// UUIDs (in string format for comparison)
static const char WEBUSB_UUID_STR[] = "3408b638-09a9-47a0-8bfd-a0768815b665";
static const char MSOS20_UUID_STR[] = "d8dd60df-4589-4cc7-9cd2-659d9e648a9f";

// BOS descriptor structure
struct usb_bos_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumDeviceCaps;
} __attribute__((packed));

// Platform capability descriptor structure
struct usb_plat_dev_cap_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDevCapabilityType;
    uint8_t  bReserved;
    uint8_t  UUID[16];
    uint8_t  CapabilityData[];
} __attribute__((packed));

static void uuid_to_string(const uint8_t *uuid, char *str) {
    sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            uuid[3], uuid[2], uuid[1], uuid[0],  // Little-endian DWORD
            uuid[5], uuid[4],                     // Little-endian WORD
            uuid[7], uuid[6],                     // Little-endian WORD
            uuid[8], uuid[9],                     // Big-endian bytes
            uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}

static void parse_bos_descriptor(unsigned char *data, int length) {
    int offset = 0;
    int error_count = 0;
    int warning_count = 0;
    
    printf("=== BOS Descriptor Analysis ===\n");
    printf("Total BOS length: %d bytes\n\n", length);
    
    if (length < 5) {
        printf(COLOR_RED "ERROR: BOS descriptor too short (%d bytes, minimum 5)\n" COLOR_RESET, length);
        return;
    }
    
    const struct usb_bos_descriptor *bos = (const struct usb_bos_descriptor *)data;
    
    printf("BOS Header:\n");
    printf("  bLength: %d\n", bos->bLength);
    printf("  bDescriptorType: 0x%02x (%s)\n", bos->bDescriptorType, 
           bos->bDescriptorType == USB_DT_BOS ? "BOS" : "UNKNOWN");
    printf("  wTotalLength: %d\n", bos->wTotalLength);
    printf("  bNumDeviceCaps: %d\n\n", bos->bNumDeviceCaps);
    
    if (bos->bDescriptorType != USB_DT_BOS) {
        printf(COLOR_RED "ERROR: Invalid BOS descriptor type\n" COLOR_RESET);
        error_count++;
    }
    
    if (bos->wTotalLength != length) {
        printf(COLOR_ORANGE "WARNING: BOS total length mismatch (reported=%d, actual=%d)\n" COLOR_RESET, 
               bos->wTotalLength, length);
        warning_count++;
    }
    
    offset = bos->bLength;
    int cap_count = 0;
    
    while (offset < length && cap_count < bos->bNumDeviceCaps) {
        if (offset + 3 > length) {
            printf(COLOR_RED "ERROR: Truncated device capability at offset %d\n" COLOR_RESET, offset);
            error_count++;
            break;
        }
        
        uint8_t cap_length = data[offset];
        uint8_t cap_type = data[offset + 1];
        uint8_t cap_capability_type = data[offset + 2];
        
        printf("Device Capability %d (offset %d):\n", cap_count, offset);
        printf("  bLength: %d\n", cap_length);
        printf("  bDescriptorType: 0x%02x (%s)\n", cap_type,
               cap_type == USB_DT_DEVICE_CAPABILITY ? "DEVICE_CAPABILITY" : "UNKNOWN");
        printf("  bDevCapabilityType: 0x%02x\n", cap_capability_type);
        
        if (cap_capability_type == USB_PLAT_DEV_CAP_TYPE && offset + (int)sizeof(struct usb_plat_dev_cap_descriptor) <= length) {
            const struct usb_plat_dev_cap_descriptor *plat_cap = (const struct usb_plat_dev_cap_descriptor *)(data + offset);
            char uuid_str[37];
            uuid_to_string(plat_cap->UUID, uuid_str);
            
            printf("  Platform Capability:\n");
            printf("    bReserved: %d\n", plat_cap->bReserved);
            printf("    UUID: %s\n", uuid_str);
            
            if (strcasecmp(uuid_str, WEBUSB_UUID_STR) == 0) {
                printf("    Type: WebUSB Platform Capability\n");
                if (cap_length >= sizeof(struct usb_plat_dev_cap_descriptor) + 4) {
                    uint16_t bcd_version = data[offset + sizeof(struct usb_plat_dev_cap_descriptor)] | 
                                         (data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 1] << 8);
                    uint8_t vendor_code = data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 2];
                    uint8_t landing_page = data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 3];
                    
                    printf("    WebUSB Data:\n");
                    printf("      bcdVersion: 0x%04x\n", bcd_version);
                    printf("      bVendorCode: 0x%02x\n", vendor_code);
                    printf("      iLandingPage: %d (%s)\n", landing_page,
                           landing_page == 1 ? "Present" : "Not Present");
                    
                    if (vendor_code == 0) {
                        printf("      " COLOR_ORANGE "WARNING: WebUSB vendor code is 0 (invalid)\n" COLOR_RESET);
                        warning_count++;
                    }
                }
            } else if (strcasecmp(uuid_str, MSOS20_UUID_STR) == 0) {
                printf("    Type: MS OS 2.0 Platform Capability\n");
                if (cap_length >= sizeof(struct usb_plat_dev_cap_descriptor) + 8) {
                    uint32_t win_version = data[offset + sizeof(struct usb_plat_dev_cap_descriptor)] |
                                         (data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 1] << 8) |
                                         (data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 2] << 16) |
                                         (data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 3] << 24);
                    uint16_t desc_set_len = data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 4] |
                                          (data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 5] << 8);
                    uint8_t vendor_code = data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 6];
                    uint8_t alt_enum = data[offset + sizeof(struct usb_plat_dev_cap_descriptor) + 7];
                    
                    printf("    MS OS 2.0 Data:\n");
                    printf("      dwWindowsVersion: 0x%08x\n", win_version);
                    printf("      wMSOSDescriptorSetTotalLength: %d\n", desc_set_len);
                    printf("      bMS_VendorCode: 0x%02x\n", vendor_code);
                    printf("      bAltEnumCode: %d\n", alt_enum);
                    
                    if (win_version != 0x06030000) {
                        printf("      " COLOR_ORANGE "WARNING: Unusual Windows version (expected 0x06030000)\n" COLOR_RESET);
                        warning_count++;
                    }
                }
            } else {
                printf("    Type: Unknown Platform Capability\n");
            }
        } else {
            printf("  Non-Platform Capability (type 0x%02x)\n", cap_capability_type);
        }
        
        printf("\n");
        offset += cap_length;
        cap_count++;
    }
    
    printf("=== BOS Summary ===\n");
    printf("Parsed %d device capabilities, %d errors, %d warnings\n", cap_count, error_count, warning_count);
    
    if (error_count == 0 && warning_count == 0) {
        printf("✓ BOS descriptor appears to be well-formed\n");
    } else if (error_count == 0) {
        printf("⚠ BOS descriptor is valid but has %d warning(s)\n", warning_count);
    } else {
        printf("✗ BOS descriptor has %d error(s) and %d warning(s)\n", error_count, warning_count);
    }
    printf("\n");
}

static void parse_webusb_url_descriptor(const unsigned char *data, int length) {
    printf("=== WebUSB URL Descriptor ===\n");
    printf("Length: %d bytes\n", length);
    
    if (length < 3) {
        printf(COLOR_RED "ERROR: WebUSB URL descriptor too short\n" COLOR_RESET);
        return;
    }
    
    uint8_t bLength = data[0];
    uint8_t bDescriptorType = data[1];
    uint8_t bScheme = data[2];
    
    printf("bLength: %d\n", bLength);
    printf("bDescriptorType: %d (%s)\n", bDescriptorType,
           bDescriptorType == WEBUSB_URL_DESCRIPTOR_TYPE ? "WebUSB URL" : "UNKNOWN");
    printf("bScheme: %d (", bScheme);
    
    const char *scheme_prefix;
    switch (bScheme) {
        case WEBUSB_URL_SCHEME_HTTP:
            printf("HTTP)\n");
            scheme_prefix = "http://";
            break;
        case WEBUSB_URL_SCHEME_HTTPS:
            printf("HTTPS)\n");
            scheme_prefix = "https://";
            break;
        case WEBUSB_URL_SCHEME_NONE:
            printf("None)\n");
            scheme_prefix = "";
            break;
        default:
            printf("Unknown)\n");
            scheme_prefix = "unknown://";
            break;
    }
    
    if (length > 3) {
        printf("URL: %s", scheme_prefix);
        for (int i = 3; i < length && i < bLength; i++) {
            printf("%c", data[i]);
        }
        printf("\n");
    }
    printf("\n");
}

static void parse_msos20_descriptor(const unsigned char *data, int length) {
    int offset = 0;
    int error_count = 0;
    int warning_count = 0;
    
    printf("=== MS OS 2.0 Descriptor Analysis ===\n");
    printf("Total descriptor length: %d bytes\n\n", length);
    
    while (offset < length) {
        // Check if we have enough bytes for basic header
        if (offset + 4 > length) {
            printf(COLOR_RED "ERROR: Truncated descriptor at offset %d (need 4 bytes, have %d)\n" COLOR_RESET, 
                   offset, length - offset);
            error_count++;
            break;
        }
        
        uint16_t wLength = data[offset] | (data[offset + 1] << 8);
        uint16_t wDescriptorType = data[offset + 2] | (data[offset + 3] << 8);
        
        printf("Offset %d: ", offset);
        
        // Validate basic length constraints
        if (wLength == 0) {
            printf(COLOR_RED "ERROR: Zero length descriptor at offset %d\n" COLOR_RESET, offset);
            error_count++;
            break;
        }
        
        if (wLength < 4) {
            printf(COLOR_RED "ERROR: Invalid descriptor length %d at offset %d (minimum is 4)\n" COLOR_RESET, 
                   wLength, offset);
            error_count++;
            break;
        }
        
        if (offset + wLength > length) {
            printf(COLOR_RED "ERROR: Descriptor extends beyond buffer (offset=%d, len=%d, buffer=%d)\n" COLOR_RESET, 
                   offset, wLength, length);
            error_count++;
            break;
        }
        
        switch (wDescriptorType) {
            case MS_OS_20_SET_HEADER_DESCRIPTOR: {
                if (wLength < 10) {
                    printf(COLOR_RED "ERROR: Set Header too short (len=%d, expected=10)\n" COLOR_RESET, wLength);
                    error_count++;
                } else {
                    uint32_t dwWindowsVersion = data[offset + 4] | (data[offset + 5] << 8) | 
                                               (data[offset + 6] << 16) | (data[offset + 7] << 24);
                    uint16_t wTotalLength = data[offset + 8] | (data[offset + 9] << 8);
                    printf("Set Header (len=%d, winver=0x%08x, total=%d)\n", 
                           wLength, dwWindowsVersion, wTotalLength);
                    
                    // Validate total length matches actual length
                    if (wTotalLength != length) {
                        printf("  " COLOR_ORANGE "WARNING: Total length mismatch (reported=%d, actual=%d)\n" COLOR_RESET, 
                               wTotalLength, length);
                        warning_count++;
                    }
                    
                    // Check if it's at the beginning
                    if (offset != 0) {
                        printf("  " COLOR_ORANGE "WARNING: Set Header not at beginning (offset=%d)\n" COLOR_RESET, offset);
                        warning_count++;
                    }
                    
                    // Validate Windows version
                    if (dwWindowsVersion != 0x06030000) {
                        printf("  " COLOR_ORANGE "WARNING: Unusual Windows version (expected=0x06030000 for Win 8.1)\n" COLOR_RESET);
                        warning_count++;
                    }
                }
                break;
            }
            case MS_OS_20_SUBSET_HEADER_CONFIGURATION: {
                if (wLength < 8) {
                    printf(COLOR_RED "ERROR: Configuration Subset Header too short (len=%d, expected=8)\n" COLOR_RESET, wLength);
                    error_count++;
                } else {
                    uint8_t bConfigurationValue = data[offset + 4];
                    uint8_t bReserved = data[offset + 5];
                    uint16_t wTotalLength = data[offset + 6] | (data[offset + 7] << 8);
                    printf("Configuration Subset Header (len=%d, config=%d, total=%d)\n", 
                           wLength, bConfigurationValue, wTotalLength);
                    
                    if (bReserved != 0) {
                        printf("  " COLOR_ORANGE "WARNING: Reserved field not zero (value=%d)\n" COLOR_RESET, bReserved);
                        warning_count++;
                    }
                    
                    // Validate subset length doesn't exceed remaining buffer
                    if (offset + wTotalLength > length) {
                        printf("  " COLOR_RED "ERROR: Configuration subset extends beyond buffer\n" COLOR_RESET);
                        error_count++;
                    }
                }
                break;
            }
            case MS_OS_20_SUBSET_HEADER_FUNCTION: {
                if (wLength < 8) {
                    printf(COLOR_RED "ERROR: Function Subset Header too short (len=%d, expected=8)\n" COLOR_RESET, wLength);
                    error_count++;
                } else {
                    uint8_t bFirstInterface = data[offset + 4];
                    uint8_t bReserved = data[offset + 5];
                    uint16_t wSubsetLength = data[offset + 6] | (data[offset + 7] << 8);
                    printf("Function Subset Header (len=%d, interface=%d, subset=%d)\n", 
                           wLength, bFirstInterface, wSubsetLength);
                    
                    if (bReserved != 0) {
                        printf("  " COLOR_ORANGE "WARNING: Reserved field not zero (value=%d)\n" COLOR_RESET, bReserved);
                        warning_count++;
                    }
                    
                    // Validate subset length
                    if (offset + wSubsetLength > length) {
                        printf("  " COLOR_RED "ERROR: Function subset extends beyond buffer\n" COLOR_RESET);
                        error_count++;
                    }
                    
                    if (wSubsetLength < wLength) {
                        printf("  " COLOR_RED "ERROR: Function subset length smaller than header length\n" COLOR_RESET);
                        error_count++;
                    }
                }
                break;
            }
            case MS_OS_20_FEATURE_COMPATIBLE_ID: {
                if (wLength < 20) {
                    printf(COLOR_RED "ERROR: Compatible ID Feature too short (len=%d, expected=20)\n" COLOR_RESET, wLength);
                    error_count++;
                } else {
                    printf("Compatible ID Feature (len=%d, compat='%.8s', subcompat='%.8s')\n", 
                           wLength, &data[offset + 4], &data[offset + 12]);
                    
                    // Check for null termination and padding
                    int has_winusb = (strncmp((char*)&data[offset + 4], "WINUSB", 6) == 0);
                    if (!has_winusb) {
                        printf("  " COLOR_ORANGE "WARNING: Compatible ID is not 'WINUSB'\n" COLOR_RESET);
                        warning_count++;
                    }
                    
                    // Check for proper null termination
                    if (data[offset + 4 + 6] != 0 || data[offset + 4 + 7] != 0) {
                        printf("  " COLOR_ORANGE "WARNING: Compatible ID not properly null-terminated\n" COLOR_RESET);
                        warning_count++;
                    }
                }
                break;
            }
            case MS_OS_20_FEATURE_REG_PROPERTY: {
                if (wLength < 8) {
                    printf(COLOR_RED "ERROR: Registry Property Feature too short (len=%d, minimum=8)\n" COLOR_RESET, wLength);
                    error_count++;
                } else {
                    uint16_t wPropertyDataType = data[offset + 4] | (data[offset + 5] << 8);
                    uint16_t wPropertyNameLength = data[offset + 6] | (data[offset + 7] << 8);
                    printf("Registry Property Feature (len=%d, datatype=%d, namelen=%d)\n", 
                           wLength, wPropertyDataType, wPropertyNameLength);
                    
                    // Validate property data type
                    if (wPropertyDataType != 1 && wPropertyDataType != 7) {
                        printf("  " COLOR_ORANGE "WARNING: Unusual property data type (1=REG_SZ, 7=REG_MULTI_SZ)\n" COLOR_RESET);
                        warning_count++;
                    }
                    
                    // Validate name length (should be even for UTF-16LE and include null terminator)
                    if (wPropertyNameLength == 0 || wPropertyNameLength % 2 != 0) {
                        printf("  " COLOR_RED "ERROR: Invalid property name length (must be even and >0)\n" COLOR_RESET);
                        error_count++;
                    } else if (offset + 8 + wPropertyNameLength > length) {
                        printf("  " COLOR_RED "ERROR: Property name extends beyond descriptor\n" COLOR_RESET);
                        error_count++;
                    } else {
                        // Parse property name (UTF-16LE)
                        printf("  Property Name: ");
                        int name_chars = 0;
                        for (int i = 0; i < wPropertyNameLength - 2; i += 2) {
                            if (offset + 8 + i + 1 < length) {
                                char c = data[offset + 8 + i];
                                if (c >= 32 && c <= 126) { // Printable ASCII
                                    printf("%c", c);
                                    name_chars++;
                                } else if (c == 0) {
                                    break; // Null terminator found early
                                } else {
                                    printf("?"); // Non-printable character
                                }
                            }
                        }
                        printf("\n");
                        
                        if (name_chars == 0) {
                            printf("  " COLOR_ORANGE "WARNING: Empty property name\n" COLOR_RESET);
                            warning_count++;
                        }
                        
                        // Parse property data length and data
                        int data_offset = offset + 8 + wPropertyNameLength;
                        if (data_offset + 2 > length) {
                            printf("  " COLOR_RED "ERROR: Property data length field beyond descriptor\n" COLOR_RESET);
                            error_count++;
                        } else {
                            uint16_t wPropertyDataLength = data[data_offset] | (data[data_offset + 1] << 8);
                            printf("  Property Data Length: %d\n", wPropertyDataLength);
                            
                            // Validate total size
                            int expected_total = 8 + wPropertyNameLength + 2 + wPropertyDataLength;
                            if (expected_total != wLength) {
                                printf("  " COLOR_RED "ERROR: Length mismatch (calculated=%d, reported=%d)\n" COLOR_RESET,
                                       expected_total, wLength);
                                error_count++;
                            }
                            
                            if (data_offset + 2 + wPropertyDataLength > length) {
                                printf("  " COLOR_RED "ERROR: Property data extends beyond descriptor\n" COLOR_RESET);
                                error_count++;
                            } else if (wPropertyDataLength > 0) {
                                printf("  Property Data: ");
                                for (int i = 0; i < wPropertyDataLength - 2; i += 2) {
                                    if (data_offset + 2 + i < length) {
                                        char c = data[data_offset + 2 + i];
                                        if (c >= 32 && c <= 126) {
                                            printf("%c", c);
                                        } else if (c == 0) {
                                            break;
                                        } else {
                                            printf("?");
                                        }
                                    }
                                }
                                printf("\n");
                            }
                        }
                    }
                }
                break;
            }
            default:
                printf(COLOR_RED "ERROR: Unknown Descriptor Type 0x%04x (len=%d)\n" COLOR_RESET, wDescriptorType, wLength);
                error_count++;
                break;
        }
        
        offset += wLength;
    }
    
    printf("\n=== Summary ===\n");
    printf("Parsing completed: %d errors, %d warnings\n", error_count, warning_count);
    
    if (error_count == 0 && warning_count == 0) {
        printf("✓ Descriptor appears to be well-formed\n");
    } else if (error_count == 0) {
        printf("⚠ Descriptor is valid but has %d warning(s)\n", warning_count);
    } else {
        printf("✗ Descriptor has %d error(s) and %d warning(s)\n", error_count, warning_count);
    }
    printf("\n");
}

int main(int argc, const char * const argv[]) {
    libusb_device_handle *handle;
    unsigned char buffer[512];
    int result;
    uint16_t vid, pid;
    char *endptr;

    if (argc != 3) {
        printf("Usage: %s <vid> <pid>\n", argv[0]);
        printf("Example: %s 0x361d 0x0202\n", argv[0]);
        printf("         %s 13917 514\n", argv[0]);
        return -1;
    }

    // Parse VID and PID from command line with error checking
    vid = (uint16_t)strtoul(argv[1], &endptr, 0);
    if (*endptr != '\0' || vid == 0) {
        printf(COLOR_RED "ERROR: Invalid VID '%s' (must be a valid hex or decimal number)\n" COLOR_RESET, argv[1]);
        return -1;
    }

    pid = (uint16_t)strtoul(argv[2], &endptr, 0);
    if (*endptr != '\0' || pid == 0) {
        printf(COLOR_RED "ERROR: Invalid PID '%s' (must be a valid hex or decimal number)\n" COLOR_RESET, argv[2]);
        return -1;
    }
    
    printf("Looking for USB device %04x:%04x\n", vid, pid);

    // Initialize libusb with error checking
    result = libusb_init(NULL);
    if (result != 0) {
        printf(COLOR_RED "ERROR: Failed to initialize libusb: %s\n" COLOR_RESET, libusb_error_name(result));
        return -1;
    }

    handle = libusb_open_device_with_vid_pid(NULL, vid, pid);
    if (!handle) {
        printf(COLOR_RED "ERROR: Device %04x:%04x not found\n" COLOR_RESET, vid, pid);
        printf("Make sure:\n");
        printf("- Device is connected and powered\n");
        printf("- You have permission to access USB devices (try with sudo)\n");
        printf("- VID:PID values are correct (check with lsusb)\n");
        libusb_exit(NULL);
        return -1;
    }

    printf("Device opened successfully\n");

    // Check if we need to detach kernel driver
    if (libusb_kernel_driver_active(handle, 0) == 1) {
        printf("Kernel driver is active on interface 0, attempting to detach...\n");
        result = libusb_detach_kernel_driver(handle, 0);
        if (result != 0 && result != LIBUSB_ERROR_NOT_FOUND) {
            printf(COLOR_ORANGE "WARNING: Could not detach kernel driver: %s\n" COLOR_RESET, libusb_error_name(result));
        }
    }

    // Clear buffer to ensure clean data
    memset(buffer, 0, sizeof(buffer));

    // First, fetch the BOS descriptor
    printf("=== Fetching BOS Descriptor ===\n");
    result = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN, LIBUSB_REQUEST_GET_DESCRIPTOR, 
                                   (USB_DT_BOS << 8), 0, buffer, sizeof(buffer), 5000);
    
    if (result > 0) {
        printf("SUCCESS: BOS descriptor retrieved (%d bytes)\n\n", result);
        
        // Raw hex dump of BOS
        printf("Raw BOS data:\n");
        for (int i = 0; i < result; i++) {
            printf("%02x ", buffer[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        if (result % 16 != 0) printf("\n");
        printf("\n");
        
        // Parse BOS descriptor
        parse_bos_descriptor(buffer, result);
        
        // Extract WebUSB vendor code and landing page index for later use
        uint8_t webusb_vendor_code = 0;
        uint8_t webusb_landing_page_index = 0;
        int bos_offset = 5; // Skip BOS header
        for (int cap = 0; cap < buffer[4] && bos_offset < result; cap++) {
            if (bos_offset + 3 > result) break;
            
            uint8_t cap_length = buffer[bos_offset];
            uint8_t cap_capability_type = buffer[bos_offset + 2];
            
            if (cap_capability_type == USB_PLAT_DEV_CAP_TYPE && 
                bos_offset + (int)sizeof(struct usb_plat_dev_cap_descriptor) <= result) {
                char uuid_str[37];
                uuid_to_string(&buffer[bos_offset + 4], uuid_str);
                
                if (strcasecmp(uuid_str, WEBUSB_UUID_STR) == 0 && 
                    cap_length >= sizeof(struct usb_plat_dev_cap_descriptor) + 4) {
                    webusb_vendor_code = buffer[bos_offset + sizeof(struct usb_plat_dev_cap_descriptor) + 2];
                    webusb_landing_page_index = buffer[bos_offset + sizeof(struct usb_plat_dev_cap_descriptor) + 3];
                    break;
                }
            }
            bos_offset += cap_length;
        }
        
        // Try to fetch WebUSB URL if we found a WebUSB capability
        if (webusb_vendor_code != 0 && webusb_landing_page_index != 0) {
            printf("=== Fetching WebUSB URL ===\n");
            printf("Using WebUSB vendor code: 0x%02x\n", webusb_vendor_code);
            printf("Using landing page index: %d\n", webusb_landing_page_index);
            
            memset(buffer, 0, sizeof(buffer));
            result = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR,
                                           webusb_vendor_code, webusb_landing_page_index, WEBUSB_GET_URL, 
                                           buffer, sizeof(buffer), 5000);
            
            if (result > 0) {
                printf("SUCCESS: WebUSB URL descriptor retrieved (%d bytes)\n\n", result);
                
                printf("Raw WebUSB URL data:\n");
                for (int i = 0; i < result; i++) {
                    printf("%02x ", buffer[i]);
                    if ((i + 1) % 16 == 0) printf("\n");
                }
                if (result % 16 != 0) printf("\n");
                printf("\n");
                
                parse_webusb_url_descriptor(buffer, result);
            } else {
                printf("INFO: WebUSB URL request failed (%d): %s\n", result, libusb_error_name(result));
                if (result == LIBUSB_ERROR_PIPE) {
                    printf("  This may indicate no landing page is configured\n");
                }
                printf("\n");
            }
        } else {
            printf("INFO: No WebUSB capability found in BOS descriptor\n\n");
        }
    } else {
        printf("INFO: BOS descriptor request failed (%d): %s\n", result, libusb_error_name(result));
        printf("Device may not support BOS descriptors (USB 2.0 device?)\n\n");
    }

    // Now test MS OS 2.0 descriptor
    printf("=== Fetching MS OS 2.0 Descriptor ===\n");
    memset(buffer, 0, sizeof(buffer));
    printf("Sending MS OS 2.0 descriptor request...\n");
    printf("  bmRequestType: 0xC0 (IN, VENDOR, DEVICE)\n");
    printf("  bRequest: 0x02 (MS OS vendor code)\n");
    printf("  wValue: 0x0000\n");
    printf("  wIndex: 0x0007 (MS_OS_20_DESCRIPTOR_INDEX)\n");
    printf("  wLength: %zu (buffer size)\n\n", sizeof(buffer));

    // Test MS OS 2.0 descriptor request
    result = libusb_control_transfer(handle, 0xC0, 0x02, 0x0000, 0x0007, buffer, sizeof(buffer), 5000);

    if (result > 0) {
        printf("SUCCESS: MS OS 2.0 descriptor retrieved (%d bytes)\n\n", result);
        
        // Validate minimum expected size
        if (result < 10) {
            printf(COLOR_ORANGE "WARNING: Descriptor very short (%d bytes), may be truncated\n" COLOR_RESET, result);
        }
        
        // Raw hex dump
        printf("Raw MS OS 2.0 data:\n");
        for (int i = 0; i < result; i++) {
            printf("%02x ", buffer[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        if (result % 16 != 0) printf("\n");
        printf("\n");
        
        // Parse the descriptor
        parse_msos20_descriptor(buffer, result);
    } else if (result == 0) {
        printf(COLOR_ORANGE "WARNING: Device returned 0 bytes (empty response)\n" COLOR_RESET);
        printf("This may indicate the device doesn't support MS OS 2.0 descriptors\n");
    } else {
        printf(COLOR_RED "ERROR: Failed to get MS OS 2.0 descriptor (%d): %s\n" COLOR_RESET, result, libusb_error_name(result));
        
        switch (result) {
            case LIBUSB_ERROR_PIPE:
                printf("  Device returned STALL - likely doesn't support MS OS 2.0 descriptors\n");
                printf("  or the vendor code (0x02) is incorrect\n");
                break;
            case LIBUSB_ERROR_TIMEOUT:
                printf("  Request timed out - device may be unresponsive\n");
                break;
            case LIBUSB_ERROR_NO_DEVICE:
                printf("  Device was disconnected during request\n");
                break;
            case LIBUSB_ERROR_ACCESS:
                printf("  Access denied - try running with sudo\n");
                break;
            case LIBUSB_ERROR_NOT_SUPPORTED:
                printf("  Control transfer not supported by device or host controller\n");
                break;
            default:
                printf("  Check device documentation for supported vendor requests\n");
                break;
        }
    }

    libusb_close(handle);
    libusb_exit(NULL);
    return (result > 0) ? 0 : -1;
}
