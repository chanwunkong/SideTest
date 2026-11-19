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
static long music_data, music_end;  // 資料區在檔案裡的範圍
static SDL_AudioStream *music_stream;
static volatile int music_eof, music_over;  // 檔案讀完、聲音也放完（原版 OpenAL 來源播完一次就停，0x57d1b0 看狀態）
static int music_pct = 15;                  // 設定的音樂音量百分比（DAT_00953ab8）
static Sfx file_cache[MAX_FILE];
static struct { int n, file[5]; } sfx_def[MAX_SFX];  // n：0 = 還沒讀、−1 = 沒有
static uint8_t *snd_tab;     // WAV 表：3000 筆 × 0x10c（檔名 260、大小、位移）
static uint32_t snd_rec, snd_data, snd_files;
static Voice voices[VOICES];
static Sfx voice_buf[2];  // 對話語音（audio_voice）
static int voice_flip;
static Voice voice_v;
static float voice_secs;  // 目前這句語音的長度（秒）；章節字幕用它算捲動速度（Mfx_LWave_GetPlayTime，Poq.exe 0x57a5b0）
#define LOOPS 4  // 環境音槽：0 雨、1 風（也是建築環境音組 1）、2、3 建築環境音組（Mfx 以音效編號當鍵，原版 0x480740／0x47ff60）
static Voice loop_v[LOOPS];
static int loop_id[LOOPS] = {-1, -1, -1, -1}, loop_pct[LOOPS];

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
            if (left <= 0) { music_eof = 1; break; }  // 不循環：播完由 audio_music_tick 的狀態機決定下一首（原版 0x57a010）
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
        if (music_eof && !SDL_AudioStreamAvailable(music_stream)) music_over = 1;
    }
    for (int i = 0; i < VOICES; i++) {
        Voice *v = &voices[i];
        if (!v->sfx) continue;
        Uint32 n = v->sfx->len - v->pos < (Uint32)len ? v->sfx->len - v->pos : (Uint32)len;
        SDL_MixAudioFormat(out, v->sfx->buf + v->pos, have.format, n, sfx_vol);
        v->pos += n;
        if (v->pos >= v->sfx->len) v->sfx = NULL;
    }
    if (voice_v.sfx) {
        Uint32 n = voice_v.sfx->len - voice_v.pos < (Uint32)len ? voice_v.sfx->len - voice_v.pos : (Uint32)len;
        SDL_MixAudioFormat(out, voice_v.sfx->buf + voice_v.pos, have.format, n, sfx_vol);
        voice_v.pos += n;
        if (voice_v.pos >= voice_v.sfx->len) voice_v.sfx = NULL;
    }
    for (int k = 0; k < LOOPS; k++)
    for (int done = 0; loop_v[k].sfx && loop_v[k].sfx->len && done < len;) {
        Uint32 n = loop_v[k].sfx->len - loop_v[k].pos < (Uint32)(len - done) ? loop_v[k].sfx->len - loop_v[k].pos : (Uint32)(len - done);
        SDL_MixAudioFormat(out + done, loop_v[k].sfx->buf + loop_v[k].pos, have.format, n, sfx_vol * loop_pct[k] / 100);
        done += n;
        loop_v[k].pos = (loop_v[k].pos + n) % loop_v[k].sfx->len;
    }
}

static struct { int state, mode, prev_state, cd, gap, prev_gap, has, prev_has; char name[64], prev_name[64]; } mu;  // CMusic：0x649200 狀態、0x649210 模式、0x649208 前一狀態、0x953ac4 倒數
static char cur_name[64];  // 最後開始播的檔名（沒有裝置時也記，測試用）
static int sim_busy;       // 沒有裝置時模擬「正在播」（測試用 audio_music_sim_end 結束）

// 0x57a720／0x579df0 裡的音量：場景模式 10 且狀態 2 時減半
static void music_vol_apply(void) {
    int p = music_pct;
    if (mu.mode == 10 && mu.state == 2) p /= 2;
    music_vol = p * SDL_MIX_MAXVOLUME / 100;
}

