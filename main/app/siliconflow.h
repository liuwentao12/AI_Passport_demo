#pragma once

#include <stddef.h>

/* On failure, returns a short displayable message. Transcribe owns/free(wav). */
const char *siliconflow_transcribe(void *wav, size_t bytes, char *text, size_t capacity);
const char *siliconflow_answer(const char *text, char *answer, size_t capacity);
