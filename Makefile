# Simple Makefile for USB BOS/WebUSB/MS OS 2.0 analyzer tool

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LIBS = -lusb-1.0
TARGET = usb_bos_webusb_msos20_analyzer
SOURCE = usb_bos_webusb_msos20_analyzer.c

# Default target
all: $(TARGET)

# Build the analyzer
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Install target (optional, installs to /usr/local/bin)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Check if libusb-1.0 is installed
check-deps:
	@pkg-config --exists libusb-1.0 || (echo "ERROR: libusb-1.0 development package not found. Install with:" && echo "  Ubuntu/Debian: sudo apt install libusb-1.0-0-dev" && echo "  RHEL/CentOS: sudo yum install libusb1-devel" && echo "  Arch: sudo pacman -S libusb" && false)
	@echo "libusb-1.0 development package found"

# Build with dependency check
build: check-deps $(TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  all        - Build the analyzer (default)"
	@echo "  build      - Build with dependency check"
	@echo "  install    - Install to /usr/local/bin (requires sudo)"
	@echo "  clean      - Remove build artifacts"
	@echo "  check-deps - Check if required dependencies are installed"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Usage example:"
	@echo "  make build"
	@echo "  ./$(TARGET) 0x361d 0x0202"

.PHONY: all build install clean check-deps help