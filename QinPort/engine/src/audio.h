#pragma once

// 音量是原版 ini/Config.ini 的百分比（音乐音量、音效音量）
void audio_init(int music_percent, int sfx_percent);  // 先 res_init
void audio_volume(int music_percent, int sfx_percent);  // 設定視窗改音量（0..100）
void audio_music(const char *name);  // Music/<name>.wav 播一次（立刻換掉現在這首）；"" = 停止
void audio_scene_music(int mode, const char *name, int gap);  // 換場景（0x579df0）：mode = G_Type；name = music.txt 第 2 欄；gap = 第 3 欄非空
void audio_music_tick(int force);     // 每拍一次（0x57a010）
void audio_music_special(const char *name);  // credit／start／dead（0x57a910）
void audio_music_pop(void);           // 特殊曲收尾還原（0x57ab50）
void audio_music_sim_end(void);       // 測試：沒有裝置時結束「正在播」
const char *audio_music_now(void);    // 正在播的曲名（沒在播 = ""）
int audio_music_mode(void);           // 目前場景種類（G_Type）
void audio_update(int network_idle);  // 每幀呼叫（網頁版：網路空閒才開始下載音樂，下載好了就播）
void audio_sfx(int id);              // snd.lib 的第 id 個音效
int audio_voice_state(void);          // 0 沒語音 / 1 下載中 / 2 播放中（過場對話框靠它決定何時收）
float audio_voice_seconds(void);      // 目前這句語音的長度（秒，還沒載好 = 0）
void audio_voice(int key);            // 對話語音 game/sound/<key %03d>.wav（一次一句，沒有檔 = 停掉上一句）
void audio_loop(int id, int pct);    // 環境音循環播放（同時一個），pct = 音量 %，0 = 停
void audio_quit(void);
extern int audio_trace;
