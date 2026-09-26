#ifndef REPLAY_H
#define REPLAY_H

#define REPLAY_NONE 0
#define REPLAY_RECORDING 1
#define REPLAY_PLAYING 2

extern int replay_mode;

void replay_tick(void);
int replay_is_key(int key);
void replay_toggle_r(void);
void replay_stop_recording(void);
void replay_start_recording(void);
void replay_start_playback(void);
void replay_draw_osd(void);
void replay_save(void);
int replay_has_buffer(void);
int replay_exists(void);
void replay_delete(void);
int replay_load(void);

#endif
