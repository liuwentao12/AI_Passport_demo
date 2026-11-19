#pragma once

#if __has_include("voice_config.local.h")
#include "voice_config.local.h"
#else
#include "voice_config.example.h"
#endif

#ifndef VOICE_CHAT_MODEL
#define VOICE_CHAT_MODEL "Qwen/Qwen2.5-7B-Instruct"
#endif
