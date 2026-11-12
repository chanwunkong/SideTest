"""網頁版的伺服器：提供引擎檔案（web/）與原版遊戲檔（game/），給自己的手機透過 Tailscale 測試。

- GET  /index.html 等：web/ 底下的引擎
- GET  /game/<路徑>：原版遊戲檔（檔名不分大小寫，跟 Windows 一樣）
- POST /batch：一次抓很多段檔案內容。內容是 JSON [[路徑, 位移, 長度], ...]，
  回傳全部片段依序串在一起（每段前面 4 bytes 長度，讀不到的長度為 0）。
  進場景時引擎用它一次抓回幾百張圖，避免一張一張來回。
只綁 Tailscale 位址，不要開到公開網路：遊戲檔只給自己的裝置用。
用法：python3 webserve.py <web 資料夾> <遊戲資料夾> <綁定位址> [連接埠]
"""
import http.server, json, os, struct, sys
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


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=WEB, **k)

    def log_message(self, fmt, *a):
        if '404' in fmt % a:  # 只記找不到的檔案，方便查缺什麼
            sys.stderr.write(f'{self.path} → {fmt % a}\n')

    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()

    def do_GET(self):
        if not self.path.startswith('/game/'):
            return super().do_GET()
        from urllib.parse import unquote
        path = resolve(unquote(self.path[len('/game/'):].split('?')[0]))
        if not path:
            return self.send_error(404)
        data = open(path, 'rb').read()
        self.send_response(200)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

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
            if path and length > 0:
                f = files.get(path) or files.setdefault(path, open(path, 'rb'))
                f.seek(off)
                chunk = f.read(length)
            out.append(struct.pack('<I', len(chunk)) + chunk)
        for f in files.values():
            f.close()
        body = b''.join(out)
        self.send_response(200)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class Server(ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


if __name__ == '__main__':
    host, port = sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 8642
    print(f'http://{host}:{port}/  遊戲檔：{GAME}')
    Server((host, port), Handler).serve_forever()
