// 音樂與音效。音樂是很大的 WAV（最大 45MB），邊讀檔邊播並即時轉成裝置格式；音效（sound/NNN.wav）載入後快取。
#include "audio.h"
#include "res.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define VOICES 16
#define MAX_SFX 1024

typedef struct { Uint8 *buf; Uint32 len; } Sfx;
typedef struct { const Sfx *sfx; Uint32 pos; } Voice;

static SDL_AudioDeviceID dev;
static SDL_AudioSpec have;
static int music_vol = 128, sfx_vol = 128;  // SDL_MixAudioFormat 的音量 0..128

static FILE *music_file;
static long music_data, music_end;  // 資料區在檔案裡的範圍（循環播放用）
static SDL_AudioStream *music_stream;
static Sfx sfx_cache[MAX_SFX];
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

void audio_music(const char *name) {
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    if (music_stream) SDL_FreeAudioStream(music_stream);
    if (music_file) fclose(music_file);
    music_stream = NULL;
    music_file = NULL;
    char file[96];
    snprintf(file, sizeof file, "%.80s.wav", name);
    SDL_AudioSpec src = {0};
    if (name[0] && (music_file = res_fopen("Music", file)) && wav_open(music_file, &src, &music_data, &music_end))
        music_stream = SDL_NewAudioStream(src.format, src.channels, src.freq, have.format, have.channels, have.freq);
    if (!music_stream && music_file) { fclose(music_file); music_file = NULL; }
    SDL_UnlockAudioDevice(dev);
}

void audio_sfx(int id) {
    if (!dev || id < 0 || id >= MAX_SFX) return;
    Sfx *s = &sfx_cache[id];
    if (!s->buf) {
        char file[16];
        snprintf(file, sizeof file, "%03d.wav", id);
        size_t size;
        uint8_t *data = res_file("sound", file, &size);
        SDL_AudioSpec spec;
        Uint8 *buf;
        Uint32 len;
        if (!data) return;
        if (!SDL_LoadWAV_RW(SDL_RWFromConstMem(data, (int)size), 1, &spec, &buf, &len)) { free(data); return; }
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
    }
    SDL_LockAudioDevice(dev);
    for (int i = 0; i < VOICES; i++)
        if (!voices[i].sfx) { voices[i] = (Voice){s, 0}; break; }
    SDL_UnlockAudioDevice(dev);
}

void audio_quit(void) {
    if (dev) SDL_CloseAudioDevice(dev);
    if (music_stream) SDL_FreeAudioStream(music_stream);
    if (music_file) fclose(music_file);
    for (int i = 0; i < MAX_SFX; i++) SDL_free(sfx_cache[i].buf);
}
