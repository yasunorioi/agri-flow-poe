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
const char *FW_VERSION  = "0.6.0";
const char *FW_REPO     = "yasunorioi/agri-flow-poe";
const char *FW_BIN_NAME = "agri-flow-poe.bin";

AppConfig g_cfg;
volatile uint32_t g_pulse_count[NUM_FLOW]        = {0, 0};
uint32_t          g_cum_pulses[NUM_FLOW]         = {0, 0};
uint32_t          g_last_window_ms               = 0;
uint32_t          g_last_window_pulses[NUM_FLOW] = {0, 0};
float             g_flow_lpm[NUM_FLOW]           = {0.0f, 0.0f};

// Flow ISRs (one per channel). Defined here (not in sensors.h) so the
// IRAM_ATTR placement resolves against a DRAM-located literal pool —
// keeping them inline in a header trips a "dangerous relocation: l32r"
// link error on ESP32.
void IRAM_ATTR onFlowPulse0() { g_pulse_count[0]++; }
void IRAM_ATTR onFlowPulse1() { g_pulse_count[1]++; }

static String renderDashboardSensors() {
  String s; s.reserve(600);
  char buf[12];
  s = F("<h3>Flow</h3><table>"
        "<tr><th></th><th>ch0 (G26)</th><th>ch1 (G32)</th></tr>");

  s += F("<tr><th>Topic</th>");
  for (int i = 0; i < NUM_FLOW; i++) { s += "<td>"; s += g_cfg.topic[i]; s += "</td>"; }
  s += F("</tr>");

  s += F("<tr><th>Flow rate</th>");
  for (int i = 0; i < NUM_FLOW; i++) {
    dtostrf(g_flow_lpm[i], 1, 3, buf);
    s += "<td>"; s += buf; s += " L/min</td>";
  }
  s += F("</tr>");

  s += F("<tr><th>Total volume</th>");
  for (int i = 0; i < NUM_FLOW; i++) {
    dtostrf(volumeLiters(i), 1, 3, buf);
    s += "<td>"; s += buf; s += " L</td>";
  }
  s += F("</tr>");

  s += F("<tr><th>Raw pulses</th>");
  for (int i = 0; i < NUM_FLOW; i++) { s += "<td>"; s += g_cum_pulses[i]; s += "</td>"; }
  s += F("</tr>");

  s += "<tr><th>Pulses / L</th><td colspan=2>"; s += g_cfg.pulses_per_liter;
  s += F(" (shared, configurable)</td></tr>"
         "</table>"
         "<p><small>Reset cumulative volume from the Config page.</small></p>");
  return s;
}

static String renderConfigSensorRows() {
  String s;
  auto row = [&](const char *label, const String &input) {
    s += "<tr><th>"; s += label; s += "</th><td>"; s += input; s += "</td></tr>";
  };
  auto num = [](const char *name, int v) {
    return "<input type=number name=" + String(name) + " value='" + String(v) + "'>";
  };
  auto txt = [](const char *name, const char *v) {
    return "<input name=" + String(name) + " value='" + String(v) + "'>";
  };

  s += F("<tr><th colspan=2><small>Note: the common Topic/Prefix above is the "
         "node sys/LWT scope; the common CCM room/region are unused. Data + CCM "
         "are per-channel below (ch0=G26→house2, ch1=G32→house3).</small></th></tr>");

  row("Pulses per litre (shared)", num("ppl", g_cfg.pulses_per_liter));

  row("MQTT topic (ch0 → house2)", txt("topic0", g_cfg.topic[0]));
  row("MQTT topic (ch1 → house3)", txt("topic1", g_cfg.topic[1]));

  row("CCM room / region (ch0)",
      num("ccm_rm0", g_cfg.ccm_room[0]) + " / " + num("ccm_rg0", g_cfg.ccm_region[0]));
  row("CCM room / region (ch1)",
      num("ccm_rm1", g_cfg.ccm_room[1]) + " / " + num("ccm_rg1", g_cfg.ccm_region[1]));

  row("Order Flow / Cons (ch0)",
      num("ccm_of0", g_cfg.ccm_order_flow[0]) + " / " + num("ccm_oc0", g_cfg.ccm_order_cons[0]));
  row("Order Flow / Cons (ch1)",
      num("ccm_of1", g_cfg.ccm_order_flow[1]) + " / " + num("ccm_oc1", g_cfg.ccm_order_cons[1]));

  // CCM識別子 (UECS <DATA type> names), shared by both channels. Empty = off.
  row("CCM識別子: 流量 (WaterFlow)", txt("ct_flow", g_cfg.ccm_type_flow));
  row("CCM識別子: 積算 (WaterCons)", txt("ct_cons", g_cfg.ccm_type_cons));

  // Reset is a config-form action (core has no /reset route). Tick + Save.
  row("Reset cumulative volume",
      "<label><input type=checkbox name=flow_reset value=1> zero both channels on Save</label>");
  return s;
}

