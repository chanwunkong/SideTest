#!/usr/bin/env python3
"""FORA 拍照 AI 判讀 worker：從 MQTT 收照片，丟給 OpenAI 相容的視覺模型（Ollama / vLLM / LM Studio），結果回傳 MQTT。

家用電腦或叢集每台都跑一份；共享訂閱讓 broker 把請求輪流分給各台，加機器不用改設定。
設定都用環境變數，見 AI_README.md。需要：pip install paho-mqtt
"""
import json, os, ssl, sys, urllib.request
from concurrent.futures import ThreadPoolExecutor

import paho.mqtt.client as mqtt

E = os.environ.get
MQTT_HOST, MQTT_PORT = E("MQTT_HOST", "localhost"), int(E("MQTT_PORT", "1883"))
TOPIC = E("MQTT_TOPIC", "fora/ai")
LLM_URL = E("LLM_URL", "http://localhost:11434/v1/chat/completions")  # Ollama 預設
LLM_MODEL = E("LLM_MODEL", "qwen2.5vl:7b")
LLM_KEY = E("LLM_KEY", "")
WORKERS = int(E("WORKERS", "1"))  # 同時處理幾張；GPU 小就維持 1

PROMPTS = {
    "intake": "這是病人的飲食（攝入）照片。請列出看得到的食物與飲料，估計份量（碗/杯/毫升）與大約吃了幾成，最後一行給「攝入量估計：」。",
    "output": "這是病人的排出物照片（尿液、糞便、引流液或嘔吐物）。請判斷種類，描述顏色、性狀、估計量，指出需要注意的異常（如血色、黑便、混濁），最後一行給「是否建議通知醫護：是/否」。",
    "wound": "這是病人「{part}」部位的傷口照片。請描述傷口大小（估計公分）、顏色、紅腫、滲液、壞死組織、周圍皮膚狀況，給出壓傷/傷口分級建議，最後一行給「是否建議通知醫護：是/否」。",
}
SYSTEM = "你是協助護理師的醫療影像輔助判讀助手。用繁體中文、條列、簡短回答（150 字內）。看不清楚就直說，不要猜測。你的結果僅供參考。"


def judge(req):
    prompt = PROMPTS[req["type"]].format(part=req.get("part") or "未註明")
    body = {
        "model": LLM_MODEL,
        "temperature": 0.2,
        "messages": [
            {"role": "system", "content": SYSTEM},
            {"role": "user", "content": [
                {"type": "text", "text": prompt},
                {"type": "image_url", "image_url": {"url": "data:image/jpeg;base64," + req["image"]}},
            ]},
        ],
    }
    headers = {"Content-Type": "application/json"}
    if LLM_KEY:
        headers["Authorization"] = "Bearer " + LLM_KEY
    r = urllib.request.Request(LLM_URL, json.dumps(body).encode(), headers)
    with urllib.request.urlopen(r, timeout=120) as resp:
        out = json.load(resp)
    return out["choices"][0]["message"]["content"].strip()


def handle(client, payload):
    rid = None
    try:
        req = json.loads(payload)
        rid = str(req["id"])
        if not rid.isalnum() or len(rid) > 64:  # 單號會拼進 topic，擋掉 + # / 之類
            return
        if req.get("type") not in PROMPTS or not isinstance(req.get("image"), str):
            raise ValueError("請求格式錯誤")
        res = {"text": judge(req), "model": LLM_MODEL}
    except Exception as e:
        if rid is None:
            return
        res = {"error": f"{type(e).__name__}: {e}"[:300]}
    # 照片只在記憶體裡，處理完就丟，不存檔
    client.publish(f"{TOPIC}/res/{rid}", json.dumps(res, ensure_ascii=False), qos=1)
    print(rid[:8], "error" if "error" in res else "ok", flush=True)


def main():
    pool = ThreadPoolExecutor(WORKERS)  # 模型跑在別的執行緒，MQTT 心跳不會被卡住
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=E("MQTT_CLIENT_ID", ""))
    if E("MQTT_USER"):
        client.username_pw_set(E("MQTT_USER"), E("MQTT_PASS"))
    if E("MQTT_TLS") == "1":
        client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

    def on_connect(c, userdata, flags, reason, props):
        if reason.is_failure:
            print("MQTT 連線失敗：", reason, file=sys.stderr)
            return
        c.subscribe(f"$share/fora-ai/{TOPIC}/req/+", qos=1)
        print(f"已連線 {MQTT_HOST}:{MQTT_PORT}，模型 {LLM_MODEL} @ {LLM_URL}", flush=True)

    client.on_connect = on_connect
    client.on_message = lambda c, u, msg: pool.submit(handle, c, msg.payload)
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_forever()


if __name__ == "__main__":
    main()
