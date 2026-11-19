#pragma once

// 音量是原版 ini/Config.ini 的百分比（音乐音量、音效音量）
void audio_init(int music_percent, int sfx_percent);  // 先 res_init
void audio_volume(int music_percent, int sfx_percent);  // 設定視窗改音量（0..100）
void audio_music(const char *name);  // Music/<name>.wav 循環播放；"" = 停止
void audio_update(int network_idle);  // 每幀呼叫（網頁版：網路空閒才開始下載音樂，下載好了就播）
void audio_sfx(int id);              // snd.lib 的第 id 個音效
int audio_voice_state(void);          // 0 沒語音 / 1 下載中 / 2 播放中（過場對話框靠它決定何時收）
float audio_voice_seconds(void);      // 目前這句語音的長度（秒，還沒載好 = 0）
void audio_voice(int key);            // 對話語音 game/sound/<key %03d>.wav（一次一句，沒有檔 = 停掉上一句）
void audio_loop(int id, int pct);    // 環境音循環播放（同時一個），pct = 音量 %，0 = 停
void audio_quit(void);
extern int audio_trace;
