# Mac：make ；R36S：make r36s（用 zig 交叉編譯成 aarch64 Linux）
CC ?= cc
CFLAGS += -O2 -Wall -Wextra $(shell sdl2-config --cflags)
LDLIBS += $(shell sdl2-config --libs)

qin: src/main.c src/res.c src/res.h src/audio.c src/audio.h
	$(CC) $(CFLAGS) -o $@ src/main.c src/res.c src/audio.c $(LDLIBS)

clean:
	rm -rf qin build

# 網頁版（給手機透過 Tailscale 測試）：遊戲檔不打包，執行時跟 tools/webserve.py 要
web: src/main.c src/res.c src/res.h src/audio.c src/audio.h ../web/shell.html
	emcc -O2 -sUSE_SDL=2 -sASYNCIFY -sALLOW_MEMORY_GROWTH -sFORCE_FILESYSTEM -sEXPORTED_RUNTIME_METHODS=FS \
		-sEXPORTED_FUNCTIONS=_main,_malloc,_free \
		--shell-file ../web/shell.html -o ../web/index.html src/main.c src/res.c src/audio.c

# R36S（ArkOS、ROCKNIX 等 aarch64 Linux）：zig 交叉編譯，只連 Debian 11 的 SDL2 標頭與 .so；執行時用掌機系統內建的 SDL2
SDL_DEB = https://deb.debian.org/debian/pool/main/libs/libsdl2
SDL_VER = 2.0.14+dfsg2-3+deb11u1
SYSROOT = build/sysroot
$(SYSROOT)/usr/include/SDL2/SDL.h:
	mkdir -p build/deb $(SYSROOT)
	cd build/deb && for p in libsdl2-dev libsdl2-2.0-0; do curl -sfLO $(SDL_DEB)/$${p}_$(SDL_VER)_arm64.deb && ar x $${p}_$(SDL_VER)_arm64.deb data.tar.xz && tar xf data.tar.xz -C ../sysroot; done

r36s: src/main.c src/res.c src/audio.c src/res.h src/audio.h $(SYSROOT)/usr/include/SDL2/SDL.h
	mkdir -p ../r36s/qinport
	zig cc -target aarch64-linux-gnu.2.17 -O2 -s -Wall -Wextra -D_REENTRANT \
		-I$(SYSROOT)/usr/include/SDL2 -I$(SYSROOT)/usr/include/aarch64-linux-gnu \
		-o ../r36s/qinport/qin src/main.c src/res.c src/audio.c \
		-L$(SYSROOT)/usr/lib/aarch64-linux-gnu -lSDL2-2.0 -lm -Wl,--allow-shlib-undefined
