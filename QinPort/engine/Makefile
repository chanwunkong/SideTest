# Mac：make ；R36S：make r36s（用 zig 交叉編譯成 aarch64 Linux）
CC ?= cc
CFLAGS += -O2 -Wall -Wextra $(shell sdl2-config --cflags)
LDLIBS += $(shell sdl2-config --libs)

qin: src/main.c src/res.c src/res.h src/audio.c src/audio.h
	$(CC) $(CFLAGS) -o $@ src/main.c src/res.c src/audio.c $(LDLIBS)

# 技能戰鬥規則的自我檢查（要有 ../game）
skilltest: test/skilltest.c src/main.c src/res.c src/res.h src/audio.c src/audio.h
	$(CC) $(CFLAGS) -Wno-unused-function -o build/skilltest test/skilltest.c src/res.c src/audio.c $(LDLIBS) && cd test && ../build/skilltest

clean:
	rm -rf qin build storytest

# 劇情：../story 是 tools/storytool.py 從原版 EasyRPG.dll 產生的（只在本機，不進 git）；全部編成 build/story.a
# 反編譯的寫法會有大量型別警告，這裡關掉
STORY_CFLAGS = -std=gnu17 -O1 -w -Wno-error=int-conversion -Wno-error=incompatible-pointer-types \
	-Wno-error=int-to-pointer-cast -Wno-error=incompatible-function-pointer-types -Isrc/story
# $(call story_lib,編譯器,ar,物件資料夾,輸出)
story_lib = rm -rf $3 && mkdir -p $3 && find ../story -name '*.c' -print0 | xargs -0 -n1 -P8 sh -c \
	'$1 $(STORY_CFLAGS) -c "$$0" -o "$3/$$(printf %s "$$0" | cksum | cut -d" " -f1).o"' && $2 rcs $4 $3/*.o
build/story.a: ../story/index.txt src/story/qinstory.h
	$(call story_lib,$(CC),ar,build/story,$@)

# 劇情是原版 32 位元程式翻的（-1 寫成 0xffffffff 等），只在 32 位元的 WebAssembly 下才對，所以測試也用它跑：
# make storytest && node build/storytest.js 秦殇/咸阳/NPC/绣女 0 1
build/story-wasm.a: ../story/index.txt src/story/qinstory.h
	$(call story_lib,emcc,emar,build/story-wasm,$@)

storytest: src/story/storytest.c src/story/story_rt.c src/story/story.h build/story-wasm.a
	emcc -O1 -Wall -Wextra -Isrc/story -sALLOW_MEMORY_GROWTH -o build/storytest.js src/story/storytest.c src/story/story_rt.c build/story-wasm.a

# 網頁版（給手機透過 Tailscale 測試）：遊戲檔不打包，執行時跟 tools/webserve.py 要
# 有 ../story（tools/storytool.py 產生）就把原版劇情編進去（-DSTORY）；劇情只在 32 位元下才對，所以只有網頁版有
WEB_STORY = $(if $(wildcard ../story/index.txt),-DSTORY -Isrc/story src/story/story_rt.c build/story-wasm.a)
web: src/main.c src/res.c src/res.h src/audio.c src/audio.h ../web/shell.html $(if $(WEB_STORY),build/story-wasm.a src/story/story_rt.c)
	emcc -O2 -sUSE_SDL=2 -sASYNCIFY -sALLOW_MEMORY_GROWTH -sFORCE_FILESYSTEM -sEXPORTED_RUNTIME_METHODS=FS -lidbfs.js \
		-sEXPORTED_FUNCTIONS=_main,_malloc,_free \
		--shell-file ../web/shell.html -o ../web/index.html src/main.c src/res.c src/audio.c $(WEB_STORY)

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
