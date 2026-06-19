// mqtt_pub.h — dual-line flow JSON payload to the configured topic prefix.

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AgriNode.h>
#include "config.h"
#include "sensors.h"

inline bool mqttPublishFlow() {
  if (!agri::MQTT::hasHost(g_cfg.common) || !agri::MQTT::connected()) return false;

  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["node_id"]     = g_cfg.common.node_id;
  root["uptime_s"]    = millis() / 1000;
  root["flow1_lpm"]   = g_flow_lpm[0];
  root["volume1_l"]   = volumeLiters(0);
  root["raw_pulses1"] = g_cum_pulses[0];
  root["flow2_lpm"]   = g_flow_lpm[1];
  root["volume2_l"]   = volumeLiters(1);
  root["raw_pulses2"] = g_cum_pulses[1];

  char payload[256];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  bool ok = agri::MQTT::mqtt.publish(g_cfg.common.mqtt_topic_prefix,
                                     (const uint8_t*)payload, n, true);
  Serial.printf("[MQTT] %s %s (%u bytes)\n",
                g_cfg.common.mqtt_topic_prefix, ok ? "OK" : "FAIL",
                (unsigned)n);
  return ok;
}
