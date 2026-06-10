// agri-flow-poe — DIGITEN hall-effect flow meter node on M5 ATOM PoE.
// Publishes instantaneous flow rate (L/min) and cumulative volume (L) to
// MQTT and UECS-CCM via the agri-node-poe-core library.

#include <Arduino.h>
#include <AgriNode.h>

#include "config.h"
#include "sensors.h"
#include "mqtt_pub.h"
#include "ccm_pub.h"

const char *FW_NAME     = "agri-flow-poe";
const char *FW_VERSION  = "0.3.0";
const char *FW_REPO     = "yasunorioi/agri-flow-poe";
const char *FW_BIN_NAME = "agri-flow-poe.bin";

AppConfig g_cfg;
volatile uint32_t g_pulse_count       = 0;
uint32_t          g_cum_pulses        = 0;
uint32_t          g_last_window_ms    = 0;
uint32_t          g_last_window_pulses = 0;
float             g_flow_lpm          = 0.0f;

// Flow ISR. Defined here (not in sensors.h) so the IRAM_ATTR placement
// resolves against a DRAM-located literal pool — keeping it inline in a
// header trips a "dangerous relocation: l32r" link error on ESP32.
void IRAM_ATTR onFlowPulse() {
  g_pulse_count++;
}

static String renderDashboardSensors() {
  String s; s.reserve(300);
  char buf[12];
  s = F("<h3>Flow</h3><table>");
  dtostrf(g_flow_lpm, 1, 3, buf);
  s += "<tr><th>Flow rate</th><td>"; s += buf;             s += " L/min</td></tr>";
  dtostrf(volumeLiters(), 1, 3, buf);
  s += "<tr><th>Total volume</th><td>"; s += buf;          s += " L</td></tr>";
  s += "<tr><th>Raw pulses</th><td>"; s += g_cum_pulses;   s += "</td></tr>";
  s += "<tr><th>Pulses / L</th><td>"; s += g_cfg.pulses_per_liter; s += " (configurable)</td></tr>";
  s += F("</table>"
         "<form method=POST action='/reset'><input type=submit value='Reset volume'></form>");
  return s;
}

static String renderConfigSensorRows() {
  String s;
  auto row = [&](const char *label, const String &input) {
    s += "<tr><th>"; s += label; s += "</th><td>"; s += input; s += "</td></tr>";
  };
  row("Pulses per litre",
      "<input type=number name=ppl value='" + String(g_cfg.pulses_per_liter) + "'>");
  row("Order (Flow)",
      "<input type=number name=ccm_ofl value='" + String(g_cfg.ccm_order_flow) + "'>");
  row("Order (Cumulative)",
      "<input type=number name=ccm_oco value='" + String(g_cfg.ccm_order_cons) + "'>");
  return s;
}

static void applyConfigSensorForm(const String &body) {
  g_cfg.pulses_per_liter = (uint16_t)agri::parseFormInt(body, "ppl",     g_cfg.pulses_per_liter);
  g_cfg.ccm_order_flow   = (int16_t) agri::parseFormInt(body, "ccm_ofl", g_cfg.ccm_order_flow);
  g_cfg.ccm_order_cons   = (int16_t) agri::parseFormInt(body, "ccm_oco", g_cfg.ccm_order_cons);
}

static void addStatusFields(JsonObject doc) {
  doc["flow_lpm"]   = g_flow_lpm;
  doc["volume_l"]   = volumeLiters();
  doc["raw_pulses"] = g_cum_pulses;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== %s v%s ===\n", FW_NAME, FW_VERSION);

  agri::Led::begin();
  loadConfig();
  Serial.printf("[CFG] node=%s mqtt_host=%s ccm=%s ppl=%u\n",
                g_cfg.common.node_id,
                g_cfg.common.mqtt_host[0] ? g_cfg.common.mqtt_host : "(unset)",
                g_cfg.common.ccm_enabled ? "on" : "off",
                g_cfg.pulses_per_liter);

  sensorsBegin();

  agri::Network::begin(g_cfg.common.hostname);
  agri::Network::waitForLease();

  agri::ccmBegin();
  agri::MQTT::begin();

  agri::WebHooks hooks;
  hooks.nodeTitle             = [](){ return FW_NAME; };
  hooks.renderDashboardSensors= renderDashboardSensors;
  hooks.renderConfigSensorRows= renderConfigSensorRows;
  hooks.applyConfigSensorForm = applyConfigSensorForm;
  hooks.addStatusFields       = addStatusFields;
  hooks.saveConfig            = [](){ saveConfig(); };
  agri::WebUI::begin(g_cfg.common, hooks, FW_NAME, FW_VERSION);

  agri::mdnsBegin(g_cfg.common.hostname);
  agri::otaBegin(g_cfg.common.hostname);

  agri::OTA::begin(FW_REPO, FW_BIN_NAME, FW_VERSION);
  agri::OTA::checkLatest();

  Serial.println("[BOOT] ready");
}

void loop() {
  agri::otaHandle();
  agri::OTA::poll();
  agri::WebUI::handle(agri::Network::link_up, agri::Network::have_lease);

  uint32_t now = millis();

  sensorsPoll();  // rate-limited internally to ~2 Hz

  if (agri::networkUp() && agri::MQTT::hasHost(g_cfg.common)) {
    if (!agri::MQTT::connected()) {
      static uint32_t lastTry = 0;
      if (now - lastTry > 5000) { lastTry = now; agri::MQTT::reconnect(g_cfg.common); }
    } else {
      agri::MQTT::loop();
      static uint32_t lastPub = 0;
      uint32_t interval = (uint32_t)g_cfg.common.mqtt_interval_s * 1000UL;
      if (now - lastPub >= interval) {
        lastPub = now;
        if (mqttPublishFlow()) agri::Led::flashPublish();
      }
    }
  }

  if (agri::networkUp() && g_cfg.common.ccm_enabled) {
    static uint32_t lastCcm = 0;
    uint32_t interval = (uint32_t)g_cfg.common.ccm_interval_s * 1000UL;
    if (now - lastCcm >= interval) {
      lastCcm = now;
      if (ccmPublish()) agri::Led::flashPublish();
    }
  }

  agri::LedState desired;
  if (!agri::networkUp())                                                desired = agri::LED_NO_LINK;
  else if (agri::MQTT::hasHost(g_cfg.common) && !agri::MQTT::connected()) desired = agri::LED_NO_MQTT;
  else                                                                   desired = agri::LED_OK;
  agri::Led::set(desired);

  static uint32_t lastStatus = 0;
  if (now - lastStatus >= 30000) {
    lastStatus = now;
    Serial.printf("[STATUS] link=%d lease=%d mqtt=%d  flow=%.3f L/min  vol=%.3f L  pulses=%lu  up=%lus\n",
                  agri::Network::link_up, agri::Network::have_lease,
                  agri::MQTT::connected(),
                  g_flow_lpm, volumeLiters(),
                  (unsigned long)g_cum_pulses,
                  (unsigned long)(now / 1000));
  }

  delay(20);
}
