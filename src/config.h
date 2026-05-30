// config.h — agri-flow-poe NVS-backed config.
//
// Wraps the library's CommonConfig with two CCM channels (instantaneous
// flow rate + cumulative consumption) and a calibration constant for the
// DIGITEN sensor.

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <AgriCommonConfig.h>

struct AppConfig {
  agri::CommonConfig common;
  uint16_t           pulses_per_liter;   // sensor-specific (DIGITEN spec)
  int16_t            ccm_order_flow;     // WaterFlow.cMC  (L/min)
  int16_t            ccm_order_cons;     // WaterCons.cMC  (L cumulative)
};

extern AppConfig g_cfg;

inline void setDefaults() {
  // ccm_rp2350_relay convention: sensor_region 11; irrigation/water gear
  // sits at +20 → 31. Re-tune from /config if your central uses something
  // else.
  agri::commonDefaults(g_cfg.common,
                       "flow_node_01", "agri-flow-01",
                       "agriha/h01/sensor/Flow",
                       /*default_ccm_region=*/31);
  g_cfg.pulses_per_liter = 450;   // YF-S201-class default
  g_cfg.ccm_order_flow   = 1;
  g_cfg.ccm_order_cons   = 1;
}

inline void loadConfig() {
  setDefaults();
  Preferences p;
  if (!p.begin("flow-cfg", true)) return;
  agri::commonLoad(g_cfg.common, p);
  g_cfg.pulses_per_liter = p.getUShort("ppl",     g_cfg.pulses_per_liter);
  g_cfg.ccm_order_flow   = p.getShort ("ccm_ofl", g_cfg.ccm_order_flow);
  g_cfg.ccm_order_cons   = p.getShort ("ccm_oco", g_cfg.ccm_order_cons);
  p.end();
}

inline bool saveConfig() {
  Preferences p;
  if (!p.begin("flow-cfg", false)) return false;
  agri::commonSave(g_cfg.common, p);
  p.putUShort("ppl",     g_cfg.pulses_per_liter);
  p.putShort ("ccm_ofl", g_cfg.ccm_order_flow);
  p.putShort ("ccm_oco", g_cfg.ccm_order_cons);
  p.end();
  return true;
}
