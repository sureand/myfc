#include "audio.h"
#include "apu.h"

SDL_AudioDeviceID audio_device;

int setup_sdl_audio()
{
    SDL_AudioSpec desired_spec;
    SDL_zero(desired_spec);

    desired_spec.freq = 44100;          // 采样率
    desired_spec.format = AUDIO_S16SYS; // 16-bit PCM
    desired_spec.channels = 1;          // 单声道
    desired_spec.samples = 512;         // 每次回调的样本数量
    desired_spec.callback = NULL;       // 使用 `SDL_QueueAudio`

    // 打开音频设备
    audio_device = SDL_OpenAudioDevice(NULL, 0, &desired_spec, NULL, 0);
    if (audio_device == 0) {
        printf("Failed to open audio device: %s\n", SDL_GetError());
        return -1;
    }

    // 启动音频播放
    SDL_PauseAudioDevice(audio_device, 0);

    return 0;
}

void cleanup_sdl_audio()
{
    // 关闭音频设备
    SDL_CloseAudioDevice(audio_device);
}

void queue_audio_sample()
{
    int16_t sample = generate_audio_sample();
    int16_t pcm_sample = (int16_t)(sample & 0xFFFF);

    // 将 PCM 样本写入 SDL 音频缓冲区
    SDL_QueueAudio(audio_device, &pcm_sample, sizeof(pcm_sample));
}

// clamp 函数，用于限制音频输出的范围
float clamp(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;

    return value;
}

int16_t generate_audio_sample()
{
    float pulse1 = calculate_pulse_waveform(1);  // 矩形波 1
    float pulse2 = calculate_pulse_waveform(2);  // 矩形波 2
    float triangle = calculate_triangle_waveform();  // 三角波
    float noise = calculate_noise_waveform();  // 噪声
    float dmc = calculate_dmc_waveform();  // DMC

    // 计算脉冲波输出
    float pulse_sum = pulse1 + pulse2;
    float pulse_out = (pulse_sum == 0) ? 0.0f : 95.88f / ((8128.0f / pulse_sum) + 100.0f);

    // 计算TND输出（三角波，噪声，DMC）
    float tnd_denominator = (triangle / 8227.0f) + (noise / 12241.0f) + (dmc / 22638.0f);
    float tnd_out = (tnd_denominator == 0) ? 0.0f : 159.79f / (1.0f / tnd_denominator + 100.0f);

    // 混合最终的输出
    float output = pulse_out + tnd_out;

    // 限制输出值到 [-1.0, 1.0] 范围，防止音量过大导致失真
    output = clamp(output, -1.0f, 1.0f);

    // 将浮点数 [-1.0, 1.0] 转换为整数 [-32768, 32767]
    int16_t pcm_sample = (int16_t)(output * 32767.0f);

    return pcm_sample;
}