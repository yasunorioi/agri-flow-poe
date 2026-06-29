// ccm_pub.h — dual-house flow (WaterFlow + WaterCons) UECS-CCM publisher.
//
// ch0→house2, ch1→house3: each channel carries its OWN room/region/order so
// the central / pi4 bridge routes it to the right house. CCM type names
// (CCM識別子) default to WaterFlow/WaterCons (ccm_rp2350_relay / OGMS
// convention) but are configurable from /config — empty = that datum off.
// Default OFF — enable per house only after the region matches the bridge's
// region→house map.
//
// ONE <DATA> per packet. UECS permits several DATA per envelope, but ArSprout's
// receiver only keeps ONE datum per packet (a multi-datum envelope silently
// drops all but one — confirmed 2026-06-21 with agri-amp-wifi: a 2-datum packet
// lost the first datum until split). So each of the four datums gets its own
// envelope, matching every other node on the wire.

#pragma once

#include <Arduino.h>
#include <AgriNode.h>
#include "config.h"
#include "sensors.h"

inline bool ccmPublish() {
  if (!g_cfg.common.ccm_enabled) return false;

  char buf[16];
  bool any = false;

  // Wrap a single datum in its own envelope and send it as one packet.
  // Empty CCM識別子 (type name) = this datum disabled.
  auto sendDatum = [&](const char *type, int room, int region, int order,
                       const char *val) {
    if (!type || !type[0]) return;
    String xml = agri::ccmEnvelopeOpen();
    xml += agri::ccmDatumNT(type, g_cfg.common.ccm_ntype, room, region, order,
                            g_cfg.common.ccm_priority, val);
    xml += agri::ccmEnvelopeClose();
    any |= agri::ccmSend(xml);
  };

  for (int i = 0; i < NUM_FLOW; i++) {
    dtostrf(g_flow_lpm[i], 1, 3, buf);
    sendDatum(g_cfg.ccm_type_flow, g_cfg.ccm_room[i], g_cfg.ccm_region[i],
              g_cfg.ccm_order_flow[i], buf);

    dtostrf(volumeLiters(i), 1, 3, buf);
    sendDatum(g_cfg.ccm_type_cons, g_cfg.ccm_room[i], g_cfg.ccm_region[i],
              g_cfg.ccm_order_cons[i], buf);
  }

  return any;
}
