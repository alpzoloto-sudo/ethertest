# ESP32-S3 Super Mini PrintServer — experimental Canon LBP3010 test

Replaces EtherTest. Based on [WARMW00D/ESP32-S3-PrintServer](https://github.com/WARMW00D/ESP32-S3-PrintServer); upstream MIT license preserved in `firmware/PrintServer/LICENSE`.

**WARNING: Canon CAPT compatibility has NOT been verified.** Canon LBP3010 uses a proprietary host-based CAPT protocol. A standard RAW TCP 9100 / LPR server forwards print streams to USB but does not provide every bidirectional exchange of a native Canon USB CAPT driver. USB detection and a successful build do NOT prove printing compatibility.

## Hardware assumptions
ESP32-S3 Super Mini with 4MB flash, 2MB QSPI PSRAM (verify actual board). One native USB-C connector is usable in USB host mode if connected to the native USB D+/D- lines. A powered host/OTG adapter with correctly sourced 5V USB VBUS is necessary; do not source VBUS from a GPIO. The Canon printer has its own mains power.

## Compilation
GitHub Actions -> **Build PrintServer Super Mini**. The workflow installs ESP32 Arduino core 3.3.10 (3.3.11 has a reported USB Host regression), uses 4MB flash + QSPI PSRAM, and disables OLED. A successful run uploads artifact `PrintServer-SuperMini-EXPERIMENTAL`. Flash using Arduino IDE/CLI or the ESP32 bootloader, partitions, and app files at the appropriate offsets.

## Setup / test
1. Power and flash the board, then connect Canon through a proper powered USB host adapter.
2. Find ESP32-PrintServer AP and browse to http://192.168.4.1 ; configure home WiFi.
3. On the status page ensure printer enumeration works and Printer Class (0x07) is detected.
4. On Windows 11 try a Standard TCP/IP Port, RAW 9100, Canon CAPT driver. If Canon software insists on native USB or the printer does not print, it is NOT compatible without a new CAPT-aware transport implementation.

Upstream IPP/PWG-to-PCL component targets HP/PCL printers and is not a substitute for Canon CAPT. No real printer or hardware verification has been performed.
