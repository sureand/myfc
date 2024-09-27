#include "audio.h"
#include "apu.h"


int16_t sample_buffer[SAMPLE_BUFFER_SIZE];  // 样本缓冲区
int sample_buffer_index = 0;

SDL_AudioDeviceID audio_device;

int setup_sdl_audio()
{
    SDL_AudioSpec desired_spec;
    SDL_zero(desired_spec);

    desired_spec.freq = SOUND_FREQUENCY;  // 采样率
    desired_spec.format = AUDIO_S16SYS;   // 16-bit PCM
    desired_spec.channels = 1;            // 单声道
    desired_spec.samples = 512;           // 每次回调的样本数量
    desired_spec.callback = NULL;         // 使用 `SDL_QueueAudio`

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

// clamp 函数，用于限制音频输出的范围
float clamp(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;

    return value;
}

void queue_audio_sample()
{
    // FIXME: 由于cpu 的时钟周期实现的精度不够, 现在输出有杂音, 是否采用相位的计算音色会更好?
    uint8_t pulse1 = calculate_pulse_waveform(0);  // 矩形波 1
    uint8_t pulse2 = calculate_pulse_waveform(1);  // 矩形波 2
    uint8_t triangle = calculate_triangle_waveform();  // 三角波
    uint8_t noise = calculate_noise_waveform();  // 噪声
    uint8_t dmc = calculate_dmc_waveform();  // DMC

    //公式参考: https://www.nesdev.org/wiki/APU_Mixer

    // 计算 pulse_out
    float pulse_sum = (float)(pulse1 + pulse2);
    float pulse_out = (pulse_sum > 0) ? (95.88f / ((8128.0f / pulse_sum) + 100.0f)) : 0.0f;

    // 计算 tnd_out
    float tnd_sum = (triangle / 8227.0f) + (noise / 12241.0f) + (dmc / 22638.0f);
    float tnd_out = (tnd_sum > 0) ? (159.79f / ((1.0f / tnd_sum) + 100.0f)) : 0.0f;

    float output = pulse_out + tnd_out;

    // 限制输出值在 [-0.95, 0.95] 范围内，减少削波
    output = clamp(output, -0.95f, 0.95f);

    // 将浮点输出值转换为 16 位 PCM 格式
    int16_t sample = (int16_t)(output * 32767.0f);

    // 将 PCM 样本写入 SDL 音频缓冲区
    SDL_QueueAudio(audio_device, &sample, sizeof(sample));
}