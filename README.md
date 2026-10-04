# EtherTest

Pocket Ethernet/LAN tester firmware for **M5Stack Cardputer / Cardputer v1.1**.

The first target adapter is the Lenovo USB-C Ethernet adapter:

- USB VID:PID: `17EF:720C`
- commonly associated with the Realtek RTL8153 family

## Current stage: USB Ethernet probe

This first firmware intentionally starts with hardware detection before adding LAN tools.

It:

- starts the ESP32-S3 USB port in **host** mode;
- detects a connected USB device;
- shows VID/PID and active USB configuration on the Cardputer display;
- scans the device for **CDC-ECM / CDC-NCM** Ethernet interfaces;
- if a compatible network interface is found, attaches it to lwIP and starts DHCP;
- shows the acquired IP address on screen;
- prints detailed interface information to Serial at 115200 baud.

If `17EF:720C` exposes only Realtek's vendor-specific RTL8153 protocol, the display will report:

`Ethernet: no ECM/NCM`

That result is useful: it means the next step is an RTL8152/RTL8153 USB host driver rather than generic CDC Ethernet.

## Build

The repository contains a GitHub Actions workflow. Every push to `main` builds the firmware automatically.

Open **Actions → Build firmware**, then download the **EtherTest-Cardputer** artifact.

The main firmware binary is generated in the artifact together with the bootloader/partition output produced by Arduino-ESP32.

## Flashing

For development, flash from Arduino CLI/IDE using an ESP32-S3/Cardputer-compatible configuration.

Important: USB mode must stay on the normal/default Hardware CDC/JTAG setting. Do **not** select TinyUSB device/OTG mode, because EtherTest needs the ESP32-S3 peripheral as a USB **host**.

## Planned LAN tester features

Once Ethernet is working:

- DHCP/IP/mask/gateway/DNS status
- gateway ping
- DNS lookup test
- Internet connectivity test
- user-entered ping target
- ARP/LAN discovery
- common-port test
- traceroute where practical
- local throughput test

## Upstream

USB host networking is based on the experimental CDC-ECM/NCM support in
[tanakamasayuki/EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost).
