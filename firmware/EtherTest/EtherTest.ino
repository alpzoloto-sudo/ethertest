#include <M5Cardputer.h>
#include <EspUsbHost.h>
#include <esp_log.h>
#include <stdarg.h>

EspUsbHost usb;

// -----------------------------------------------------------------------------
// EtherTest USB diagnostic build
// Captures ESP-IDF log messages and shows the most recent one on the Cardputer
// screen. This lets us distinguish "nothing reached the USB controller" from
// enumeration/descriptor/host-driver failures.
// -----------------------------------------------------------------------------

static volatile uint8_t nicAddress = 0;
static volatile uint16_t nicVid = 0;
static volatile uint16_t nicPid = 0;
static volatile uint8_t nicConfiguration = 0;
static volatile bool deviceConnected = false;

static bool hostStarted = false;
static bool candidatesReported = false;
static bool networkCandidateFound = false;
static bool networkAttached = false;
static uint32_t connectedAt = 0;
static uint32_t lastUi = 0;

static char lastLog[220] = "No USB events yet";
static volatile uint32_t logCounter = 0;
static portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;

static int screenLogVprintf(const char *fmt, va_list args) {
  char tmp[220];
  va_list copy;
  va_copy(copy, args);
  int n = vsnprintf(tmp, sizeof(tmp), fmt, copy);
  va_end(copy);

  if (n > 0) {
    size_t len = strnlen(tmp, sizeof(tmp));
    while (len > 0 && (tmp[len - 1] == '\n' || tmp[len - 1] == '\r')) {
      tmp[--len] = 0;
    }

    portENTER_CRITICAL(&logMux);
    strncpy(lastLog, tmp, sizeof(lastLog) - 1);
    lastLog[sizeof(lastLog) - 1] = 0;
    logCounter++;
    portEXIT_CRITICAL(&logMux);
  }
  return n;
}

static void copyLastLog(char *dst, size_t len, uint32_t &count) {
  portENTER_CRITICAL(&logMux);
  strncpy(dst, lastLog, len - 1);
  dst[len - 1] = 0;
  count = logCounter;
  portEXIT_CRITICAL(&logMux);
}

static void drawWrappedLog(const char *s, int x, int y, int maxChars, int maxLines) {
  String text(s);
  int pos = 0;
  for (int line = 0; line < maxLines && pos < (int)text.length(); line++) {
    String part = text.substring(pos, pos + maxChars);
    M5Cardputer.Display.setCursor(x, y + line * 10);
    M5Cardputer.Display.print(part);
    pos += maxChars;
  }
}

static void drawUi() {
  char logCopy[220];
  uint32_t logs = 0;
  copyLastLog(logCopy, sizeof(logCopy), logs);

  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextWrap(false);

  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.setCursor(5, 4);
  M5Cardputer.Display.println("EtherTest USB DIAG");
  M5Cardputer.Display.drawFastHLine(5, 17, 230, TFT_DARKGREY);

  M5Cardputer.Display.setCursor(5, 23);
  M5Cardputer.Display.printf("Host: %s   up:%lus\n", hostStarted ? "READY" : "ERROR",
                             (unsigned long)(millis() / 1000));

  if (!deviceConnected) {
    M5Cardputer.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5Cardputer.Display.setCursor(5, 35);
    M5Cardputer.Display.println("USB: WAITING / NOT ENUMERATED");
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.setCursor(5, 47);
    M5Cardputer.Display.println("Unplug/replug adapter now.");
  } else {
    M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5Cardputer.Display.setCursor(5, 35);
    M5Cardputer.Display.println("USB: CONNECTED");
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.setCursor(5, 47);
    M5Cardputer.Display.printf("VID:PID %04X:%04X  CFG:%u\n",
                               nicVid, nicPid, nicConfiguration);

    M5Cardputer.Display.setCursor(5, 59);
    if (!candidatesReported) {
      M5Cardputer.Display.println("NET: probing descriptors...");
    } else if (!networkCandidateFound) {
      M5Cardputer.Display.setTextColor(TFT_RED, TFT_BLACK);
      M5Cardputer.Display.println("NET: no CDC-ECM/NCM");
      M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    } else if (!networkAttached) {
      M5Cardputer.Display.println("NET: found / attaching...");
    } else {
      IPAddress ip = usb.networkLocalIP(nicAddress);
      if ((uint32_t)ip == 0) {
        M5Cardputer.Display.println("NET: DHCP waiting...");
      } else {
        M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5Cardputer.Display.printf("NET: ONLINE %s\n", ip.toString().c_str());
        M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
      }
    }
  }

  M5Cardputer.Display.drawFastHLine(5, 75, 230, TFT_DARKGREY);
  M5Cardputer.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5Cardputer.Display.setCursor(5, 80);
  M5Cardputer.Display.printf("IDF log #%lu:", (unsigned long)logs);
  M5Cardputer.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  drawWrappedLog(logCopy, 5, 92, 38, 4);
}

static void reportNetworkCandidates(uint8_t address) {
  EspUsbHostNetworkInterfaceInfo networks[ESP_USB_HOST_MAX_NETWORK_INTERFACES];
  const size_t count =
      usb.getNetworkInterfaces(address, networks, ESP_USB_HOST_MAX_NETWORK_INTERFACES);

  networkCandidateFound = false;

  if (count == 0) {
    candidatesReported = true;
    return;
  }

  for (size_t i = 0; i < count; i++) {
    if (!networks[i].complete()) continue;
    if (networks[i].configurationValue == nicConfiguration) {
      networkCandidateFound = true;
    }
  }
  candidatesReported = true;
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);

  Serial.begin(115200);
  delay(300);

  // Capture ESP-IDF diagnostics before starting USB host.
  esp_log_set_vprintf(screenLogVprintf);
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  usb.setConfigurationSelector([](const usb_device_desc_t &device) -> uint8_t {
    // First pass: use the adapter's default configuration. If it enumerates,
    // EspUsbHost can inspect all configurations afterwards.
    (void)device;
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

    portENTER_CRITICAL(&logMux);
    strncpy(lastLog, "USB device disconnected", sizeof(lastLog) - 1);
    lastLog[sizeof(lastLog) - 1] = 0;
    logCounter++;
    portEXIT_CRITICAL(&logMux);
  });

  hostStarted = usb.begin();
  if (!hostStarted) {
    portENTER_CRITICAL(&logMux);
    snprintf(lastLog, sizeof(lastLog), "usb.begin failed: %s", usb.lastErrorName());
    logCounter++;
    portEXIT_CRITICAL(&logMux);
  }

  drawUi();
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
    }
  }

  if (millis() - lastUi > 250) {
    lastUi = millis();
    drawUi();
  }

  delay(5);
}
