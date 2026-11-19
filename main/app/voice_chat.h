#pragma once

#include <stdbool.h>

#define VOICE_QUESTION_BYTES 256
#define VOICE_ANSWER_BYTES 481

typedef struct {
    const char *status;  /* Static message, valid after the queue copy. */
    char question[VOICE_QUESTION_BYTES];
    char answer[VOICE_ANSWER_BYTES];
    bool finished;
} voice_chat_update_t;

/* Call from the UI task. Press toggles recording, and is ignored during upload. */
void voice_chat_press(void);
bool voice_chat_poll(voice_chat_update_t *update);
