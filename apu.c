#include "apu.h"

PULSE_CHANNEL pulses[2];
TRIANGLE_CHANNEL triangle1;
NOISE_CHANNEL noise1;
uint8_t frame_counter;
DMC_CHANNEL dmc1;

//引用 https://www.nesdev.org/wiki/APU_Length_Counter
static const uint8_t length_table[32] = {
    10, 254, 20, 2, 40, 4, 80, 6,
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

// 占空比表
static const uint8_t duty_table[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},  // 12.5% 占空比
    {0, 1, 1, 0, 0, 0, 0, 0},  // 25% 占空比
    {0, 1, 1, 1, 1, 0, 0, 0},  // 50% 占空比
    {1, 0, 0, 1, 1, 1, 1, 1},  // 75% 占空比
};

// 三角波的音频表, 引用 https://www.nesdev.org/wiki/APU_Triangle
static const int triangle_table[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

// 噪音周期表 https://www.nesdev.org/wiki/APU_Noise
static const int noise_period_table[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

// 启用矩形波通道，设置启用标志
void enable_pulse_channel(PULSE_CHANNEL *channel)
{
    // 设置启用标志，通道被启用时允许长度计数器倒计时
    channel->enabled = 1;
}

// 禁用矩形波通道，停止长度计数器的倒计时
void disable_pulse_channel(PULSE_CHANNEL *channel)
{
    // 禁用标志，通道被禁用时长度计数器不再递减
    channel->enabled = 0;

    // 根据 NES APU 规则，当通道被禁用时立即将长度计数器设为0
    channel->length_counter = 0;
}

void update_pulse_period(PULSE_CHANNEL *pulse)
{
    if (pulse->sweep.shift == 0) {
        return;
    }

    // 当扫频计数器为0时，应用扫频效果
    pulse->sweep.period = pulse->sweep.shift;  // 重置周期为移位值

    // 计算扫频的频率调整
    uint16_t target_freq = pulse->timer >> pulse->sweep.shift;  // 移位调整频率

    if (pulse->sweep.negate) {
        // 扫频负向：计算负向时的频率
        pulse->sweep.target_period = pulse->timer - target_freq;

        // 额外处理通道1负向扫频的特殊情况
        if (pulse == &pulses[0]) {  // 如果是通道 1
            pulse->sweep.target_period--;  // 扫频时额外减去 1
        }
    } else {
        // 扫频正向：增加频率
        pulse->sweep.target_period = pulse->timer + target_freq;
    }

    // 确保频率在合法范围内
    if (pulse->sweep.target_period >= 8 && pulse->sweep.target_period <= 0x7FF) {
        pulse->timer = pulse->sweep.target_period;
    }
}

void update_sweep(PULSE_CHANNEL *pulse)
{
     pulse->sweep.period--;
     if (pulse->sweep.period == 0 && pulse->sweep.enable) {
        update_pulse_period(pulse);
     }

    if (pulse->sweep.reload) {
        // 如果扫频需要重置，重置扫频计数器并标记重载完成
        pulse->sweep.reload = 0;
        pulse->sweep.period = pulse->sweep.period;  // 重置扫频周期
    }
}

// 更新脉冲波通道的长度计数器
void update_pulse_length_counter(PULSE_CHANNEL *pulse)
{
    // 如果“暂停”标志为 false 并且长度计数器大于 0，计数器递减
    if (pulse->enabled && !pulse->halt && pulse->length_counter > 0) {
        pulse->length_counter--;  // 长度计数器递减
    }
}

// 更新脉冲波的包络
void update_pulse_envelope(PULSE_CHANNEL *pulse)
{
    // 包络重新开始，重置计数器和分频器
    if (pulse->envelope_start) {
        pulse->envelope_counter = 15;
        pulse->envelope_divider = pulse->envelope_period;
        pulse->envelope_start = FALSE;
    } else {
        if (pulse->envelope_divider > 0) {
            pulse->envelope_divider--;
        } else {
            // 当分频器到达 0，重置分频器并递减包络计数器
            pulse->envelope_divider = pulse->envelope_period;
            if (pulse->envelope_counter > 0) {
                pulse->envelope_counter--;
            } else if (pulse->loop_flag) {
                pulse->envelope_counter = 15;  // 如果循环标志为真，包络计数器重新加载
            }
        }
    }
}

void update_pulse_timer(uint8_t channel)
{
    PULSE_CHANNEL *pulse = &pulses[channel & 1];

    if (pulse->timer > 0) {
        pulse->timer--;  // 递减定时器计数器
    } else {
        pulse->timer = pulse->timer_period;  // 当计数器为0时，重置计数器
        pulse->duty_step = (pulse->duty_step + 1) & 7;  // 更新占空比步进，循环在8个步骤内
    }
}

uint8_t calculate_pulse_waveform(uint8_t channel)
{
    PULSE_CHANNEL *pulse = &pulses[channel & 1];

    // 1. 检查长度计数器，如果为 0，则不输出信号
    if (pulse->length_counter == 0) {
        return 0;
    }

    // 2. 检查频率值是否太低或太高，防止不可听频率
    if (pulse->timer < 8 || pulse->sweep.target_period > 0x7FF) {
        return 0;
    }

    uint8_t duty_output = duty_table[pulse->duty][pulse->duty_step];
    int volume = pulse->constant_volume ? pulse->volume : pulse->envelope_counter;

    return duty_output ? volume : 0;
}

// 启用三角波通道
void enable_triangle_channel(TRIANGLE_CHANNEL* channel)
{
    // 检查是否需要重置长度计数器
    if (!channel->halt && channel->length_counter == 0) {
        channel->length_counter = channel->initial_length;  // 重置长度计数器
    }

    // 重置线性计数器
    channel->linear_counter = channel->initial_linear_counter;

    // 重置 reload 标志
    channel->reload = 1;

    // 清除三角波输出状态
    channel->step = 0;            // 重置三角波步进
    channel->output = 0;          // 清除当前输出

    // 重置定时器
    channel->timer = 0;  // 重置定时器到初始状态
}

// 禁用三角波通道
void disable_triangle_channel(TRIANGLE_CHANNEL* channel)
{
    // 设置 halt 标志以停止长度计数器的递减
    channel->halt = 1;

    channel->length_counter = 0;

    // 清空线性计数器
    channel->linear_counter = 0;

    // 停止定时器
    channel->timer = 0;

    // 重置步进状态
    channel->step = 0;
}

// 更新三角波的“包络” —— 注意：三角波使用线性计数器，不是真正的包络
void update_triangle_envelope(TRIANGLE_CHANNEL *triangle)
{
    if (triangle->reload) {
        // 如果线性计数器重载标志为真，重置线性计数器
        triangle->linear_counter = triangle->initial_linear_counter;
    }

    if (triangle->linear_counter > 0) {
        triangle->linear_counter--;
    }

    if (!triangle->halt) {
        triangle->reload = FALSE;  // 禁用自动重置
    }
}

// 更新三角波通道的长度计数器
void update_triangle_length_counter(TRIANGLE_CHANNEL *triangle)
{
    ///如果 halt 标志为 false 并且长度计数器大于 0，计数器递减
    if (!triangle->halt && triangle->length_counter > 0) {
        triangle->length_counter--;
    }
}

void update_triangle_timer()
{
    TRIANGLE_CHANNEL *triangle = &triangle1;

    if (triangle->length_counter == 0 || triangle->linear_counter == 0) {
        return;
    }

    if (triangle->timer > 0) {
        triangle->timer--;  // 定时器递减
    } else {
        // 当定时器到达 0 时，重置定时器并更新波形
        triangle->timer = triangle->timer_period;  // 重新加载定时器值
        triangle->step = (triangle->step + 1) & 31;
    }
}

uint8_t calculate_triangle_waveform()
{
    TRIANGLE_CHANNEL *triangle = &triangle1;

    // 解决频率过低输出爆破音的问题
    if (triangle->timer_period < 2) {
        triangle->linear_counter = 0;
        return triangle->output;
    }

    // 如果线性计数器或长度计数器为 0，则无输出
    if (triangle->linear_counter == 0 || triangle->length_counter == 0) {
        triangle->output = 0;
        return triangle->output;
    }

    triangle->output = triangle_table[triangle->step];

    return triangle->output;
}

void enable_noise_channel(NOISE_CHANNEL* channel)
{
    // 启用噪声通道时不立即重置长度计数器，长度计数器由寄存器控制
    if (channel->length_counter == 0 && !channel->halt) {
        // 如果长度计数器为 0 并且 halt 标志未设置时，通道停止
        return;
    }

    // 如果通道需要重新启用，可能需要处理包络的启动
    if (channel->envelope_start) {
        channel->envelope_start = FALSE;  // 清除 envelope_start 标志
        channel->envelope_counter = 15;   // 重置包络计数器
        channel->envelope_divider = channel->envelope_period;  // 重置包络分频器
    }
}

void disable_noise_channel(NOISE_CHANNEL* channel)
{
    // 停止噪声通道，长度计数器清零
    channel->length_counter = 0;

    // 确保包络计数器停止，或者在包络上设置标志停止声音输出
    channel->envelope_start = FALSE;
    channel->envelope_counter = 0;
    channel->envelope_divider = 0;

    // 清除噪声输出
    channel->output = 0;
}

// 更新噪声通道的长度计数器
void update_noise_length_counter(NOISE_CHANNEL *noise)
{
    // 如果“暂停”标志为 false 并且长度计数器大于 0，计数器递减
    if (!noise->halt && noise->length_counter > 0) {
        noise->length_counter--;
    }
}

// 更新噪声通道的包络
void update_noise_envelope(NOISE_CHANNEL *noise)
{
    if (noise->envelope_start) {
        // 包络重新开始，重置计数器和分频器
        noise->envelope_counter = 15;
        noise->envelope_divider = noise->envelope_period;
        noise->envelope_start = FALSE;
    } else {
        if (noise->envelope_divider > 0) {
            noise->envelope_divider--;
        } else {
            // 当分频器到达 0，重置分频器并递减包络计数器
            noise->envelope_divider = noise->envelope_period;
            if (noise->envelope_counter > 0) {
                noise->envelope_counter--;
            } else if (noise->loop_flag) {
                noise->envelope_counter = 15;  // 如果循环标志为真，包络计数器重新加载
            }
        }
    }
}

void update_noise_shift_register(NOISE_CHANNEL *noise)
{
    uint8_t mode = noise->mode_flag ? 6 : 1;
    uint16_t feedback = (noise->shift_register & 0x01) ^ ((noise->shift_register >> mode) & 0x01);

    // 右移 LFSR，并将反馈位写入最高位（第 14 位）
    noise->shift_register >>= 1;
    noise->shift_register |= (feedback << 14);
}

void update_noise_timer()
{
    NOISE_CHANNEL *noise = &noise1;

    // 更新定时器，控制噪声生成频率
    if (noise->timer > 0) {
        noise->timer--;
    }else {
        noise->timer = noise->period;  // 重置定时器为周期值

        // 伪随机序列生成
        update_noise_shift_register(noise);
    }
}

uint8_t calculate_noise_waveform()
{
    NOISE_CHANNEL *noise = &noise1;

    if (noise->length_counter == 0) {
        return 0;
    }

    uint8_t volume = noise->constant_volume ? noise->volume : noise->envelope_counter;

    // 根据移位寄存器的最低位决定是否输出声音
    noise->output = (noise->shift_register & 1) ? 0 : volume;

    // 返回最终的音量输出
    return noise->output;
}

void enable_dmc_channel(DMC_CHANNEL *dmc)
{
    // 设置起始地址
    dmc->current_address = 0xC000 + (dmc->address << 6);  // 左移6位相当于乘以64
    dmc->start_address = dmc->current_address;

    // 设置采样长度
    dmc->remaining_bytes = (dmc->length << 4) + 1;  // 左移4位相当于乘以16
    dmc->length = dmc->remaining_bytes;

    // 重置其他参数
    dmc->volume = dmc->load & 0x7F;  // 确保音量在0-127之间
    dmc->remaining_bits = 0;  // 在第一次读取字节时会被设置为8
    dmc->active = TRUE;
    dmc->irq_flag = FALSE;
}

void disable_dmc_channel(DMC_CHANNEL* dmc)
{
    // 停止 DMC 通道，停止采样和播放
    dmc->active = FALSE;
}

void update_dmc(DMC_CHANNEL *dmc)
{
    // 如果没有剩余字节并且DMC没有在播放，则返回静音
    if (dmc->remaining_bits == 0 && dmc->remaining_bytes == 0) {
        return;
    }

    // 如果所有位都处理完毕，读取下一个字节
    if (dmc->remaining_bits == 0) {
        if (dmc->remaining_bytes > 0) {
            dmc->current_address = cpu_read(0xC000 + (dmc->address * 64)); // 计算DMC起始地址
            dmc->remaining_bits = 8;  // 每个字节有8位
            dmc->address++;  // 增加地址

            // 处理地址溢出
            if (dmc->address > 0xFFFF) {
                dmc->address = 0x8000;
            }

            dmc->remaining_bytes--;
        } else {
            // 如果没有剩余字节并且不循环，返回静音
            if (!dmc->loop) {
                return;
            } else {
                // 启用循环时，重置地址和长度
                dmc->address = 0x8000 + (dmc->address * 64);  // 使用偏移后的地址
                dmc->remaining_bytes = dmc->length * 16;  // 重置采样长度
            }
        }
    }

    // 处理当前字节的当前位
    if (dmc->current_address & 1) {
        if (dmc->load <= 125) {
            dmc->load += 2;  // 当前位是1时增加音量
        }
    } else {
        if (dmc->load >= 2) {
            dmc->load -= 2;  // 当前位是0时减少音量
        }
    }

    // 移动到下一个位
    dmc->current_address >>= 1;
    dmc->remaining_bits--;
}

uint8_t calculate_dmc_waveform()
{
    // 将音量转换为浮点数进行波形输出，范围 [0.0, 1.0]
    return dmc1.load;
}

void pulse_write(PULSE_CHANNEL *pulse, uint16_t reg, uint8_t data)
{
    switch (reg) {
        // 0x4000: 控制包络和占空比
        case 0x0000:
            pulse->duty = (data >> 6) & 0x03;  // 占空比设置 (2 位)
            pulse->loop_flag = (data >> 5) & 0x01;   // Length counter halt (或 envelope loop flag)
            pulse->constant_volume = (data >> 4) & 0x01;  // 固定音量或包络使能, 使用 constant_volume_flag
            pulse->volume = data & 0x0F;  // 音量或包络的初始值 (4 位)
            break;

        // 0x4001: 控制扫频单元（Sweep Unit）
        case 0x0001:
            pulse->sweep.enable = (data >> 7) & 0x01;  // 是否启用扫频
            pulse->sweep.period = (data >> 4) & 0x07;  // 扫频周期 (3 位)
            pulse->sweep.negate = (data >> 3) & 0x01;  // 反转标志 (增大或减小频率)
            pulse->sweep.shift = data & 0x07;  // 扫频移位计数 (3 位)
            pulse->sweep.reload = 1;  // 每次写入时重置扫频单元
            break;

        // 0x4002: 控制定时器低 8 位
        case 0x0002:
            pulse->timer = (pulse->timer & 0xFF00) | data;  // 设置定时器的低 8 位
            break;

        // 0x4003: 控制定时器高 3 位和长度计数器
        case 0x0003:
            pulse->timer = (pulse->timer & 0x00FF) | ((data & 0x07) << 8);  // 设置定时器的高 3 位
            pulse->timer_period = pulse->timer;  // 保存定时器周期
            pulse->length_counter = length_table[data >> 3];  // 通过写入的值索引到长度计数器表
            pulse->envelope_start = 1;  // 包络启动
            break;

        default:
            break;
    }
}

void triangle_write(TRIANGLE_CHANNEL *triangle, uint16_t reg, uint8_t data)
{
    switch (reg) {
        // 0x4008: 线性计数器控制
        case 0x0000:
            triangle->halt = (data & 0x80);  // 最高位表示是否控制长度计数器
            triangle->initial_linear_counter = data & 0x7F;  // 线性计数器重载值 (7 位)
            triangle->reload = 1;  // 标记线性计数器需要重置
            break;

        // 0x400A: 定时器低 8 位
        case 0x0002:
            triangle->timer = (triangle->timer & 0xFF00) | data;  // 设置定时器的低 8 位
            break;

        // 0x400B: 定时器高 3 位和长度计数器
        case 0x0003:
            triangle->timer = (triangle->timer & 0x00FF) | ((data & 0x07) << 8);  // 设置定时器的高 3 位
            triangle->length_counter = length_table[(data >> 3) & 0x1F];  // 通过高 5 位索引长度计数器表
            triangle->reload = 1;  // 重置线性计数器
            break;

        default:
            break;
    }
}

void noise_write(NOISE_CHANNEL *noise, uint16_t reg, uint8_t data)
{
    switch (reg) {
        case 0:  // 地址 0x400C：控制包络和音量
            noise->constant_volume = (data & 0x10) ? 1 : 0;  // 第 4 位：是否使用固定音量
            noise->volume = data & 0x0F;  // 第 0-3 位：固定音量（或包络周期值）
            noise->envelope_period = data & 0x0F;  // 第 0-3 位：包络周期（4 位）
            noise->halt = data & 0x20;             // 第 5 位：长度计数器停止（也控制包络循环）
            break;

        case 1:  // 地址 0x400D：未使用，无操作
            break;

        case 2:  // 地址 0x400E：控制噪声模式和频率
            noise->mode_flag = data & 0x80;        // 第 7 位：噪声模式，1 为短模式，0 为长模式
            noise->period = noise_period_table[data & 0x0F];  // 从周期表中查找实际的噪声频率
            break;

        case 3:  // 地址 0x400F：控制长度计数器加载值和包络启动
            noise->length_counter = length_table[(data & 0xF8) >> 3];  // 第 3-7 位：长度计数器
            noise->envelope_start = TRUE;                               // 启动包络
            break;
    }
}

uint8_t read_4015()
{
    uint8_t status = 0;

    // 位 0: 矩形波 1 的长度计数器大于 0
    if (pulses[0].length_counter > 0) {
        status |= 0x01;
    }

    // 位 1: 矩形波 2 的长度计数器大于 0
    if (pulses[1].length_counter > 0) {
        status |= 0x02;
    }

    // 位 2: 三角波的长度计数器大于 0
    if (triangle1.length_counter > 0) {
        status |= 0x04;
    }

    // 位 3: 噪声通道的长度计数器大于 0
    if (noise1.length_counter > 0) {
        status |= 0x08;
    }

    // 位 4: DMC 正在播放
    if (dmc1.remaining_bytes > 0) {
        status |= 0x10;
    }

    // 位 6: 帧中断标志
    if (apu.frame_interrupt_flag) {
        status |= 0x40;
    }

    // 清除帧中断标志（读取时自动清除）
    apu.frame_interrupt_flag = FALSE;

    return status;
}

SDL_bool is_apu_address(WORD address)
{
    if (address >= 0x4000 && address <= 0x4017 && address != 0x4014 && address != 0x4016) {
        return TRUE;
    }

    return FALSE;
}

void dmc_write(DMC_CHANNEL *dmc, uint16_t index, BYTE data)
{
    switch (index) {
        case 0:
            dmc->irq_flag = data & 0x80;
            dmc->loop = data & 0x40;
            dmc->rate_index = data & 0xF;
            break;
        case 1:
            dmc->load = data & 0xFF;
        break;
        case 2:
            dmc->address = data & 0xFF;
        break;
        case 3:
            dmc->length = data & 0xFF;
        break;
        default: break;
     }
}

// 重置 APU 帧计数器
void reset_apu_frame_counter()
{
    apu.frame_counter = 0;  // 将帧计数器重置为 0
    apu.frame_step = 0;
    apu.frame_counter_reset = TRUE;  // 标记帧计数器已重置
}

void update_envelopes()
{
    update_pulse_envelope(&pulses[0]);
    update_pulse_envelope(&pulses[1]);
    update_noise_envelope(&noise1);
    update_triangle_envelope(&triangle1);
}

void update_length_counters()
{
    // 更新脉冲波、三角波、噪声通道的长度计数器
    update_pulse_length_counter(&pulses[0]);
    update_pulse_length_counter(&pulses[1]);
    update_triangle_length_counter(&triangle1);
    update_noise_length_counter(&noise1);
}

void trigger_frame_interrupt()
{
    // 设置 CPU 中断标志，或者其他需要执行的操作
    set_irq();
}

void update_sweep_units()
{
    update_sweep(&pulses[0]);
    update_sweep(&pulses[1]);
}

void step_apu_frame_counter()
{
    int step_mode = apu.mode ? 5: 4;

    apu.frame_step = (apu.frame_step % step_mode) + 1; // 循环步进
    apu.frame_counter++;  // 更新帧计数器

    if (apu.mode == 4) {
        switch (apu.frame_step) {
            case 1:
                // 第 1 步：更新包络
                update_envelopes();
                break;
            case 2:
                // 第 2 步：更新包络、长度计数器和扫频单元
                update_envelopes();
                update_length_counters();
                update_sweep_units();
                break;
            case 3:
                // 第 3 步：更新包络
                update_envelopes();
                break;
            case 4:
                // 第 4 步：更新包络、长度计数器、扫频单元，并产生中断
                update_envelopes();
                update_length_counters();
                update_sweep_units();

                // 如果启用了中断，产生帧中断
                if (apu.frame_interrupt_enabled) {
                    apu.frame_interrupt_flag = TRUE;  // 设置帧中断标志
                    trigger_frame_interrupt();  // 调用触发中断的函数
                }
                break;
        }
        return;
    }

    // 处理 5 步模式的帧步进
    switch (apu.frame_step) {
        case 1:
            // 第 1 步：更新包络、长度计数器和扫频单元
            update_envelopes();  // 更新所有通道的包络
            break;
        case 2:
            // 第 2 步：更新长度计数器和扫频单元
            update_envelopes();  // 更新所有通道的包络
            update_length_counters();
            update_sweep_units();
            break;
        case 3:
            // 第 3 步：再次更新包络、长度计数器和扫频单元
            update_envelopes();
            break;
        case 4:
            break;
        case 5:
            // 第 5 步：不产生中断，只是更新包络和长度计数器
            update_envelopes();
            update_length_counters();
            update_sweep_units();
            break;
    }
}

void write_4017(BYTE data)
{
    apu.mode = (data & 0x80) != 0;  // 检查位 7，是否启用 5 步模式
    SDL_bool interrupt_inhibit = (data & 0x40) != 0;  // 检查位 6，是否禁用帧中断
    SDL_bool reset_frame_counter = (data & 0x01) != 0;  // 检查位 0，是否重置帧计数器

    // 2. 更新帧中断的启用状态
    if (interrupt_inhibit) {
        // 禁用帧中断，并清除现有的帧中断标志
        apu.frame_interrupt_enabled = FALSE;
        apu.frame_interrupt_flag = FALSE;
    } else {
        // 启用帧中断
        apu.frame_interrupt_enabled = TRUE;
    }

    // 3. 如果需要，重置帧计数器并立即步进一次
    if (reset_frame_counter) {
        // 重置帧计数器，并根据当前模式立即步进一次
        reset_apu_frame_counter();
        step_apu_frame_counter();  // 立即步进帧计数器，开始新的周期
    }
}

//引用参考 https://www.nesdev.org/wiki/Nerdy_Nights:_APU_overview
void write_4015(uint8_t value)
{
    // 矩形波 1 通道
    if (value & 0x01) {
        enable_pulse_channel(&pulses[0]);  // 启用 pulse[0]
    } else {
        disable_pulse_channel(&pulses[0]);  // 禁用 pulse[0]
    }

    // 矩形波 2 通道
    if (value & 0x02) {
        enable_pulse_channel(&pulses[1]);  // 启用 pulse[1]
    } else {
        disable_pulse_channel(&pulses[1]);  // 禁用 pulse[1]
    }

    // 三角波通道
    if (value & 0x04) {
        enable_triangle_channel(&triangle1);  // 启用 triangle
    } else {
        disable_triangle_channel(&triangle1);  // 禁用 triangle
    }

    // 噪声通道
    if (value & 0x08) {
        enable_noise_channel(&noise1);  // 启用 noise
    } else {
        disable_noise_channel(&noise1);  // 禁用 noise
    }

    // DMC 通道
    if (value & 0x10) {
        enable_dmc_channel(&dmc1);  // 启用 DMC
    } else {
        disable_dmc_channel(&dmc1);  // 禁用 DMC
    }
}

void update_frame_counter(BYTE data)
{
    frame_counter = (data & 0xC0) >> 7;
}

void apu_write(WORD address, BYTE data)
{
    switch (address) {
        case 0x4000: case 0x4001: case 0x4002: case 0x4003:
            // 控制脉冲波通道 1
            pulse_write(&pulses[0], address - 0x4000, data);
            break;
        case 0x4004: case 0x4005: case 0x4006: case 0x4007:
            // 控制脉冲波通道 2
            pulse_write(&pulses[1], address - 0x4004, data);
            break;
        case 0x4008: case 0x4009: case 0x400A: case 0x400B:
            // 控制三角波通道
            triangle_write(&triangle1, address - 0x4008, data);
            break;
        case 0x400C: case 0x400D: case 0x400E: case 0x400F:
            // 控制噪声通道
            noise_write(&noise1, address - 0x400C, data);
            break;
        case 0x4010: case 0x4011: case 0x4012: case 0x4013:
            // 控制 DMC 通道
            dmc_write(&dmc1, address - 0x4010, data);
            break;
        case 0x4015:
            // 更新 APU 状态寄存器，控制音频通道的启用或禁用
            write_4015(data);
            break;
        case 0x4017:
            // 控制帧计数器
            write_4017(data);
            break;
        default:
            printf("Unsupported APU write at address: %04X\n", address);
            break;
    }
}

BYTE apu_read(WORD address)
{
    switch (address) {
        case 0x4015:
            return read_4015();
        default:
            printf("Unsupported APU read at address: %04X\n", address);
             return 0xFF;  // 返回默认值
    }
}

void apu_init()
{
    // 初始化脉冲波通道1
    memset(&pulses, 0, sizeof(PULSE_CHANNEL) * 2);

    memset(&triangle1, 0, sizeof(TRIANGLE_CHANNEL));

    memset(&noise1, 0, sizeof(NOISE_CHANNEL));
    noise1.shift_register = 1;

    memset(&dmc1, 0, sizeof(DMC_CHANNEL));

    memset(&apu, 0, sizeof(apu));
}

// TODO: 该函数暂时废弃
void step_apu()
{
    //三角波的是两个apu 周期
    if (apu.cycle % 2 == 0) {
        update_triangle_timer();
    }

    //方波和噪音的timer 更新频率是4个cpu 周期
    if (apu.cycle % 4 == 0) {
        update_pulse_timer(0);
        update_pulse_timer(1);
        update_noise_timer();
    }

    // 帧计数器更新步长, 约14915 个 apu 周期更新一次
    if (apu.cycle % QUARTER_FRAME == 0) {
        step_apu_frame_counter();  // 更新帧计数器
    }

    //音频样本生成, 约每81个apu 周期采样一次
    if (apu.cycle % PER_SAMPLE == 0) {
        // 推送音频样本到缓冲区
        queue_audio_sample();
    }

    apu.cycle++;
}