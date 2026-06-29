// config.h — agri-flow-poe NVS-backed config.
//
// Dual-channel flow node where the two lines belong to DIFFERENT houses:
//   ch0 = G26 → house 2,  ch1 = G32 → house 3.
// Each channel is therefore an independent sensor with its own MQTT topic
// and its own CCM room/region/order. Only the pulse calibration is shared
// (same sensor model). The CommonConfig's single mqtt_topic_prefix is used
// for the node-level LWT / sys-online scope, NOT for the data topics.

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <AgriCommonConfig.h>

static const int CFG_NUM_CH = 2;

struct AppConfig {
  agri::CommonConfig common;                 // conn + LWT scope + ccm enable/interval/priority/ntype
  uint16_t           pulses_per_liter;        // shared by both channels (same DIGITEN model)
  char               topic[CFG_NUM_CH][64];   // per-channel MQTT publish topic (house-scoped)
  int16_t            ccm_room[CFG_NUM_CH];     // per-channel CCM room
  int16_t            ccm_region[CFG_NUM_CH];   // per-channel CCM region (maps to house on the bridge)
  int16_t            ccm_order_flow[CFG_NUM_CH];
  int16_t            ccm_order_cons[CFG_NUM_CH];
  // CCM識別子 = the UECS <DATA type=...> wire names. Per-sensor, so they live
  // here (project config), not in core's envelope. Shared across both channels
  // (the channels differ by region/order, not type name). Empty = datum off.
  char               ccm_type_flow[16];   // instantaneous flow rate (WaterFlow)
  char               ccm_type_cons[16];   // cumulative volume     (WaterCons)
};

extern AppConfig g_cfg;

inline void setDefaults() {
  // Common prefix here is only the node sys/LWT scope (<prefix>/sys/<id>/online).
  // Data goes to the per-channel topics below.
  agri::commonDefaults(g_cfg.common,
                       "flow_node_01", "agri-flow-01",
                       "agriha/2",                 // node sys/LWT home scope
                       /*default_ccm_region=*/31);
  // DIGITEN G3/4 hall sensor (1-60 L/min): spec is F = 5.5*Q [Hz, L/min],
  // i.e. 5.5 pulses/s per L/min → 5.5*60 = 330 pulses/L. Shared by both ch.
  g_cfg.pulses_per_liter = 330;   // DIGITEN G3/4 (F=5.5*Q), shared

  // ch0 = G26 → house 2,  ch1 = G32 → house 3.
  strlcpy(g_cfg.topic[0], "agriha/2/sensor/Flow", sizeof(g_cfg.topic[0]));
  strlcpy(g_cfg.topic[1], "agriha/3/sensor/Flow", sizeof(g_cfg.topic[1]));

  // CCM (default OFF). region per channel must match the pi4 bridge's
  // region→house map before enabling — wrong region routes to the wrong
  // house (see the .165 mis-map incident). These are placeholders; set the
  // real per-house regions from /config.
  g_cfg.ccm_room[0]   = 1;  g_cfg.ccm_room[1]   = 1;
  g_cfg.ccm_region[0] = 31; g_cfg.ccm_region[1] = 32;
  g_cfg.ccm_order_flow[0] = 1; g_cfg.ccm_order_flow[1] = 1;
  g_cfg.ccm_order_cons[0] = 1; g_cfg.ccm_order_cons[1] = 1;

  // CCM識別子 default to the established WaterFlow/WaterCons wire names
  // (ccm_rp2350_relay / OGMS convention). Editing them does not require a
  // re-flash — set to match the receive CCM configured in ArSprout.
  strlcpy(g_cfg.ccm_type_flow, "WaterFlow", sizeof(g_cfg.ccm_type_flow));
  strlcpy(g_cfg.ccm_type_cons, "WaterCons", sizeof(g_cfg.ccm_type_cons));
}

inline void loadConfig() {
  setDefaults();
  Preferences p;
  if (!p.begin("flow-cfg", true)) return;
  agri::commonLoad(g_cfg.common, p);
  g_cfg.pulses_per_liter = p.getUShort("ppl", g_cfg.pulses_per_liter);
  { String t;
    t = p.getString("topic0", g_cfg.topic[0]); strlcpy(g_cfg.topic[0], t.c_str(), sizeof(g_cfg.topic[0]));
    t = p.getString("topic1", g_cfg.topic[1]); strlcpy(g_cfg.topic[1], t.c_str(), sizeof(g_cfg.topic[1]));
  }
  g_cfg.ccm_room[0]       = p.getShort("ccm_rm0", g_cfg.ccm_room[0]);
  g_cfg.ccm_room[1]       = p.getShort("ccm_rm1", g_cfg.ccm_room[1]);
  g_cfg.ccm_region[0]     = p.getShort("ccm_rg0", g_cfg.ccm_region[0]);
  g_cfg.ccm_region[1]     = p.getShort("ccm_rg1", g_cfg.ccm_region[1]);
  g_cfg.ccm_order_flow[0] = p.getShort("ccm_of0", g_cfg.ccm_order_flow[0]);
  g_cfg.ccm_order_flow[1] = p.getShort("ccm_of1", g_cfg.ccm_order_flow[1]);
  g_cfg.ccm_order_cons[0] = p.getShort("ccm_oc0", g_cfg.ccm_order_cons[0]);
  g_cfg.ccm_order_cons[1] = p.getShort("ccm_oc1", g_cfg.ccm_order_cons[1]);
  // String-default overload: a pre-existing NVS without these keys keeps the
  // setDefaults() value instead of being wiped to "" (same guard as ccm_nt).
  auto loadType = [&](const char *key, char *dst, size_t n) {
    String v = p.getString(key, dst);
    strlcpy(dst, v.c_str(), n);
  };
  loadType("ct_flow", g_cfg.ccm_type_flow, sizeof(g_cfg.ccm_type_flow));
  loadType("ct_cons", g_cfg.ccm_type_cons, sizeof(g_cfg.ccm_type_cons));
  p.end();
}

inline bool saveConfig() {
  Preferences p;
  if (!p.begin("flow-cfg", false)) return false;
  agri::commonSave(g_cfg.common, p);
  p.putUShort("ppl",    g_cfg.pulses_per_liter);
  p.putString("topic0", g_cfg.topic[0]);
  p.putString("topic1", g_cfg.topic[1]);
  p.putShort ("ccm_rm0", g_cfg.ccm_room[0]);
  p.putShort ("ccm_rm1", g_cfg.ccm_room[1]);
  p.putShort ("ccm_rg0", g_cfg.ccm_region[0]);
  p.putShort ("ccm_rg1", g_cfg.ccm_region[1]);
  p.putShort ("ccm_of0", g_cfg.ccm_order_flow[0]);
  p.putShort ("ccm_of1", g_cfg.ccm_order_flow[1]);
  p.putShort ("ccm_oc0", g_cfg.ccm_order_cons[0]);
  p.putShort ("ccm_oc1", g_cfg.ccm_order_cons[1]);
  p.putString("ct_flow", g_cfg.ccm_type_flow);
  p.putString("ct_cons", g_cfg.ccm_type_cons);
  p.end();
  return true;
}
