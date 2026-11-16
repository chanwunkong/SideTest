# FORA 拍照 AI 判讀（MQTT）

照片拍好或選好後，網頁會**自動**送 AI 判讀，不用多按任何按鈕，結果直接顯示在照片下方。上傳時結果會一起放進 `ai_intake` / `ai_output` / `ai_wound` 欄位，這是預留給醫院 API 的接口；目前 `DEMO_MODE` 還是模擬上傳。

```
手機 fora.html ──wss──> mosquitto（家用電腦）<──1883── ai_worker.py ×N ──> 視覺模型（Ollama / vLLM）
  發 fora/ai/req/{單號}                          共享訂閱自動分流
  收 fora/ai/res/{單號} <─────────────────────── 回傳判讀文字
```

- 送出的資料**不含病歷號**，只有隨機單號、照片類型和傷口部位。照片會先縮到長邊 1024px。
- worker 只在記憶體裡處理照片，不存檔。
- AI 主機沒開的話，等 60 秒會顯示「AI 判讀失敗」，照片照樣可以上傳。

## 1. 家用電腦：broker

```sh
brew install mosquitto            # Linux：apt install mosquitto
cd fora/mosquitto
mosquitto_passwd -c passwd fora-web      # 網頁用（密碼會寫在 fora.html，視為公開）
mosquitto_passwd passwd fora-worker      # worker 用
chmod 700 passwd acl
mosquitto -c mosquitto.conf              # 要在這個資料夾裡執行
```

`acl` 規定網頁帳號只能送請求、收結果，就算密碼外流也冒充不了 AI 回傳結果。

## 2. 對外：Cloudflare Tunnel（不用開路由器 port，自動有 https）

```sh
brew install cloudflared
cloudflared tunnel login
cloudflared tunnel create fora-mqtt
cloudflared tunnel route dns fora-mqtt mqtt.你的網域
cloudflared tunnel run --url http://localhost:9001 fora-mqtt
```

然後把 `fora.html` 裡 `CONFIG.AI` 的 `MQTT_URL` 改成 `wss://mqtt.你的網域`，`PASSWORD` 填 fora-web 的密碼。

## 3. 模型 + worker（家用電腦或叢集每台各跑一份）

```sh
brew install ollama && ollama pull qwen2.5vl:7b    # 或 gemma3:12b、llava 等視覺模型
pip install paho-mqtt
MQTT_HOST=家用電腦IP MQTT_USER=fora-worker MQTT_PASS=xxx python3 fora/ai_worker.py
```

| 環境變數 | 預設 | 說明 |
|---|---|---|
| `MQTT_HOST` / `MQTT_PORT` | localhost / 1883 | 叢集機器連家用電腦要把 `mosquitto.conf` 的 `listener 1883` 改成內網 IP |
| `MQTT_TLS` | 不開 | 設 `1` 走 TLS |
| `LLM_URL` | Ollama 本機 | 任何 OpenAI 相容的 `/v1/chat/completions`，例如叢集上的 vLLM |
| `LLM_MODEL` | `qwen2.5vl:7b` | |
| `LLM_KEY` | 空 | 需要 Bearer token 時填 |
| `WORKERS` | 1 | 單台同時處理幾張 |

叢集要擴充，多開幾台 worker 就好，broker 會自動輪流分配，不用改設定。

提示詞放在 `ai_worker.py` 的 `PROMPTS`，分攝入、排出、傷口三種，要調整直接改那裡。
