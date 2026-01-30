FROM python:3.10-slim

RUN apt-get update && apt-get install -y \
    libsndfile1 \
    ffmpeg \
    gcc \
    g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 強制固定環境變數
ENV ALLOSAURUS_PATH=/app/allosaurus_models
ENV PORT=8080

RUN pip install --no-cache-dir torch --index-url https://download.pytorch.org/whl/cpu
RUN pip install --no-cache-dir allosaurus fastapi uvicorn python-multipart

# 構建時強制下載到固定路徑
RUN python -m allosaurus.bin.download_model -m latest

COPY . .

CMD ["python", "main.py"]