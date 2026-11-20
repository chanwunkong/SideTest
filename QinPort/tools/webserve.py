"""網頁版的伺服器：提供引擎檔案（web/）與原版遊戲檔（game/），給自己的手機透過 Tailscale 測試。

- GET  /index.html 等：web/ 底下的引擎
- GET  /game/<路徑>：原版遊戲檔（檔名不分大小寫，跟 Windows 一樣）
- POST /batch：一次抓很多段檔案內容。內容是 JSON 陣列，每項是
    [路徑, 位移, 長度]：檔案的一段
    [路徑, -1, 序號]：Lib 封包的第幾項（伺服器自己查索引，手機不用下載索引表）
    [路徑, 0, -1]：整個檔案
  回傳全部片段依序串在一起（每段前面 4 bytes 長度，讀不到的長度為 0）。
  進場景時引擎用它一次抓回幾百張圖，避免一張一張來回。
- 瀏覽器支援 gzip 時回應會壓縮（行動網路差很多）。
只綁 Tailscale 位址，不要開到公開網路：遊戲檔只給自己的裝置用。
要 https（service worker／加入主畫面離線快取需要）：伺服器綁 127.0.0.1，再 `tailscale serve --bg --https=443 http://127.0.0.1:<連接埠>`，
手機開 https://<機器名>.<tailnet>.ts.net/（反向代理，這支程式不用改；細節見 HANDOFF「PWA」）。
用法：python3 webserve.py <web 資料夾> <遊戲資料夾> <綁定位址> [連接埠]
"""
import gzip, http.server, json, os, struct, subprocess, sys, tempfile
from functools import lru_cache
from socketserver import ThreadingMixIn

WEB, GAME = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])


@lru_cache(maxsize=None)
def listing(d):
    return {n.lower(): n for n in os.listdir(d)} if os.path.isdir(d) else {}


def resolve(rel):
    """遊戲資料夾內不分大小寫找檔案；不允許跳出遊戲資料夾。"""
    path = GAME
    for part in rel.replace('\\', '/').split('/'):
        if part in ('', '.'):
            continue
        if part == '..':
            return None
        real = listing(path).get(part.lower())
        if not real:
            return None
        path = os.path.join(path, real)
    return path if os.path.isfile(path) else None


def send_body(handler, data):
    if 'gzip' in handler.headers.get('Accept-Encoding', '') and len(data) > 1024:
        data = gzip.compress(data, compresslevel=4)
        handler.send_header('Content-Encoding', 'gzip')
    handler.send_header('Content-Length', str(len(data)))
    handler.end_headers()
    handler.wfile.write(data)


MOVIE_CACHE = os.path.join(tempfile.gettempdir(), 'qinport-movie')


def movie_mp4(name):
    """game/movie/<name>.avi 轉成瀏覽器放得動的 mp4（第一次要求時轉，之後用快取）。
    原版 End.avi 是 Indeo5 且檔頭第一個位元組被清成 0（'RIFF'），所以先補檔頭再重新編碼；H.264 的直接搬進 mp4。"""
    src = resolve('movie/' + name + '.avi')
    if not src:
        return None
    os.makedirs(MOVIE_CACHE, exist_ok=True)
    out = os.path.join(MOVIE_CACHE, name.lower() + '.mp4')
    if not os.path.exists(out):
        d = open(src, 'rb').read()
        fixed = os.path.join(MOVIE_CACHE, name.lower() + '.avi')
        open(fixed, 'wb').write(b'R' + d[1:] if d[:4] != b'RIFF' else d)
        # 一律重新編碼：H.264 直接從 AVI 搬進 mp4 時間戳不穩、碼率又高（Start.avi 45MB），手機上會卡；CRF 26 約縮成 1/3
        v = ['-fflags', '+genpts', '-c:v', 'libx264', '-profile:v', 'main', '-preset', 'veryfast', '-crf', '26', '-pix_fmt', 'yuv420p', '-g', '48']
        tmp = out + '.part.mp4'
        r = subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', fixed, *v, '-c:a', 'aac', '-movflags', '+faststart', tmp])
        os.remove(fixed)
        if r.returncode:
            return None
        os.replace(tmp, out)
    return out


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, '.webmanifest': 'application/manifest+json', '.wasm': 'application/wasm', '.js': 'text/javascript'}

    def __init__(self, *a, **k):
        super().__init__(*a, directory=WEB, **k)

    def log_message(self, fmt, *a):
        if '404' in fmt % a:  # 只記找不到的檔案，方便查缺什麼
            sys.stderr.write(f'{self.path} → {fmt % a}\n')

    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()

    def send_movie(self, path):
        size, rng = os.path.getsize(path), self.headers.get('Range', '')
        a, b = 0, size - 1
        if rng.startswith('bytes='):  # iOS Safari 的 <video> 一定要 Range
            lo, _, hi = rng[6:].partition('-')
            a = int(lo or 0)
            b = int(hi) if hi else b
            self.send_response(206)
            self.send_header('Content-Range', f'bytes {a}-{b}/{size}')
        else:
            self.send_response(200)
        self.send_header('Content-Type', 'video/mp4')
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Content-Length', str(b - a + 1))
        self.end_headers()
        with open(path, 'rb') as f:  # 只讀要的片段（以前每個 Range 請求都整檔讀進記憶體）
            f.seek(a)
            left = b - a + 1
            while left > 0:
                chunk = f.read(min(1 << 20, left))
                if not chunk:
                    break
                self.wfile.write(chunk)
                left -= len(chunk)

    def do_GET(self):
        if self.path.startswith('/movie/') and self.path.split('?')[0].endswith('.mp4'):
            path = movie_mp4(os.path.basename(self.path.split('?')[0])[:-4])
            return self.send_movie(path) if path else self.send_error(404)
        if not self.path.startswith('/game/'):
            return super().do_GET()
        from urllib.parse import unquote
        path = resolve(unquote(self.path[len('/game/'):].split('?')[0]))
        if not path:
            return self.send_error(404)
        self.send_response(200)
        self.send_header('Content-Type', 'application/octet-stream')
        send_body(self, open(path, 'rb').read())

    def do_POST(self):
        if self.path != '/batch':
            return self.send_error(404)
        try:
            reqs = json.loads(self.rfile.read(int(self.headers.get('Content-Length', 0))))
        except ValueError:
            return self.send_error(400)
        out, files = [], {}
        for rel, off, length in reqs:
            path = resolve(rel)
            chunk = b''
            if path:
                f = files.get(path) or files.setdefault(path, open(path, 'rb'))
                if off == -1:  # Lib 封包的第 length 項：0x10 項目數，0x100 起 (位移, 大小)
                    f.seek(16)
                    count = struct.unpack('<I', f.read(4))[0]
                    off, index, length = 0, length, 0
                    if 0 <= index < count:
                        f.seek(0x100 + index * 8)
                        off, length = struct.unpack('<II', f.read(8))
                if length == -1:  # 整個檔案
                    f.seek(0)
                    chunk = f.read()
                elif length > 0:
                    f.seek(off)
                    chunk = f.read(length)
            out.append(struct.pack('<I', len(chunk)) + chunk)
        for f in files.values():
            f.close()
        self.send_response(200)
        self.send_header('Content-Type', 'application/octet-stream')
        send_body(self, b''.join(out))


class Server(ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


if __name__ == '__main__':
    host, port = sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 8642
    print(f'http://{host}:{port}/  遊戲檔：{GAME}')
    Server((host, port), Handler).serve_forever()
