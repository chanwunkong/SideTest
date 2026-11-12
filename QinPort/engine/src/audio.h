#pragma once

// 音量是原版 ini/Config.ini 的百分比（音乐音量、音效音量）
void audio_init(int music_percent, int sfx_percent);  // 先 res_init
void audio_music(const char *name);  // Music/<name>.wav 循環播放；"" = 停止
void audio_update(int network_idle);  // 每幀呼叫（網頁版：網路空閒才開始下載音樂，下載好了就播）
void audio_sfx(int id);              // sound/<id 三位數>.wav
void audio_quit(void);