void audio_volume(int music_percent, int sfx_percent) {
    music_pct = music_percent;
    music_vol_apply();
    sfx_vol = sfx_percent * SDL_MIX_MAXVOLUME / 100;
}

void audio_init(int music_percent, int sfx_percent) {
    music_pct = music_percent;
    music_vol_apply();
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
    music_eof = music_over = 0;
    SDL_UnlockAudioDevice(dev);
}

void audio_music(const char *name) {
    snprintf(cur_name, sizeof cur_name, "%s", name);
    sim_busy = name[0] != 0;
    if (audio_trace) printf("music %s\n", name[0] ? name : "(stop)");
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

#ifdef __EMSCRIPTEN__
static void voice_update(void);
#endif
void audio_update(int network_idle) {
#ifdef __EMSCRIPTEN__
    voice_update();
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

// ---- 背景音樂狀態機（CMusic，Poq.exe 0x579df0 換場景、0x57a010 每拍、0x57a910 特殊曲、0x57ab50 收尾還原、0x57a720 音量）----
// 狀態 2 = 場景音樂、1 = 特殊曲（credit／start／dead，播一次）。原版沒有戰鬥曲、日夜曲、淡入淡出：music.txt 第 3、4 欄（战斗音乐名）
// 只用來判斷「播完要不要隔幾拍再播」（第 3 欄非空 = 隔 rand()%12 拍，空 = 馬上）。換場景只改「下一首」，現在這首播完才換
// （0x57a010 的 isPlaying 檢查，OpenAL 播放中不能換緩衝）。場景種類（G_Type）12（山洞）= 播完就停；10／0x12 不重選曲、10 音量減半。
// dead.wav 是連線版 CDeadDialog（0x547b00）、start.wav 是標題選單（0x535250），單機流程沒有觸發點。
static int music_busy(void) {
#ifdef __EMSCRIPTEN__
    if (pending[0]) return 1;
#endif
    if (!dev) return sim_busy;
    return music_stream && !music_over;
}
void audio_music_sim_end(void) { sim_busy = 0; }
const char *audio_music_now(void) { return music_busy() ? cur_name : ""; }
int audio_music_mode(void) { return mu.mode; }

static void music_end_check(void) {  // 播完的來源（OpenAL 狀態不是 PLAYING）：放掉串流
    if (dev && music_stream && music_over) audio_music("");
}

void audio_music_tick(int force) {  // 0x57a010
    music_end_check();
    if (mu.state != 2 && mu.state != 0) return;
    if (!force && music_busy()) return;
    if (--mu.cd > 0) return;
    if (mu.mode == 12) { if (cur_name[0]) audio_music(""); }
    else if (mu.has && mu.name[0]) { music_vol_apply(); audio_music(mu.name); }
    mu.cd = mu.has && mu.gap ? rand() % 12 : 0;
}

void audio_scene_music(int mode, const char *name, int gap) {  // 0x579df0（mode = 房間 G_Type）
    int sp = mode == 10 || mode == 0x12, prev_sp = mu.mode == 10 || mu.mode == 0x12, upd = 1;
    mu.mode = mode;
    if (sp && !prev_sp) { music_vol_apply(); upd = 0; }
    else if (sp) upd = 0;
    else if (prev_sp) { music_vol_apply(); upd = 0; }
    if (upd) {
        mu.prev_state = mu.state; snprintf(mu.prev_name, sizeof mu.prev_name, "%s", mu.name); mu.prev_gap = mu.gap; mu.prev_has = mu.has;
        mu.state = 2; mu.has = 1; snprintf(mu.name, sizeof mu.name, "%s", name); mu.gap = gap;
    }
    mu.cd = 2;
    audio_music_tick(upd);
}

void audio_music_special(const char *name) {  // 0x57a910：存下目前的、改播特殊曲一次（shortcut: 原版沒先停，OpenAL 播放中換緩衝會失敗；這裡照意圖直接換）
    mu.prev_state = mu.state; snprintf(mu.prev_name, sizeof mu.prev_name, "%s", mu.name); mu.prev_gap = mu.gap; mu.prev_has = mu.has;
    mu.state = 1; mu.has = 1; snprintf(mu.name, sizeof mu.name, "%s", name); mu.mode = 0;
    audio_music(name);
}

void audio_music_pop(void) {  // 0x57ab50：還原前一個狀態
    mu.state = mu.prev_state;
    if (mu.state == 1) { char n[64]; snprintf(n, sizeof n, "%s", mu.prev_name); audio_music_special(n); return; }
    snprintf(mu.name, sizeof mu.name, "%s", mu.prev_name); mu.gap = mu.prev_gap; mu.has = mu.prev_has;
    if (mu.has) { mu.cd = 0; audio_music_tick(1); return; }
    audio_music("");  // 沒有前一首：整個重設
    mu.state = 0; mu.mode = 0; mu.name[0] = 0; mu.cd = 0; mu.has = 0;
    music_vol_apply();
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

// WAV 記憶體 → 裝置格式的 Sfx（音效庫與語音檔共用）
static int wav_to_sfx(const uint8_t *data, uint32_t size, Sfx *s) {
    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;
    if (!SDL_LoadWAV_RW(SDL_RWFromConstMem(data, (int)size), 1, &spec, &buf, &len)) return 0;
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, have.format, have.channels, have.freq);
    cvt.len = len;
    cvt.buf = SDL_malloc(len * (cvt.len_mult > 0 ? cvt.len_mult : 1));
    memcpy(cvt.buf, buf, len);
    SDL_FreeWAV(buf);
    if (cvt.needed) SDL_ConvertAudio(&cvt);
    s->buf = cvt.buf;
    s->len = cvt.needed ? cvt.len_cvt : len;
    return 1;
}

static const Sfx *sfx_load(int k) {
    Sfx *s = &file_cache[k];
    if (s->buf) return s;
    const uint8_t *e = snd_tab + k * 0x10c;
    uint32_t size = rd32(e + 0x104);
    uint8_t *data = res_span("snd.lib", snd_data + rd32(e + 0x108), size);
    if (!data) return NULL;
    int ok = wav_to_sfx(data, size, s);
    free(data);
    return ok ? s : NULL;
}

// 對話語音（Poq.exe 0x4725c0 '@' 4：GetSoundPath + "%.3d.wav"，對話鍵 = 檔號；沒有檔就停掉上一句，0x57a6b0 < 0 → Mfx_LWave_Stop）。
// 一次只一句：新的換掉舊的。網頁版先背景下載到 /voice，audio_update 下載好才播。
static void voice_start(const uint8_t *data, uint32_t size) {
    Sfx *s = &voice_buf[voice_flip ^ 1];
    SDL_LockAudioDevice(dev);
    voice_v.sfx = NULL;
    SDL_UnlockAudioDevice(dev);
    SDL_free(s->buf);
    *s = (Sfx){0};
    if (!wav_to_sfx(data, size, s)) return;
    voice_secs = (float)s->len / ((SDL_AUDIO_BITSIZE(have.format) / 8) * have.channels * have.freq);
    voice_flip ^= 1;
    SDL_LockAudioDevice(dev);
    voice_v = (Voice){s, 0};
    SDL_UnlockAudioDevice(dev);
}
// 檔名（不含副檔名）：1000 + n = 章節配音 c%03d（Poq.exe 0x566920 的 sound\\C%.3d.wav，n = 表 0x645200），其餘 = 對話語音 %03d
static void voice_name(int key, char *out, size_t cap) {
    if (key >= 1000) snprintf(out, cap, "c%03d", key - 1000);
    else snprintf(out, cap, "%03d", key);
}
#ifdef __EMSCRIPTEN__
static int voice_want = -1;
EM_JS(void, js_fetch_voice, (int key), {
    const n = key >= 1000 ? 'c' + String(key - 1000).padStart(3, '0') : String(key).padStart(3, '0');  // 1000 + n = 章節配音 sound\\C%.3d.wav（小寫檔名）
    fetch('game/sound/' + n + '.wav').then(r => r.ok ? r.arrayBuffer() : null).then(b => {
        try { FS.mkdir('/voice'); } catch (e) {}
        if (b) { FS.writeFile('/voice/' + n + '.part', new Uint8Array(b)); FS.rename('/voice/' + n + '.part', '/voice/' + n + '.wav'); }
        else FS.writeFile('/voice/' + n + '.none', new Uint8Array(0));
    });
});
static void voice_update(void) {
    char path[48];
    if (voice_want < 0) return;
    char nm[16];
    voice_name(voice_want, nm, sizeof nm);
    snprintf(path, sizeof path, "/voice/%.11s.none", nm);
    if (!access(path, F_OK)) { remove(path); voice_want = -1; return; }
    snprintf(path, sizeof path, "/voice/%.11s.wav", nm);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    uint8_t *d = malloc(n);
    if (d && fread(d, 1, n, f) == (size_t)n) voice_start(d, (uint32_t)n);
    free(d);
    fclose(f);
    remove(path);
    voice_want = -1;
}
#endif
void audio_voice(int key) {
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    voice_v.sfx = NULL;
    SDL_UnlockAudioDevice(dev);
    voice_secs = 0;
    if (key < 0 || key > 1999) return;
#ifdef __EMSCRIPTEN__
    voice_want = key;
    js_fetch_voice(key);
#else
    char name[16], base[16];
    size_t n;
    voice_name(key, base, sizeof base);
    snprintf(name, sizeof name, "%s.wav", base);
    uint8_t *d = res_file("sound", name, &n);
    if (d) { voice_start(d, (uint32_t)n); free(d); }
#endif
}

float audio_voice_seconds(void) { return voice_secs; }

// 對話語音狀態（0x472e70 的 Mfx_LWave_IsPlaying）：0 = 沒在播也沒在等下載，1 = 網頁版下載中，2 = 播放中
int audio_voice_state(void) {
    if (voice_v.sfx) return 2;
#ifdef __EMSCRIPTEN__
    if (voice_want >= 0) return 1;
#endif
    return 0;
}

int audio_trace;  // --debug-trace：每個音效印出編號、檔數、載入結果
void audio_sfx(int id) {
    if (!dev || id < 0 || id >= MAX_SFX) return;
    int n = sfx_files(id);
    const Sfx *s = n > 0 ? sfx_load(sfx_def[id].file[rand() % n]) : NULL;
    if (audio_trace) printf("sfx %d files=%d %s\n", id, n, s ? "ok" : "MISSING");
    if (!s) return;
    SDL_LockAudioDevice(dev);
    for (int i = 0; i < VOICES; i++)
        if (!voices[i].sfx) { voices[i] = (Voice){s, 0}; break; }
    SDL_UnlockAudioDevice(dev);
}

// 環境音（原版 0x480350 第二個參數 1 = 循環；0x480740 停；0x47ff60 調音量）：槽 = 音效編號 0~3，同編號再呼叫只調音量；pct = 音量百分比，0 = 停
void audio_loop(int id, int pct) {
    if (!dev || id < 0 || id >= LOOPS) return;
    const Sfx *s = NULL;
    if (pct > 0 && id != loop_id[id] && id < MAX_SFX) {
        int n = sfx_files(id);
        s = n > 0 ? sfx_load(sfx_def[id].file[0]) : NULL;
    }
    SDL_LockAudioDevice(dev);
    if (pct <= 0) loop_v[id].sfx = NULL, loop_id[id] = -1;
    else if (id != loop_id[id]) loop_v[id] = (Voice){s, 0}, loop_id[id] = s ? id : -1;
    loop_pct[id] = pct;
    SDL_UnlockAudioDevice(dev);
}

void audio_quit(void) {
    if (dev) SDL_CloseAudioDevice(dev);
    if (music_stream) SDL_FreeAudioStream(music_stream);
    if (music_file) fclose(music_file);
    for (int i = 0; i < MAX_FILE; i++) SDL_free(file_cache[i].buf);
}
