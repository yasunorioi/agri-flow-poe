# agri-flow-poe

[🇯🇵 日本語](README_ja.md) · **English**

[M5Stack ATOM PoE Kit](https://docs.m5stack.com/en/atom/atom_poe) + [DIGITEN](https://www.digiten.shop/collections/counter)-family Hall-effect flow meters **×2 (two irrigation lines in separate greenhouses)** → MQTT + UECS-CCM.
A sketch that sits thinly on top of the
[agri-node-poe-core](https://github.com/yasunorioi/agri-node-poe-core)
library; the body is just repetition of the boilerplate.

**ch0 = G26 → house 2, ch1 = G32 → house 3.** Since the two lines are in separate greenhouses, each ch has
its own independent MQTT topic and CCM room/region/order (only calibration is shared).

## Hardware

- **MCU**: [M5Stack ATOM Lite](https://docs.m5stack.com/en/core/ATOM%20Lite) (ESP32-PICO-D4)
- **PoE / Ethernet**: [M5Stack ATOM PoE Base](https://docs.m5stack.com/en/atom/Atomic%20PoE%20Base) (W5500 on SPI)
- **Sensor**: DIGITEN-family Hall-effect flow meters **×2** ([shop](https://www.digiten.shop/collections/counter))
  - Signal: Grove **G26 (ch0→house2)** / **G32 (ch1→house3)** — each counted by a FALLING-edge ISR.
    Both are exposed on the M5 ATOM's Grove connector, so two units can be wired to a single port sharing 5V/GND.
  - Power: Grove 5V or 3.3V (depending on the sensor spec)
  - The calibration constant `pulses_per_liter` is shared by both ch (assuming the same model number)

## Configuration (NVS persistence)

`Preferences` namespace `flow-cfg`. Edit from the Web UI's `/config`:

- **Common**: Node ID, hostname, MQTT host/port/user/pass/interval.
  `Topic/Prefix` is **dedicated to the node liveness (LWT `<prefix>/sys/<id>/online`) scope only** (default `agriha/2`).
  The common CCM room/region are **unused** (the per-ch settings below are used).
- **Flow-sensor specific (per ch)**:
  - `pulses_per_liter` — match to the sensor spec (for DIGITEN G3/4, `F=5.5*Q` → 5.5×60=**330** is the default, shared by both ch)
  - `MQTT topic (ch0/ch1)` — the publish destination for each ch (default `agriha/2/sensor/Flow` / `agriha/3/sensor/Flow`, manually configurable)
  - `CCM room / region (ch0/ch1)` — greenhouse mapping. ⚠️ Align region with the pi4 bridge's region→house map
    before enabling CCM (a wrong region flows to the wrong greenhouse). CCM is OFF by default.
  - `Order Flow / Cons (ch0/ch1)` — CCM order
  - `Reset cumulative volume` — checking this and Saving zeroes the cumulative totals of both ch
    (since core has no `/reset` route, the reset is an action of the Config form)

## Publishing

| Output | Contents |
|---|---|
| MQTT (ch0 topic) | JSON: `flow_lpm`, `volume_l`, `raw_pulses`, `channel`(=0), `node_id`, `uptime_s` |
| MQTT (ch1 topic) | Same as above (`channel`=1). Each ch is retained to its own house topic as a single-ch blob |
| CCM `WaterFlow.cMC` ×2 | Instantaneous flow (L/min); ch0/ch1 distinguished by **room/region/order** (separate greenhouses) |
| CCM `WaterCons.cMC` ×2 | Cumulative flow (L, reset at reboot or via Config); ch0/ch1 distinguished by room/region/order |

> The running version is `curl http://<host>.local/api/status | jq .fw_version` (exposed in core 0.4.0+).

## Build / Flash

```bash
pio run -e m5atom-poe -t upload                                       # USB-C
pio run -e m5atom-poe -t upload --upload-port agri-flow-01.local      # OTA
```

> 🛠 **Build environment (shared for Windows / Linux) and first-time Linux setup (udev, etc.)** →
> [agri-node-poe-core/docs/cross-platform-build.md](https://github.com/yasunorioi/agri-node-poe-core/blob/main/docs/cross-platform-build.md)

## Related projects

- [agri-node-poe-core](https://github.com/yasunorioi/agri-node-poe-core) — common library
- [agri-rain-poe](https://github.com/yasunorioi/agri-rain-poe) — rainfall
- [agri-env-poe](https://github.com/yasunorioi/agri-env-poe) — temperature/humidity + pressure + CO₂
- [OGMS](https://github.com/yasunorioi/OGMS) — main control
- [ccm_rp2350_relay](https://github.com/yasunorioi/ccm_rp2350_relay) — relay
