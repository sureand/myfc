#ifndef __APU_HEADER
#define __APU_HEADER
#include "common.h"
#include "SDL2/SDL.h"

#define PULSE_COUNT (2)
#define SAMPLE_BUFFER_SIZE (512)  //缓冲区大小
#define SOUND_FREQUENCY (44100)   //音频采样率
#define APU_FREQUENCY (3579545) //apu 的运行频率
#define PER_SAMPLE (APU_FREQUENCY / SOUND_FREQUENCY) // APU 频率与音频采样率 ~=81

/*
* apu 序列器的一帧的周期是29830 cpu 周期, 序列器的频率只有cpu 的一半, 因此是4个apu 周期
* 每个frame 分为四步, 因此是 (29830 * 2) / 4 = 14915
* 引用 https://www.nesdev.org/wiki/APU_Frame_Counter, 注意文档上面的apu cycle 指的仅仅是序列器的cycle 只有真实apu 频率的四分之一
*/
#define QUARTER_FRAME (14915) //四分之一帧

typedef struct {
    uint8_t enable;          // 是否启用扫频
    uint8_t period;          // 扫频周期
    uint16_t target_period;  // 扫频器目标周期
    uint8_t negate;          // 是否反转频率变化（负向）
    uint8_t shift;           // 扫频移位位数
    uint8_t reload;          // 是否重置扫频计数器
} SWEEP;

typedef struct {
    uint8_t enabled;           // 是否启用
    uint16_t timer;            // 当前定时器值，控制波形频率（11 位）
    uint16_t timer_period;     // 定时器周期，用于控制频率
    uint8_t halt;              // 停止标志，影响长度计数器是否倒计时
    uint8_t constant_volume;   // 是否启用固定音量
    uint8_t volume;            // 音量（4 位，范围0-15）
    uint8_t duty;              // 占空比，2 位（4种模式）
    uint8_t duty_step;         // 占空比步进
    uint16_t length_counter;   // 长度计数器
    uint8_t envelope_start;    // 是否重置包络
    uint8_t envelope_counter;  // 当前包络计数器
    uint8_t envelope_period;   // 包络周期
    uint8_t envelope_divider;  // 包络分频器
    uint8_t output;            // 当前的输出值（0或1）
    uint8_t loop_flag;         // 包络循环标志，是否循环播放
    SWEEP sweep;               // 扫频单元
} PULSE_CHANNEL;

typedef struct
{
    uint8_t halt;             // halt 标志
    uint16_t initial_length;  // 初始长度，用于重置 length_counter
    uint16_t length_counter;  // 长度计数器，控制三角波的播放时间
    uint16_t timer;           // 定时器，用于控制波形频率
    uint8_t reload;           // 线性计数器重置标志
    uint16_t timer_period;    // 用于存储来自寄存器 $400A 和 $400B 的周期值
    uint8_t step;             // 当前三角波的步进值 (0-15)
    uint8_t linear_counter;   // 线性计数器
    uint8_t initial_linear_counter; // 初始线性计数器值
    uint8_t output;           // 三角波输出值
} TRIANGLE_CHANNEL;

typedef struct {
    uint8_t length_counter;    // 长度计数器
    uint8_t halt;              // 是否停止长度计数器
    uint8_t timer;             // 定时器计数，用于控制噪声频率
    uint16_t period;           // 定时器的周期值
    uint16_t shift_register;   // 伪随机数生成的移位寄存器
    uint8_t mode_flag;         // 噪声模式（短模式或长模式）
    uint8_t output;            // 当前噪声的输出值（0或1）

    uint8_t envelope_counter;  // 包络计数器
    uint8_t envelope_divider;  // 包络分频器
    uint8_t envelope_period;   // 包络的周期
    uint8_t envelope_start;    // 包络启动标志
    uint8_t constant_volume;   // 是否使用固定音量
    uint8_t volume;            // 是否使用固定音量
    uint8_t loop_flag;         // 是否循环包络
} NOISE_CHANNEL;

typedef struct
{
    uint8_t irq_flag;
    uint16_t loop;
    uint8_t rate_index;
    uint8_t load;
    uint8_t volume;
    uint16_t address;
    uint8_t length;
    uint8_t interrupt_flag;
    uint8_t active;
    uint8_t remaining_bits;
    uint8_t remaining_bytes;
    uint16_t current_address;
    uint16_t start_address;
}DMC_CHANNEL;

uint8_t calculate_pulse_waveform(uint8_t channel);
uint8_t calculate_noise_waveform();
uint8_t calculate_triangle_waveform();
uint8_t calculate_dmc_waveform();

#endif