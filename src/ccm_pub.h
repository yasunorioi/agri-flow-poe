// ccm_pub.h — flow (WaterFlow + WaterCons) UECS-CCM publisher.
//
// CCM type names follow the convention used by ccm_rp2350_relay / OGMS.
// Adjust the literal strings here if your central uses different ones.

#pragma once

#include <Arduino.h>
#include <AgriNode.h>
#include "config.h"
#include "sensors.h"

inline bool ccmPublish() {
  if (!g_cfg.common.ccm_enabled) return false;

  char buf[16];
  String xml = agri::ccmEnvelopeOpen();

  dtostrf(g_flow_lpm, 1, 3, buf);
  xml += agri::ccmDatum("WaterFlow",
                        g_cfg.common.ccm_room, g_cfg.common.ccm_region,
                        g_cfg.ccm_order_flow, g_cfg.common.ccm_priority, buf);

  dtostrf(volumeLiters(), 1, 3, buf);
  xml += agri::ccmDatum("WaterCons",
                        g_cfg.common.ccm_room, g_cfg.common.ccm_region,
                        g_cfg.ccm_order_cons, g_cfg.common.ccm_priority, buf);

  xml += agri::ccmEnvelopeClose();
  return agri::ccmSend(xml);
}
