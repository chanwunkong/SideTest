// 音樂與音效。音樂是很大的 WAV（最大 45MB），邊讀檔邊播並即時轉成裝置格式；音效從 snd.lib（Mfx 音效庫）讀，載入後快取。
#include "audio.h"
#include "res.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <unistd.h>

// 網頁版：音樂在背景下載進記憶體檔案系統的 /music，下載完 audio_update 才開始播
EM_JS(void, js_fetch_music, (const char *name), {
    const n = UTF8ToString(name);
    fetch('game/Music/' + n + '.wav').then(r => r.ok ? r.arrayBuffer() : null).then(b => {
        if (!b) return;
        try { FS.mkdir('/music'); } catch (e) {}
        FS.writeFile('/music/' + n + '.wav.part', new Uint8Array(b));
        FS.rename('/music/' + n + '.wav.part', '/music/' + n + '.wav');
    });
});
static char pending[96], playing_path[128];
static int fetching;
#endif

#define VOICES 16
#define MAX_SFX 2000   // snd.lib 的音效數（檔頭 0x104）
#define MAX_FILE 3000  // snd.lib 的 WAV 數（檔頭 0x108）

typedef struct { Uint8 *buf; Uint32 len; } Sfx;
typedef struct { const Sfx *sfx; Uint32 pos; } Voice;

static SDL_AudioDeviceID dev;
static SDL_AudioSpec have;
static int music_vol = 128, sfx_vol = 128;  // SDL_MixAudioFormat 的音量 0..128

static FILE *music_file;
static long music_data, music_end;  // 資料區在檔案裡的範圍（循環播放用）
static SDL_AudioStream *music_stream;
static Sfx file_cache[MAX_FILE];
static struct { int n, file[5]; } sfx_def[MAX_SFX];  // n：0 = 還沒讀、−1 = 沒有
static uint8_t *snd_tab;     // WAV 表：3000 筆 × 0x10c（檔名 260、大小、位移）
static uint32_t snd_rec, snd_data, snd_files;
static Voice voices[VOICES];

static Uint32 rd32(const Uint8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (Uint32)p[3] << 24; }

// 只處理 PCM WAV：找到 fmt 與 data 區塊
static int wav_open(FILE *f, SDL_AudioSpec *spec, long *data, long *end) {
    Uint8 h[12], c[8], fmt[16];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return 0;
    int got_fmt = 0;
    while (fread(c, 1, 8, f) == 8) {
        Uint32 len = rd32(c + 4);
        if (!memcmp(c, "fmt ", 4) && len >= 16) {
            if (fread(fmt, 1, 16, f) != 16) return 0;
            fseek(f, len - 16 + (len & 1), SEEK_CUR);
            if ((fmt[0] | fmt[1] << 8) != 1) return 0;  // 不是 PCM
            spec->channels = fmt[2] | fmt[3] << 8;
            spec->freq = rd32(fmt + 4);
            int bits = fmt[14] | fmt[15] << 8;
            spec->format = bits == 8 ? AUDIO_U8 : AUDIO_S16LSB;
            got_fmt = 1;
        } else if (!memcmp(c, "data", 4) && got_fmt) {
            *data = ftell(f);
            *end = *data + len;
            return 1;
        } else {
            fseek(f, len + (len & 1), SEEK_CUR);
        }
    }
    return 0;
}

static void mix(void *userdata, Uint8 *out, int len) {
    (void)userdata;
    memset(out, have.silence, len);
    if (music_stream) {
        Uint8 tmp[16384];
        while (SDL_AudioStreamAvailable(music_stream) < len) {
            long left = music_end - ftell(music_file);
            if (left <= 0) { fseek(music_file, music_data, SEEK_SET); continue; }  // 循環
            size_t n = fread(tmp, 1, left < (long)sizeof tmp ? (size_t)left : sizeof tmp, music_file);
            if (!n) break;
            SDL_AudioStreamPut(music_stream, tmp, (int)n);
        }
        Uint8 buf[16384];
        for (int done = 0; done < len;) {
            int n = SDL_AudioStreamGet(music_stream, buf, len - done < (int)sizeof buf ? len - done : (int)sizeof buf);
            if (n <= 0) break;
            SDL_MixAudioFormat(out + done, buf, have.format, n, music_vol);
            done += n;
        }
    }
    for (int i = 0; i < VOICES; i++) {
        Voice *v = &voices[i];
        if (!v->sfx) continue;
        Uint32 n = v->sfx->len - v->pos < (Uint32)len ? v->sfx->len - v->pos : (Uint32)len;
        SDL_MixAudioFormat(out, v->sfx->buf + v->pos, have.format, n, sfx_vol);
        v->pos += n;
        if (v->pos >= v->sfx->len) v->sfx = NULL;
    }
}

void audio_init(int music_percent, int sfx_percent) {
    music_vol = music_percent * SDL_MIX_MAXVOLUME / 100;
    sfx_vol = sfx_percent * SDL_MIX_MAXVOLUME / 100;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) { fprintf(stderr, "沒有音效：%s\n", SDL_GetError()); return; }
    SDL_AudioSpec want = {0};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = mix;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) { fprintf(stderr, "沒有音效：%s\n", SDL_GetError()); return; }
    SDL_PauseAudioDevice(dev, 0);
}

