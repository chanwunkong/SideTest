from fastapi import FastAPI, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
import os
import shutil
import uvicorn

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["https://chanwunkong.github.io"],
    allow_methods=["POST"],
    allow_headers=["*"],
)

# 固定環境變數
os.environ["ALLOSAURUS_PATH"] = "/app/allosaurus_models"
model = None

# 這行是為了讓 Cloud Run 快速通過啟動檢查
@app.get("/")
def health():
    return {"status": "ready"}

@app.post("/recognize")
async def recognize(file: UploadFile = File(...)):
    global model
    # 第一次請求時才載入，避開啟動超時
    if model is None:
        from allosaurus.app import read_recognizer
        model = read_recognizer()

    temp_wav = "input.wav"
    try:
        with open(temp_wav, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
        
        ipa_result = model.recognize(temp_wav)
        return {"ipa": ipa_result}
    except Exception as e:
        return {"error": str(e)}
    finally:
        if os.path.exists(temp_wav):
            os.remove(temp_wav)

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8080))
    uvicorn.run(app, host="0.0.0.0", port=port)