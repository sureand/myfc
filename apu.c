#include "apu.h"

PULSE_CHANNEL pulses[2];
TRIANGLE_CHANNEL triangle1;
NOISE_CHANNEL noise1;
uint8_t frame_counter;
DMC_CHANNEL dmc1;

const uint8_t length_table[32] = {
    10, 254, 20, 2, 40, 4, 80, 6,
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

PULSE_CHANNEL get_pulse(uint16_t channel)
{
    channel &= 2;
    return pulses[channel - 1];
}

NOISE_CHANNEL *get_noise()
{
    return &noise1;
}

DMC_CHANNEL *get_dmc()
{
    return &dmc1;
}

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

void update_sweep(PULSE_CHANNEL *pulse)
{
    if (!pulse->sweep.enable) {
        return;
    }

    if (pulse->sweep.reload) {
        // 如果扫频需要重置，重置扫频计数器
        pulse->sweep.reload = 0;
    } else {
        // 扫频计数器递减
        if (pulse->sweep.period > 0) {
            pulse->sweep.period--;
        } else {
            // 当扫频计数器为0时，应用扫频效果
            pulse->sweep.period = pulse->sweep.shift;

            // 计算扫频的频率调整
            uint16_t target_freq = pulse->timer >> pulse->sweep.shift;
            if (pulse->sweep.negate) {
                target_freq = pulse->timer - target_freq;
            } else {
                target_freq = pulse->timer + target_freq;
            }

            // 只在频率有效范围内应用扫频
            if (target_freq >= 8 && target_freq <= 0x7FF) {
                pulse->timer = target_freq;
            }
        }
    }
}

void do_update_pulse(PULSE_CHANNEL *pulse)
{
    // 1. 更新长度计数器
    // 只有在通道启用且没有“暂停（halt）”时，才会递减长度计数器
    if (pulse->enabled && !pulse->halt && pulse->length_counter > 0) {
        pulse->length_counter--;  // 长度计数器递减
    }

    // 2. 更新频率计数器，控制波形周期
    if (pulse->timer > 0) {
        pulse->timer--;  // 递减定时器计数器
    } else {
        pulse->timer = pulse->timer_period;  // 当计数器为0时，重置计数器
        pulse->duty = (pulse->duty + 1) % 8;  // 更新占空比步进，循环在8个步骤内
    }

    // 3. 更新包络 (Envelope)
    if (pulse->envelope_start) {
        // 重置包络
        pulse->envelope_counter = 15;
        pulse->envelope_divider = pulse->envelope_period;
        pulse->envelope_start = 0;  // 清除重置标志
    } else {
        if (pulse->envelope_divider > 0) {
            pulse->envelope_divider--;  // 递减包络分频器
        } else {
            // 当分频器到达0时，重置为包络周期
            pulse->envelope_divider = pulse->envelope_period;
            if (pulse->envelope_counter > 0) {
                pulse->envelope_counter--;  // 包络计数器递减
            } else if (pulse->constant_volume == 0 && pulse->loop_flag) {
                pulse->envelope_counter = 15;  // 如果启用了循环模式，包络计数器重置为15
            }
        }
    }

    // 4. 扫频更新 (Sweep unit)
    update_sweep(pulse);
}

void update_pulses()
{
    do_update_pulse(&pulses[0]);
    do_update_pulse(&pulses[1]);
}

float calculate_pulse_waveform(uint16_t channel)
{
    // 假设 channel 为 1 或 2，表示第一个或第二个矩形波通道
    static int phase1 = 0, phase2 = 0;  // 记录相位

    PULSE_CHANNEL pulse = get_pulse(channel);

    // 获取当前周期，定时器是 11 位
    int period = pulse.timer;
    if (period < 8) return 0.0f;  // NES 的矩形波周期最小值为 8

    // 获取占空比，不需要右移
    int duty = pulse.duty & 0x03;  // duty 现在是 0 到 3

    // 计算当前相位值
    int phase = (channel == 1) ? phase1 : phase2;
    phase++;
    if (phase >= period) phase = 0;

    // 占空比数组：{12.5%, 25%, 50%, 75%}
    static const float duty_table[4] = {0.125f, 0.25f, 0.5f, 0.75f};

    // 确保使用整数进行占空比比较
    int duty_period = duty_table[duty] * period;
    float output = (phase < duty_period) ? 1.0f : 0.0f;

    // 处理音量
    float volume = pulse.constant_volume ? pulse.volume / 15.0f : pulse.envelope_counter / 15.0f;
    output *= volume;  // 乘以音量

    // 更新相位
    if (channel == 1) phase1 = phase; else phase2 = phase;

    return output;
}

// 启用三角波通道
void enable_triangle_channel(TRIANGLE_CHANNEL* channel)
{
    // 检查是否需要重置长度计数器
    if (!(channel->control & 0x80) && channel->length_counter == 0) {
        channel->length_counter = channel->initial_length;  // 重置长度计数器
    }

    // 重置线性计数器
    channel->linear_counter = channel->initial_linear_counter;

    // 重置 reload 标志
    channel->reload = 1;

    // 清除三角波输出状态
    channel->step = 0;            // 重置三角波步进
    channel->step_direction = 1;  // 确保三角波初始为上升阶段
    channel->output = 0;          // 清除当前输出

    // 重置定时器
    channel->timer = 0;  // 重置定时器到初始状态
}

// 禁用三角波通道
void disable_triangle_channel(TRIANGLE_CHANNEL* channel)
{
    // 设置 halt 标志以停止长度计数器的递减
    channel->control |= 0x80;  // 设置控制位的最高位（halt 位）

    // 停止输出
    channel->output = 0;  // 停止三角波输出

    // 清空长度计数器
    channel->length_counter = 0;

    // 清空线性计数器
    channel->linear_counter = 0;

    // 停止定时器
    channel->timer = 0;

    // 重置步进状态
    channel->step = 0;
    channel->step_direction = 1;  // 确保步进方向为上升
}

void update_triangle(TRIANGLE_CHANNEL *triangle)
{
    // 1. 更新长度计数器
    if (!(triangle->control & 0x80) && triangle->length_counter > 0) {  // 检查控制标志的 halt 位是否为 0
        triangle->length_counter--;  // 递减长度计数器
    }

    // 2. 更新线性计数器
    if (triangle->reload) {  // 如果 reload 标志被设置，则重置线性计数器
        triangle->linear_counter = triangle->initial_linear_counter;  // 重置线性计数器
        triangle->reload = 0;  // 清除 reload 标志
    } else if (triangle->linear_counter > 0) {
        triangle->linear_counter--;  // 递减线性计数器
    }

    // 3. 检查线性计数器和长度计数器
    if (triangle->linear_counter == 0 || triangle->length_counter == 0) {
        triangle->output = 0.0f;  // 如果任何一个计数器为 0，三角波无输出
        return;
    }

    // 4. 更新定时器，生成三角波形
    if (triangle->timer > 0) {
        triangle->timer--;  // 定时器递减
    } else {
        // 当定时器到达 0 时，重置定时器并更新波形
        triangle->timer = triangle->timer_period;  // 重新加载定时器值

        // 5. 更新三角波步进
        if (triangle->step_direction) {
            if (triangle->step < 15) {
                triangle->step++;  // 逐步增加到 15
            } else {
                triangle->step_direction = 0;  // 达到 15 后改变方向
                triangle->step--;  // 开始减少
            }
        } else {
            if (triangle->step > 0) {
                triangle->step--;  // 逐步减少到 0
            } else {
                triangle->step_direction = 1;  // 达到 0 后改变方向
                triangle->step++;  // 开始增加
            }
        }
    }

    // 6. 生成三角波输出
    if (triangle->step < 8) {
        triangle->output = (float)triangle->step / 7.5f;  // 上升阶段，归一化到 [0, 1]
    } else {
        triangle->output = (float)(15 - triangle->step) / 7.5f;  // 下降阶段，归一化到 [0, 1]
    }

    triangle->output = triangle->output * 2.0f - 1.0f;  // 将输出缩放到 [-1.0, 1.0]
}

float calculate_triangle_waveform()
{
    return triangle1.output;
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

    // channel->shift_register = 1;  // 通常 NES 硬件不会立即重置移位寄存器
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

    // channel->shift_register = 1;  // NES 硬件不会立即清零移位寄存器
}

void update_noise(NOISE_CHANNEL *noise)
{
    // 更新长度计数器
    if (!noise->halt && noise->length_counter > 0) {
        noise->length_counter--;
    }

    // 更新频率计数器，控制噪声生成频率
    noise->timer--;
    if (noise->timer == 0) {
        noise->timer = noise->period;

        // 伪随机序列生成
        uint16_t feedback;
        if (noise->mode_flag) {
            feedback = (noise->shift_register >> 6) ^ (noise->shift_register >> 0);  // 模式1
        } else {
            feedback = (noise->shift_register >> 1) ^ (noise->shift_register >> 0);  // 模式0
        }

        noise->shift_register = (noise->shift_register >> 1) | (feedback << 14);
        noise->output = ~(noise->shift_register & 1) & 1;  // 根据 LFSR 生成噪音
    }

    // 更新包络，影响音量
    if (noise->envelope_start) {
        noise->envelope_counter = 15;
        noise->envelope_divider = noise->envelope_period;
        noise->envelope_start = FALSE;
    } else {
        if (noise->envelope_divider > 0) {
            noise->envelope_divider--;
        } else {
            noise->envelope_divider = noise->envelope_period;
            if (noise->envelope_counter > 0) {
                noise->envelope_counter--;
            } else if (noise->constant_volume == 0 && noise->loop_flag) {
                noise->envelope_counter = 15;  // 包络计数循环
            }
        }
    }

    // 最终的输出音量：根据包络或固定音量
    noise->output = noise->output ? (noise->constant_volume ? noise->volume : noise->envelope_counter) / 15.0f : 0.0f;
}

float calculate_noise_waveform()
{
    NOISE_CHANNEL *noise = get_noise();

    // 直接返回生成的 output
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
            dmc->current_address = bus_read(0xC000 + (dmc->address * 64)); // 计算DMC起始地址
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

float calculate_dmc_waveform()
{
    // 将音量转换为浮点数进行波形输出，范围 [0.0, 1.0]
    return dmc1.load / 128.0f;
}

void pulse_write(PULSE_CHANNEL *pulse, uint16_t reg, uint8_t data)
{
    switch (reg) {
        // 0x4000: 控制包络和占空比
        case 0x0000:
            pulse->duty = (data >> 6) & 0x03;  // 占空比设置 (2 位)
            pulse->loop_flag = (data >> 5) & 0x01;  // Length counter halt (或 envelope loop flag)
            pulse->constant_volume = (data >> 4) & 0x01;  // 固定音量或包络使能
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
            triangle->control = data & 0x80;  // 最高位表示是否控制长度计数器
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
            noise->period = data & 0x0F;  // 第 0-3 位：选择噪声的频率索引
            break;

        case 3:  // 地址 0x400F：控制长度计数器加载值和包络启动
            noise->length_counter = length_table[(data & 0xF8) >> 3];  // 第 3-7 位：长度计数器
            noise->envelope_start = TRUE;                               // 启动包络
            break;
    }
}

uint8_t is_triangle_active()
{
    // 如果控制位为 1，则忽略长度计数器，仅依赖线性计数器
    if (triangle1.control) {
        return triangle1.length_counter > 0;
    }

    // 控制位为 0 时，同时依赖长度计数器和线性计数器
    return (triangle1.length_counter > 0) && (triangle1.timer > 0);
}

uint16_t get_triangle_period()
{
    return triangle1.timer & 0x7FF;
}

void set_status(BYTE data)
{
    apu.status = data;
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

    // 位 7: DMC 中断标志
    if (dmc1.interrupt_flag) {
        status |= 0x80;
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

void write_4017(BYTE data)
{
    SDL_bool mode_5_step = (data & 0x80) != 0;  // 检查位 7，是否启用 5 步模式
    SDL_bool interrupt_inhibit = (data & 0x40) != 0;  // 检查位 6，是否禁用帧中断
    SDL_bool reset_frame_counter = (data & 0x01) != 0;  // 检查位 0，是否重置帧计数器

    if (mode_5_step) {
        // 设置为 5 步模式
        apu.mode = 5;
    } else {
        // 设置为 4 步模式
        apu.mode = 4;
    }

    if (interrupt_inhibit) {
        // 禁用帧中断，并清除现有的帧中断标志
        apu.frame_interrupt_enabled = FALSE;
        apu.frame_interrupt_flag = FALSE;
    } else {
        // 启用帧中断
        apu.frame_interrupt_enabled = TRUE;
    }
}

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

uint16_t get_dmc_period()
{
    return 0;
}

uint16_t read_dmc_sample()
{
    return 1;
}

uint16_t get_noise_period()
{
    return noise1.period;
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
            return 0; // 未知地址返回 0
    }
}

void step_apu()
{
    static double apu_clock_accumulator = 0.0;
    static const double apu_clock_per_sample = 1789773.0 / 44100.0;  // APU 时钟与采样率之比

    // 更新其他 APU 通道（矩形波、三角波、噪声等）
    update_pulses();
    update_triangle(&triangle1);
    update_noise(&noise1);

    // 更新 DMC 通道
    update_dmc(&dmc1);

    // 累积 APU 时钟周期
    apu_clock_accumulator += 1.0;

    // 检查是否需要生成多个音频样本（可能某些周期会累积较多时钟）
    while (apu_clock_accumulator >= apu_clock_per_sample) {
        apu_clock_accumulator -= apu_clock_per_sample;

        // 生成音频样本并推送到缓冲区
        queue_audio_sample();
    }
}