static void applyConfigSensorForm(const String &body) {
  g_cfg.pulses_per_liter  = (uint16_t)agri::parseFormInt(body, "ppl", g_cfg.pulses_per_liter);
  agri::parseFormStr(body, "topic0", g_cfg.topic[0], sizeof(g_cfg.topic[0]));
  agri::parseFormStr(body, "topic1", g_cfg.topic[1], sizeof(g_cfg.topic[1]));
  g_cfg.ccm_room[0]       = (int16_t) agri::parseFormInt(body, "ccm_rm0", g_cfg.ccm_room[0]);
  g_cfg.ccm_room[1]       = (int16_t) agri::parseFormInt(body, "ccm_rm1", g_cfg.ccm_room[1]);
  g_cfg.ccm_region[0]     = (int16_t) agri::parseFormInt(body, "ccm_rg0", g_cfg.ccm_region[0]);
  g_cfg.ccm_region[1]     = (int16_t) agri::parseFormInt(body, "ccm_rg1", g_cfg.ccm_region[1]);
  g_cfg.ccm_order_flow[0] = (int16_t) agri::parseFormInt(body, "ccm_of0", g_cfg.ccm_order_flow[0]);
  g_cfg.ccm_order_flow[1] = (int16_t) agri::parseFormInt(body, "ccm_of1", g_cfg.ccm_order_flow[1]);
  g_cfg.ccm_order_cons[0] = (int16_t) agri::parseFormInt(body, "ccm_oc0", g_cfg.ccm_order_cons[0]);
  g_cfg.ccm_order_cons[1] = (int16_t) agri::parseFormInt(body, "ccm_oc1", g_cfg.ccm_order_cons[1]);
  agri::parseFormStr(body, "ct_flow", g_cfg.ccm_type_flow, sizeof(g_cfg.ccm_type_flow));
  agri::parseFormStr(body, "ct_cons", g_cfg.ccm_type_cons, sizeof(g_cfg.ccm_type_cons));
  if (agri::parseFormInt(body, "flow_reset", 0)) resetVolume();
}

static void addStatusFields(JsonObject doc) {
  doc["flow1_lpm"]   = g_flow_lpm[0];
  doc["volume1_l"]   = volumeLiters(0);
  doc["raw_pulses1"] = g_cum_pulses[0];
  doc["flow2_lpm"]   = g_flow_lpm[1];
  doc["volume2_l"]   = volumeLiters(1);
  doc["raw_pulses2"] = g_cum_pulses[1];
  // CCM識別子 (wire type names) — exposed for inspection / verification.
  JsonObject ct = doc["ccm_types"].to<JsonObject>();
  ct["flow"] = g_cfg.ccm_type_flow;
  ct["cons"] = g_cfg.ccm_type_cons;
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
    Serial.printf("[STATUS] link=%d lease=%d mqtt=%d  ch0=%.3f L/min/%.3f L  ch1=%.3f L/min/%.3f L  pulses=%lu/%lu  up=%lus\n",
                  agri::Network::link_up, agri::Network::have_lease,
                  agri::MQTT::connected(),
                  g_flow_lpm[0], volumeLiters(0),
                  g_flow_lpm[1], volumeLiters(1),
                  (unsigned long)g_cum_pulses[0], (unsigned long)g_cum_pulses[1],
                  (unsigned long)(now / 1000));
  }

  delay(20);
}
