# Mac：make ；R36S（交叉編譯）時把 CC 與 sdl2-config 換成目標平台的版本
CC ?= cc
CFLAGS += -O2 -Wall -Wextra $(shell sdl2-config --cflags)
LDLIBS += $(shell sdl2-config --libs)

qin: src/main.c src/res.c src/res.h src/audio.c src/audio.h
	$(CC) $(CFLAGS) -o $@ src/main.c src/res.c src/audio.c $(LDLIBS)

clean:
	rm -f qin

# 網頁版（給手機透過 Tailscale 測試）：遊戲檔不打包，執行時跟 tools/webserve.py 要
web: src/main.c src/res.c src/res.h src/audio.c src/audio.h ../web/shell.html
	emcc -O2 -sUSE_SDL=2 -sASYNCIFY -sALLOW_MEMORY_GROWTH -sFORCE_FILESYSTEM -sEXPORTED_RUNTIME_METHODS=FS \
		-sEXPORTED_FUNCTIONS=_main,_malloc,_free \
		--shell-file ../web/shell.html -o ../web/index.html src/main.c src/res.c src/audio.c
