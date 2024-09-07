#include "audio.h"

int init_audio(SDL_AudioSpec* audio_spec, audio_callback callback)
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    audio_spec->freq = 44100;
    audio_spec->format = AUDIO_F32SYS;
    audio_spec->channels = 1;
    audio_spec->samples = 512;
    audio_spec->callback = callback;
    audio_spec->userdata = NULL;

    if (SDL_OpenAudio(audio_spec, NULL) < 0) {
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
        return -1;
    }

    return 0;
}

//测试: 简单的三角波生成
float generate_audio_sample()
{
    static float phase = 0.0f;
    phase += (440.0f / 44100.0f);

    if (phase >= 1.0f) {
        phase -= 1.0f;
    }

    return (phase < 0.5f) ? (phase * 2.0f - 1.0f) : ((1.0f - phase) * 2.0f - 1.0f);
}

void _audio_callback(void* userdata, Uint8* stream, int len)
{
    SDL_memset(stream, 0, len);
    float* buffer = (float*)stream;
    int samples = len / sizeof(float);

    for (int i = 0; i < samples; ++i) {
        buffer[i] = generate_audio_sample();  // 生成 APU 的音频样本
    }
}