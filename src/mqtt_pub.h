// mqtt_pub.h — per-channel flow JSON to each channel's own house topic.
//
// The two lines live in different houses (ch0→house2, ch1→house3), so each
// channel publishes a single-channel blob to its own configured topic.

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AgriNode.h>
#include "config.h"
#include "sensors.h"

inline bool mqttPublishChannel(int ch) {
  if (!agri::MQTT::hasHost(g_cfg.common) || !agri::MQTT::connected()) return false;
  if (g_cfg.topic[ch][0] == '\0') return false;   // unconfigured topic → skip

  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["node_id"]    = g_cfg.common.node_id;
  root["channel"]    = ch;
  root["uptime_s"]   = millis() / 1000;
  root["flow_lpm"]   = g_flow_lpm[ch];
  root["volume_l"]   = volumeLiters(ch);
  root["raw_pulses"] = g_cum_pulses[ch];

  char payload[256];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  bool ok = agri::MQTT::mqtt.publish(g_cfg.topic[ch], (const uint8_t*)payload, n, true);
  Serial.printf("[MQTT] ch%d %s %s (%u bytes)\n",
                ch, g_cfg.topic[ch], ok ? "OK" : "FAIL", (unsigned)n);
  return ok;
}

inline bool mqttPublishFlow() {
  bool any = false;
  for (int i = 0; i < NUM_FLOW; i++) any |= mqttPublishChannel(i);
  return any;
}
