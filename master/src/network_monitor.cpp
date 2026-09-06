#include "network_monitor.h"

#include <ETH.h>
#include <WiFi.h>

#include "config.h"
#include "device_config.h"

namespace {
void onNetworkEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_ETH_START) ETH.setHostname("marklabs-netguard");
  // State is sampled in update(); callback intentionally performs no blocking work.
}
}  // namespace

void NetworkMonitor::begin() {
  WiFi.mode(WIFI_OFF);
  WiFi.onEvent(onNetworkEvent);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ETH.begin(ETH_PHY_LAN8720, config::kEthPhyAddress, config::kEthMdcPin,
            config::kEthMdioPin, config::kEthPowerPin, ETH_CLOCK_GPIO0_IN);
#else
  ETH.begin(config::kEthPhyAddress, config::kEthPowerPin, config::kEthMdcPin,
            config::kEthMdioPin, ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN);
#endif
  const DeviceConfig cfg = config_.snapshot();
  if (!cfg.useDhcp) {
    IPAddress ip, gateway, subnet, dns;
    ip.fromString(cfg.deviceIp); gateway.fromString(cfg.deviceGateway);
    subnet.fromString(cfg.subnetMask); dns.fromString(cfg.primaryDns);
    ETH.config(ip, gateway, subnet, dns);
  }
}

void NetworkMonitor::update(uint32_t now) {
  (void)now;
  const bool newLink = ETH.linkUp();
  if (newLink != linkUp_) {
    linkUp_ = newLink;
    ++linkChanges_;
  }
  hasIp_ = linkUp_ && ETH.localIP() != IPAddress(0, 0, 0, 0);
  if (hasIp_) {
    linkSpeedMbps_ = ETH.linkSpeed();
    fullDuplex_ = ETH.fullDuplex();
    snprintf(ipAddress_, sizeof(ipAddress_), "%u.%u.%u.%u", ETH.localIP()[0],
             ETH.localIP()[1], ETH.localIP()[2], ETH.localIP()[3]);
    String mac = ETH.macAddress();
    mac.toCharArray(macAddress_, sizeof(macAddress_));
  } else {
    strncpy(ipAddress_, "0.0.0.0", sizeof(ipAddress_));
    linkSpeedMbps_ = 0;
    fullDuplex_ = false;
  }
}
