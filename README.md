# agri-flow-poe

M5Stack ATOM PoE Kit + DIGITEN hall-effect flow meter → MQTT + UECS-CCM。
[agri-node-poe-core](https://github.com/yasunorioi/agri-node-poe-core)
ライブラリの上に薄く乗っているだけのスケッチで、本体は雛形の繰り返し。

## ハードウェア

- **MCU**: M5Stack ATOM Lite (ESP32-PICO-D4)
- **PoE / Ethernet**: M5Stack ATOM PoE Base (W5500 on SPI)
- **センサー**: DIGITEN 系ホール式流量計（[shop](https://www.digiten.shop/collections/counter)）
  - 信号: Grove G26 (FALLING-edge ISR でカウント)
  - 電源: Grove 5V または 3.3V（センサ仕様による）

## 設定（NVS 永続化）

`Preferences` ネームスペース `flow-cfg`。Web UI の `/config` から編集:

- **共通**: Node ID, hostname, MQTT host/port/user/pass/topic prefix/interval,
  UECS-CCM enable/interval/room/region/priority
- **流量センサ固有**:
  - `pulses_per_liter` — センサ仕様に合わせて (YF-S201 系で 450 がデフォルト)
  - `Order (Flow)` / `Order (Cumulative)` — CCM 二チャネルの order

## 配信

| 出力 | 内容 |
|---|---|
| MQTT `<prefix>` | JSON: `flow_lpm`, `volume_l`, `raw_pulses`, `node_id`, `uptime_s` |
| CCM `WaterFlow.cMC` | 瞬時流量 (L/min) |
| CCM `WaterCons.cMC` | 累積流量 (L、リブートでリセット or Web UI でリセット) |

## ビルド / 焼き込み

```bash
pio run -e m5atom-poe -t upload                                       # USB-C
pio run -e m5atom-poe -t upload --upload-port agri-flow-01.local      # OTA
```

## 関連プロジェクト

- [agri-node-poe-core](https://github.com/yasunorioi/agri-node-poe-core) — 共通ライブラリ
- [agri-rain-poe](https://github.com/yasunorioi/agri-rain-poe) — 雨量
- [agri-env-poe](https://github.com/yasunorioi/agri-env-poe) — 温湿度 + 気圧 + CO₂
- [OGMS](https://github.com/yasunorioi/OGMS) — メイン制御
- [ccm_rp2350_relay](https://github.com/yasunorioi/ccm_rp2350_relay) — リレー