static void music_start(FILE *f) {
    SDL_AudioSpec src = {0};
    SDL_LockAudioDevice(dev);
    if ((music_file = f) && wav_open(music_file, &src, &music_data, &music_end))
        music_stream = SDL_NewAudioStream(src.format, src.channels, src.freq, have.format, have.channels, have.freq);
    if (!music_stream && music_file) { fclose(music_file); music_file = NULL; }
    SDL_UnlockAudioDevice(dev);
}

void audio_music(const char *name) {
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    if (music_stream) SDL_FreeAudioStream(music_stream);
    if (music_file) fclose(music_file);
    music_stream = NULL;
    music_file = NULL;
    SDL_UnlockAudioDevice(dev);
    if (!name[0]) return;
#ifdef __EMSCRIPTEN__
    if (playing_path[0]) remove(playing_path);  // 上一首最大 45MB，不留在記憶體
    playing_path[0] = 0;
    snprintf(pending, sizeof pending, "%.80s", name);
    fetching = 0;  // 等場景的圖下載完才開始抓（行動網路不要跟畫面搶）
#else
    char file[96];
    snprintf(file, sizeof file, "%.80s.wav", name);
    music_start(res_fopen("Music", file));
#endif
}

void audio_update(int network_idle) {
#ifdef __EMSCRIPTEN__
    if (!pending[0]) return;
    if (!fetching) {
        if (!network_idle) return;
        fetching = 1;
        js_fetch_music(pending);
        return;
    }
    char path[128];
    snprintf(path, sizeof path, "/music/%s.wav", pending);
    if (access(path, F_OK)) return;
    pending[0] = 0;
    snprintf(playing_path, sizeof playing_path, "%s", path);
    music_start(fopen(path, "rb"));
#else
    (void)network_idle;
#endif
}

// snd.lib：檔頭 0x104 音效數、0x108 WAV 數；0x20c 起每個音效 0x598 bytes（變化數、名稱 128、5 個檔名各 260），
// 接著 WAV 表（每筆 0x10c：檔名 260、大小、從資料區起算的位移），再來是資料區。一個音效有幾個檔時隨機挑一個。
// shortcut: 原版挑變化的方法（Mfx_SND_Play）沒查，先隨機
static int sfx_files(int id) {
    if (sfx_def[id].n) return sfx_def[id].n;
    sfx_def[id].n = -1;
    if (!snd_tab) {
        uint8_t *h = res_span("snd.lib", 0, 0x10c);
        if (!h) return -1;
        uint32_t nrec = rd32(h + 0x104);
        snd_files = rd32(h + 0x108);
        free(h);
        if (nrec > MAX_SFX || snd_files > MAX_FILE) return -1;
        snd_rec = 0x20c;
        snd_data = snd_rec + nrec * 0x598 + snd_files * 0x10c;
        if (!(snd_tab = res_span("snd.lib", snd_rec + nrec * 0x598, snd_files * 0x10c))) return -1;
    }
    uint8_t *r = res_span("snd.lib", snd_rec + (uint32_t)id * 0x598, 0x598);
    if (!r) return -1;
    int want = (int)rd32(r) < 5 ? (int)rd32(r) : 5, n = 0;
    for (int v = 0; v < want; v++) {
        const char *nm = (const char *)r + 0x84 + v * 0x104;
        for (uint32_t k = 0; nm[0] && k < snd_files; k++)
            if (!strcasecmp((const char *)snd_tab + k * 0x10c, nm)) { sfx_def[id].file[n++] = (int)k; break; }
    }
    free(r);
    return sfx_def[id].n = n ? n : -1;
}

static const Sfx *sfx_load(int k) {
    Sfx *s = &file_cache[k];
    if (s->buf) return s;
    const uint8_t *e = snd_tab + k * 0x10c;
    uint32_t size = rd32(e + 0x104);
    uint8_t *data = res_span("snd.lib", snd_data + rd32(e + 0x108), size);
    if (!data) return NULL;
    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;
    if (!SDL_LoadWAV_RW(SDL_RWFromConstMem(data, (int)size), 1, &spec, &buf, &len)) { free(data); return NULL; }
    free(data);
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, have.format, have.channels, have.freq);
    cvt.len = len;
    cvt.buf = SDL_malloc(len * (cvt.len_mult > 0 ? cvt.len_mult : 1));
    memcpy(cvt.buf, buf, len);
    SDL_FreeWAV(buf);
    if (cvt.needed) SDL_ConvertAudio(&cvt);
    s->buf = cvt.buf;
    s->len = cvt.needed ? cvt.len_cvt : len;
    return s;
}

void audio_sfx(int id) {
    if (!dev || id < 0 || id >= MAX_SFX) return;
    int n = sfx_files(id);
    const Sfx *s = n > 0 ? sfx_load(sfx_def[id].file[rand() % n]) : NULL;
    if (!s) return;
    SDL_LockAudioDevice(dev);
    for (int i = 0; i < VOICES; i++)
        if (!voices[i].sfx) { voices[i] = (Voice){s, 0}; break; }
    SDL_UnlockAudioDevice(dev);
}

void audio_quit(void) {
    if (dev) SDL_CloseAudioDevice(dev);
    if (music_stream) SDL_FreeAudioStream(music_stream);
    if (music_file) fclose(music_file);
    for (int i = 0; i < MAX_FILE; i++) SDL_free(file_cache[i].buf);
}
