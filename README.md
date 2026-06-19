# agri-flow-poe

M5Stack ATOM PoE Kit + DIGITEN ホール式流量計 **2台（同種・2系統の灌水）** → MQTT + UECS-CCM。
[agri-node-poe-core](https://github.com/yasunorioi/agri-node-poe-core)
ライブラリの上に薄く乗っているだけのスケッチで、本体は雛形の繰り返し。

## ハードウェア

- **MCU**: M5Stack ATOM Lite (ESP32-PICO-D4)
- **PoE / Ethernet**: M5Stack ATOM PoE Base (W5500 on SPI)
- **センサー**: DIGITEN 系ホール式流量計 **×2**（[shop](https://www.digiten.shop/collections/counter)）
  - 信号: Grove **G26 (ch0)** / **G32 (ch1)** — それぞれ FALLING-edge ISR でカウント。
    どちらも M5 ATOM の Grove コネクタに出ているので 1ポートに2台を 5V/GND 共有で結線可能。
  - 電源: Grove 5V または 3.3V（センサ仕様による）
  - 校正定数 `pulses_per_liter` は両 ch 共通（同一型番前提）

## 設定（NVS 永続化）

`Preferences` ネームスペース `flow-cfg`。Web UI の `/config` から編集:

- **共通**: Node ID, hostname, MQTT host/port/user/pass/topic prefix/interval,
  UECS-CCM enable/interval/room/region/priority
- **流量センサ固有**:
  - `pulses_per_liter` — センサ仕様に合わせて (YF-S201 系で 450 がデフォルト、両 ch 共通)
  - `Order (Flow ch0/ch1)` / `Order (Cumulative ch0/ch1)` — CCM の order。
    型・room・region は両 ch 共通で、ch0/ch1 は **order** で区別する（既定 1 / 2）
  - `Reset cumulative volume` — チェックして Save すると両 ch の積算をゼロ化
    （core に `/reset` ルートが無いため、リセットは Config フォームのアクション）

## 配信

| 出力 | 内容 |
|---|---|
| MQTT `<prefix>` | JSON: `flow1_lpm`/`volume1_l`/`raw_pulses1` (ch0), `flow2_lpm`/`volume2_l`/`raw_pulses2` (ch1), `node_id`, `uptime_s` |
| CCM `WaterFlow.cMC` ×2 | 瞬時流量 (L/min)、ch0/ch1 を order で区別 |
| CCM `WaterCons.cMC` ×2 | 累積流量 (L、リブートでリセット or Config でリセット)、ch0/ch1 を order で区別 |

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
