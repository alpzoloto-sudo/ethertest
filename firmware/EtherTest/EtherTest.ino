#include <M5Cardputer.h>
#include <EspUsbHost.h>

EspUsbHost usb;

static volatile uint8_t nicAddress = 0;
static volatile uint16_t nicVid = 0;
static volatile uint16_t nicPid = 0;
static volatile uint8_t nicConfiguration = 0;
static volatile bool deviceConnected = false;

static bool candidatesReported = false;
static bool networkCandidateFound = false;
static bool networkAttached = false;
static uint32_t connectedAt = 0;
static uint32_t lastUi = 0;

static String statusLine = "Waiting for USB Ethernet...";

static void drawUi() {
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.setTextSize(1);

  M5Cardputer.Display.setCursor(6, 6);
  M5Cardputer.Display.println("EtherTest / Cardputer");
  M5Cardputer.Display.drawFastHLine(6, 20, 228, TFT_DARKGREY);

  M5Cardputer.Display.setCursor(6, 28);
  if (!deviceConnected) {
    M5Cardputer.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5Cardputer.Display.println("USB: waiting");
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.println();
    M5Cardputer.Display.println("Connect Lenovo Ethernet");
    M5Cardputer.Display.println("adapter to USB-C.");
    return;
  }

  M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5Cardputer.Display.println("USB: connected");
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);

  M5Cardputer.Display.printf("VID:PID  %04X:%04X\n", nicVid, nicPid);
  M5Cardputer.Display.printf("Config   %u\n", nicConfiguration);

  if (!candidatesReported) {
    M5Cardputer.Display.println("Ethernet: probing...");
  } else if (!networkCandidateFound) {
    M5Cardputer.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5Cardputer.Display.println("Ethernet: no ECM/NCM");
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.println("RTL8153 driver may be needed");
  } else if (!networkAttached) {
    M5Cardputer.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5Cardputer.Display.println("Ethernet: found, DHCP...");
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  } else {
    IPAddress ip = usb.networkLocalIP(nicAddress);
    if ((uint32_t)ip == 0) {
      M5Cardputer.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
      M5Cardputer.Display.println("Ethernet: DHCP waiting");
      M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
      M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
      M5Cardputer.Display.println("Ethernet: ONLINE");
      M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
      M5Cardputer.Display.printf("IP: %s\n", ip.toString().c_str());
    }
  }

  M5Cardputer.Display.setCursor(6, 116);
  M5Cardputer.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5Cardputer.Display.print(statusLine);
}

static void reportNetworkCandidates(uint8_t address) {
  EspUsbHostNetworkInterfaceInfo networks[ESP_USB_HOST_MAX_NETWORK_INTERFACES];
  const size_t count =
      usb.getNetworkInterfaces(address, networks, ESP_USB_HOST_MAX_NETWORK_INTERFACES);

  networkCandidateFound = false;

  Serial.printf("\nNetwork candidates: %u (active configuration=%u)\n",
                (unsigned)count, nicConfiguration);

  if (count == 0) {
    Serial.println("No CDC-ECM / CDC-NCM interface exposed by this USB device.");
    statusLine = "No CDC-ECM/NCM";
    candidatesReported = true;
    return;
  }

  for (size_t i = 0; i < count; i++) {
    espUsbHostPrint(networks[i], Serial);

    if (!networks[i].complete()) {
      continue;
    }

    if (networks[i].configurationValue == nicConfiguration) {
      networkCandidateFound = true;
    } else {
      Serial.printf(
          "Candidate is in config %u. Selector rule:\n"
          "if (device.idVendor == 0x%04x && device.idProduct == 0x%04x) return %u;\n",
          networks[i].configurationValue,
          nicVid,
          nicPid,
          networks[i].configurationValue);
    }
  }

  candidatesReported = true;
  statusLine = networkCandidateFound ? "Network interface found" : "Alt config detected";
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);

  Serial.begin(115200);
  delay(500);

  drawUi();

  // Keep the default USB configuration on the first pass.
  // If the adapter exposes ECM/NCM in another configuration, the serial log
  // prints the exact selector rule needed for the next build.
  usb.setConfigurationSelector([](const usb_device_desc_t &device) -> uint8_t {
    if (device.idVendor == 0x17EF && device.idProduct == 0x720C) {
      return 0;
    }
    return 0;
  });

  usb.onDeviceConnected([](const EspUsbHostDeviceInfo &device) {
    nicAddress = device.address;
    nicVid = device.vid;
    nicPid = device.pid;
    nicConfiguration = device.configurationValue;
    deviceConnected = true;

    candidatesReported = false;
    networkCandidateFound = false;
    networkAttached = false;
    connectedAt = millis();

    statusLine = "USB device detected";

    Serial.printf(
        "\nUSB connected: addr=%u VID:PID=%04x:%04x config=%u\n",
        device.address, device.vid, device.pid, device.configurationValue);
  });

  usb.onDeviceDisconnected([](const EspUsbHostDeviceInfo &device) {
    (void)device;
    nicAddress = 0;
    nicVid = 0;
    nicPid = 0;
    nicConfiguration = 0;
    deviceConnected = false;
    candidatesReported = false;
    networkCandidateFound = false;
    networkAttached = false;
    statusLine = "USB disconnected";
    Serial.println("USB disconnected");
  });

  if (!usb.begin()) {
    statusLine = String("USB host error: ") + usb.lastErrorName();
    Serial.printf("USB host begin failed: %s\n", usb.lastErrorName());
  } else {
    Serial.println("EtherTest started. USB host ready.");
  }
}

void loop() {
  M5Cardputer.update();

  if (deviceConnected && nicAddress &&
      !candidatesReported &&
      millis() - connectedAt > 700) {
    reportNetworkCandidates(nicAddress);
  }

  if (deviceConnected && nicAddress &&
      candidatesReported &&
      networkCandidateFound &&
      !networkAttached) {
    EspUsbHostNetworkConfig netCfg;
    if (usb.networkAttachNetif(netCfg, nicAddress)) {
      networkAttached = true;
      statusLine = "DHCP client started";
      Serial.println("USB network netif attached; waiting for DHCP.");
    }
  }

  if (millis() - lastUi > 500) {
    lastUi = millis();
    drawUi();

    if (networkAttached) {
      IPAddress ip = usb.networkLocalIP(nicAddress);
      if ((uint32_t)ip != 0) {
        Serial.printf("USB Ethernet IP: %s\n", ip.toString().c_str());
      }
    }
  }

  delay(5);
}
