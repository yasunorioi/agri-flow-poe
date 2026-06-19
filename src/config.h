// config.h — agri-flow-poe NVS-backed config.
//
// Wraps the library's CommonConfig with the dual-channel flow sensor:
// two same-model irrigation lines (ch0 on G26, ch1 on G32) share one
// pulses_per_liter calibration constant. Each channel still gets its own
// CCM order so a central can tell the two WaterFlow/WaterCons instances
// apart (same type/room/region, distinct order).

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <AgriCommonConfig.h>

struct AppConfig {
  agri::CommonConfig common;
  uint16_t           pulses_per_liter;   // shared by both channels (DIGITEN spec)
  int16_t            ccm_order_flow[2];   // WaterFlow.cMC order, ch0/ch1 (L/min)
  int16_t            ccm_order_cons[2];   // WaterCons.cMC order, ch0/ch1 (L cumulative)
};

extern AppConfig g_cfg;

inline void setDefaults() {
  // ccm_rp2350_relay convention: sensor_region 11; irrigation/water gear
  // sits at +20 → 31. Re-tune from /config if your central uses something
  // else. Both lines share region; the order disambiguates ch0 vs ch1.
  agri::commonDefaults(g_cfg.common,
                       "flow_node_01", "agri-flow-01",
                       "agriha/h01/sensor/Flow",
                       /*default_ccm_region=*/31);
  g_cfg.pulses_per_liter = 450;   // YF-S201-class default
  g_cfg.ccm_order_flow[0] = 1; g_cfg.ccm_order_flow[1] = 2;
  g_cfg.ccm_order_cons[0] = 1; g_cfg.ccm_order_cons[1] = 2;
}

inline void loadConfig() {
  setDefaults();
  Preferences p;
  if (!p.begin("flow-cfg", true)) return;
  agri::commonLoad(g_cfg.common, p);
  g_cfg.pulses_per_liter  = p.getUShort("ppl",      g_cfg.pulses_per_liter);
  g_cfg.ccm_order_flow[0] = p.getShort ("ccm_ofl1", g_cfg.ccm_order_flow[0]);
  g_cfg.ccm_order_flow[1] = p.getShort ("ccm_ofl2", g_cfg.ccm_order_flow[1]);
  g_cfg.ccm_order_cons[0] = p.getShort ("ccm_oco1", g_cfg.ccm_order_cons[0]);
  g_cfg.ccm_order_cons[1] = p.getShort ("ccm_oco2", g_cfg.ccm_order_cons[1]);
  p.end();
}

inline bool saveConfig() {
  Preferences p;
  if (!p.begin("flow-cfg", false)) return false;
  agri::commonSave(g_cfg.common, p);
  p.putUShort("ppl",      g_cfg.pulses_per_liter);
  p.putShort ("ccm_ofl1", g_cfg.ccm_order_flow[0]);
  p.putShort ("ccm_ofl2", g_cfg.ccm_order_flow[1]);
  p.putShort ("ccm_oco1", g_cfg.ccm_order_cons[0]);
  p.putShort ("ccm_oco2", g_cfg.ccm_order_cons[1]);
  p.end();
  return true;
}
