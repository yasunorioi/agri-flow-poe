// ccm_pub.h — dual-line flow (WaterFlow + WaterCons) UECS-CCM publisher.
//
// Emits both irrigation lines in one CCM envelope: WaterFlow/WaterCons for
// ch0 and ch1, same type/room/region, distinguished by per-channel order.
// CCM type names follow the ccm_rp2350_relay / OGMS convention.

#pragma once

#include <Arduino.h>
#include <AgriNode.h>
#include "config.h"
#include "sensors.h"

inline bool ccmPublish() {
  if (!g_cfg.common.ccm_enabled) return false;

  char buf[16];
  String xml = agri::ccmEnvelopeOpen();

  for (int i = 0; i < NUM_FLOW; i++) {
    dtostrf(g_flow_lpm[i], 1, 3, buf);
    xml += agri::ccmDatumNT("WaterFlow", g_cfg.common.ccm_ntype,
                          g_cfg.common.ccm_room, g_cfg.common.ccm_region,
                          g_cfg.ccm_order_flow[i], g_cfg.common.ccm_priority, buf);

    dtostrf(volumeLiters(i), 1, 3, buf);
    xml += agri::ccmDatumNT("WaterCons", g_cfg.common.ccm_ntype,
                          g_cfg.common.ccm_room, g_cfg.common.ccm_region,
                          g_cfg.ccm_order_cons[i], g_cfg.common.ccm_priority, buf);
  }

  xml += agri::ccmEnvelopeClose();
  return agri::ccmSend(xml);
}
