# Mac：make ；R36S（交叉編譯）時把 CC 與 sdl2-config 換成目標平台的版本
CC ?= cc
CFLAGS += -O2 -Wall -Wextra $(shell sdl2-config --cflags)
LDLIBS += $(shell sdl2-config --libs)

qin: src/main.c src/res.c src/res.h src/audio.c src/audio.h
	$(CC) $(CFLAGS) -o $@ src/main.c src/res.c src/audio.c $(LDLIBS)

clean:
	rm -f qin

# 網頁版（給手機透過 Tailscale 測試）：先用 tools/webpack.py 產生 ../web/game
web: src/main.c src/res.c src/res.h src/audio.c src/audio.h ../web/shell.html
	emcc -O2 -sUSE_SDL=2 -sASYNCIFY -sALLOW_MEMORY_GROWTH --preload-file ../web/game@/game \
		--shell-file ../web/shell.html -o ../web/index.html src/main.c src/res.c src/audio.c
