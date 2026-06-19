// ccm_pub.h — dual-house flow (WaterFlow + WaterCons) UECS-CCM publisher.
//
// ch0→house2, ch1→house3: each channel carries its OWN room/region/order so
// the central / pi4 bridge routes it to the right house. UECS datums are
// independent, so all four go in one envelope. CCM type names follow the
// ccm_rp2350_relay / OGMS convention. Default OFF — enable per house only
// after the region matches the bridge's region→house map.

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
                          g_cfg.ccm_room[i], g_cfg.ccm_region[i],
                          g_cfg.ccm_order_flow[i], g_cfg.common.ccm_priority, buf);

    dtostrf(volumeLiters(i), 1, 3, buf);
    xml += agri::ccmDatumNT("WaterCons", g_cfg.common.ccm_ntype,
                          g_cfg.ccm_room[i], g_cfg.ccm_region[i],
                          g_cfg.ccm_order_cons[i], g_cfg.common.ccm_priority, buf);
  }

  xml += agri::ccmEnvelopeClose();
  return agri::ccmSend(xml);
}
