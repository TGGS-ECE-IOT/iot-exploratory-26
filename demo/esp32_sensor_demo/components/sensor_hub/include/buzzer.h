#ifndef BUZZER_H
#define BUZZER_H

typedef enum {
    SONG_NONE = 0,
    SONG_HAPPY_BIRTHDAY,
    SONG_JINGLE_BELLS,
    SONG_LOY_KRATHONG
} buzzer_song_id_t;

void buzzer_init(void);
void buzzer_task(void *arg);

void buzzer_play_ok(void);
void buzzer_play_ok_short(void);
void buzzer_play_warn(void);
void buzzer_play_warn_short(void);
void buzzer_play_bt(void);
void buzzer_play_song(buzzer_song_id_t song_id, const char *command_id);
void buzzer_stop(void);

#endif
