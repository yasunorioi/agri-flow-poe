# agri-flow-poe

M5Stack ATOM PoE Kit + DIGITEN ホール式流量計 **2台（別ハウスの灌水2系統）** → MQTT + UECS-CCM。
[agri-node-poe-core](https://github.com/yasunorioi/agri-node-poe-core)
ライブラリの上に薄く乗っているだけのスケッチで、本体は雛形の繰り返し。

**ch0 = G26 → ハウス2、ch1 = G32 → ハウス3。** 2系統は別ハウスなので、各 ch は
それぞれ独立した MQTT トピックと CCM room/region/order を持つ（校正のみ共通）。

## ハードウェア

- **MCU**: M5Stack ATOM Lite (ESP32-PICO-D4)
- **PoE / Ethernet**: M5Stack ATOM PoE Base (W5500 on SPI)
- **センサー**: DIGITEN 系ホール式流量計 **×2**（[shop](https://www.digiten.shop/collections/counter)）
  - 信号: Grove **G26 (ch0→house2)** / **G32 (ch1→house3)** — それぞれ FALLING-edge ISR でカウント。
    どちらも M5 ATOM の Grove コネクタに出ているので 1ポートに2台を 5V/GND 共有で結線可能。
  - 電源: Grove 5V または 3.3V（センサ仕様による）
  - 校正定数 `pulses_per_liter` は両 ch 共通（同一型番前提）

## 設定（NVS 永続化）

`Preferences` ネームスペース `flow-cfg`。Web UI の `/config` から編集:

- **共通**: Node ID, hostname, MQTT host/port/user/pass/interval。
  `Topic/Prefix` は**ノード死活(LWT `<prefix>/sys/<id>/online`)スコープ専用**（既定 `agriha/2`）。
  共通の CCM room/region は**未使用**（下の ch 別設定を使う）。
- **流量センサ固有（ch 別）**:
  - `pulses_per_liter` — センサ仕様に合わせて (YF-S201 系で 450 がデフォルト、両 ch 共通)
  - `MQTT topic (ch0/ch1)` — 各 ch の publish 先（既定 `agriha/2/sensor/Flow` / `agriha/3/sensor/Flow`、手動設定可）
  - `CCM room / region (ch0/ch1)` — ハウス対応。⚠️ region は pi4 ブリッジの region→house マップに
    合わせてから CCM を有効化（誤 region は誤ハウスに流れる）。CCM は既定 OFF。
  - `Order Flow / Cons (ch0/ch1)` — CCM order
  - `Reset cumulative volume` — チェックして Save すると両 ch の積算をゼロ化
    （core に `/reset` ルートが無いため、リセットは Config フォームのアクション）

## 配信

| 出力 | 内容 |
|---|---|
| MQTT (ch0 topic) | JSON: `flow_lpm`, `volume_l`, `raw_pulses`, `channel`(=0), `node_id`, `uptime_s` |
| MQTT (ch1 topic) | 同上（`channel`=1）。各 ch を自分のハウストピックへ単一ch blob で retain |
| CCM `WaterFlow.cMC` ×2 | 瞬時流量 (L/min)、ch0/ch1 を **room/region/order** で区別（別ハウス） |
| CCM `WaterCons.cMC` ×2 | 累積流量 (L、リブート or Config でリセット)、ch0/ch1 を room/region/order で区別 |

> 稼働版数は `curl http://<host>.local/api/status | jq .fw_version`（core 0.4.0+ で公開）。

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
