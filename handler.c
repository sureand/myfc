#include "common.h"
#include "handler.h"
#include "memory.h"
#include "disasm.h"

static inline BYTE test_bit(BYTE data, BYTE nr)
{
    return data & ( 1 << (nr & 7));
}

static inline int test_flag(BYTE flag)
{
    return !!test_bit(cpu.P, flag);
}

static void set_bit(volatile BYTE *addr, BYTE nr)
{
    (*addr) |= 1 << (nr & 7);
}

static void clear_bit(volatile BYTE *addr, BYTE nr)
{
    (*addr) &= ~(1 << (nr & 7));
}

static void set_flag(BYTE flag)
{
    set_bit(&cpu.P, flag);
}

static void clear_flag(BYTE flag)
{
    clear_bit(&cpu.P, flag);
}

void display_reg()
{
    int neg_flag = test_flag(NEG);
    int of_flag = test_flag(OF);
    int ex_flag = test_flag(EX);
    int brk_flag = test_flag(BRK);
    int dec_flag = test_flag(DEC);
    int inter_flag = test_flag(INT);
    int zero_flag = test_flag(ZERO);
    int carry_flag = test_flag(CARRY);
    printf("NEG:[%d], OF:[%d], EX:[%d], BRK:[%d], DEC:[%d], INT:[%d], ZERO:[%d], CARRY:[%d] \n",
        neg_flag, of_flag, ex_flag, brk_flag, dec_flag, inter_flag, zero_flag, carry_flag);
}

void display_stack()
{
    if(cpu.SP == 0xFF) return;

    BYTE spp = cpu.SP + 1;
    WORD addr = 0x0100;

    BYTE c = 1;
    printf("stack: [ ");
    while(spp) {
        if(c % 5 == 0) printf("\n");
        printf("%04X:%02X ", addr + spp, bus_read(addr + spp));
        ++spp;
        ++c;
    }
    printf("]\n");
}

static void set_nz(short value)
{
    //0 清空负数标记, 打上0 标记
    if(value == 0) {
        clear_flag(NEG);
        set_flag(ZERO);
        return;
    }

    //正数, 清空负数和 0 标记
    if(value > 0 && value <= 0x7F) {
        clear_flag(NEG);
        clear_flag(ZERO);
        return;
    }

    //负数 打上负数标记和清空 0 标记
    set_flag(NEG);
    clear_flag(ZERO);
}

void push(BYTE data)
{
    WORD addr = 0x0100 + cpu.SP;
    bus_write(addr, data);

    --cpu.SP;
}

static inline BYTE pop()
{
    ++cpu.SP;
    WORD addr = 0x0100 + cpu.SP;

    BYTE value = bus_read(addr);
    return value;
}

void jump(WORD address)
{
    BYTE addr1 = bus_read(address);
    BYTE addr2 = bus_read(address + 1);

    PC = addr2 << 8 | addr1;
}

static inline WORD IRQ_vector()
{
    return 0xFFFE;
}

void handler_ADC(WORD address)
{
    BYTE value = bus_read(address);

    WORD ret = cpu.A + value + (test_flag(CARRY) ? 1 : 0);

    BYTE result = ret & 0xFF;

    // 正确的溢出标志设置
    if (((cpu.A ^ result) & 0x80) && !((cpu.A ^ value) & 0x80)) {
        set_flag(OF);
    } else {
        clear_flag(OF);
    }

    cpu.A = result;
    set_nz(cpu.A);

    // 进位标志设置
    if (ret > 0xFF) {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }
}

void handler_SBC(WORD address)
{
    BYTE value = bus_read(address);

    WORD ret = cpu.A - value - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ value) & (cpu.A ^ ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    cpu.A = ret & 0xFF;
    set_nz(cpu.A);

    BYTE cf = ret >> 8;
    if(!cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void handler_ORA(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A = cpu.A | value;
    set_nz(cpu.A);

    if (cpu.A & 0x80) {
        cpu.P |= 0x80;
    } else {
        cpu.P &= ~0x80; // 清除负标志位
    }
}

void handler_SLO(WORD address)
{
    BYTE value = bus_read(address);

    BYTE cf = value & 0x80;
    if(cf) { set_flag(CARRY); }
    else { clear_flag(CARRY); }

    value <<= 1;

    bus_write(address, value);
    set_nz(value);

    cpu.A |= value;
    set_nz(cpu.A);
}

void handler_ASL(WORD address)
{
    BYTE value = bus_read(address);

    BYTE or = value & 0x80;
    value <<= 1;

    //置空第0位
    value &= 0xFE;

    //第七位进入进位
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    bus_write(address, value);

    set_nz(value);
}

void handler_AND(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A = cpu.A & value;
    set_nz(cpu.A);
}

void handler_ROL(WORD address)
{
    BYTE value = bus_read(address);

    BYTE or = value & 0x80;

    value <<= 1;

    if(test_flag(CARRY)) {
        value |= 0x01;
    } else {
        value |= 0x00;
    }

    if(or) {
        set_flag(CARRY);
    }else {
        clear_flag(CARRY);
    }

    bus_write(address, value);

    set_nz(value);
}

void handler_EOR(WORD address)
{
    BYTE value = bus_read(address);
    cpu.A = cpu.A ^ value;

    set_nz(cpu.A);
}

void handler_LSR(WORD address)
{
    BYTE value = bus_read(address);

    BYTE or = value & 0x01;
    value >>= 1;

    //置空第七位
    value &= 0x7F;
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    bus_write(address, value);

    set_nz(value);
}

void handler_ROR(WORD address)
{
    BYTE value = bus_read(address);

    BYTE or = value & 0x01;

    value >>= 1;
    if(test_flag(CARRY)) {
        value |= 0x80;
    }

    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    bus_write(address, value);

    set_nz(value);
}

void handler_LDX(WORD address)
{
    BYTE value = bus_read(address);

    cpu.X = value;
    set_nz(cpu.X);
}

void handler_LDY(WORD address)
{
    BYTE value = bus_read(address);

    cpu.Y = value;

    set_nz(value);
}

void handler_LDA(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A = value;
    set_nz(value);
}

void handler_CMP(WORD address)
{
    BYTE value = bus_read(address);

    if (cpu.A >= value) {
        cpu.P |= 0x01;  // 设置进位标志
    } else {
        cpu.P &= ~0x01; // 清除进位标志
    }

    WORD ret = cpu.A - value;

    if ((ret & 0xFF) == 0) {
        cpu.P |= 0x02;  // 设置零标志
    } else {
        cpu.P &= ~0x02; // 清除零标志
    }

    if (ret & 0x80) {
        cpu.P |= 0x80;  // 设置负标志
    } else {
        cpu.P &= ~0x80; // 清除负标志
    }
}

void handler_CPY(WORD address)
{
    BYTE value = bus_read(address);

    WORD ret = cpu.Y - value;

    if (cpu.Y >= value) {
        cpu.P |= 0x01;  // 设置进位标志
    } else {
        cpu.P &= ~0x01; // 清除进位标志
    }

    if ((ret & 0xFF) == 0) {
        cpu.P |= 0x02;  // 设置零标志
    } else {
        cpu.P &= ~0x02; // 清除零标志
    }

    if (ret & 0x80) {
        cpu.P |= 0x80;  // 设置负标志
    } else {
        cpu.P &= ~0x80; // 清除负标志
    }
}

void handler_DCP(WORD address)
{
    BYTE value = bus_read(address);

    char ret = value - 1;
    bus_write(address, ret);
    set_nz(ret);

    short sr = (WORD)cpu.A - ret;

    if((sr & 0xFF) == 0) { set_flag(ZERO); }
    else { clear_flag(ZERO); }

    if((sr >> 0x07) & 0x01) { set_flag(NEG); }
    else { clear_flag(NEG); }

    if(cpu.A >= (sr & 0xFF)) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void handler_CPX(WORD address)
{
    BYTE value = bus_read(address);

    WORD ret = cpu.X - value;

    if (cpu.X >= value) {
        cpu.P |= 0x01;  // 设置进位标志
    } else {
        cpu.P &= ~0x01; // 清除进位标志
    }

    if ((ret & 0xFF) == 0) {
        cpu.P |= 0x02;  // 设置零标志
    } else {
        cpu.P &= ~0x02; // 清除零标志
    }

    if (ret & 0x80) {
        cpu.P |= 0x80;  // 设置负标志
    } else {
        cpu.P &= ~0x80; // 清除负标志
    }
}

void handler_ISC(WORD address)
{
    BYTE value = bus_read(address);

    value += 1;
    bus_write(address, value);

    WORD ret = cpu.A - value - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ value) & (cpu.A ^ ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    cpu.A = ret & 0xFF;
    set_nz(cpu.A);

    BYTE cf = ret >> 8;
    if(!cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void handler_INC(WORD address)
{
    BYTE value = bus_read(address);

    char ret = value + 1;
    bus_write(address, ret);

    set_nz(ret);
}

void handler_DEC(WORD address)
{
    BYTE value = bus_read(address);

    char ret = value - 1;
    bus_write(address, ret);

    set_nz(ret);
}

void handler_LAX(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A = value;
    cpu.X = value;

    set_nz(value);
}

void handler_SRE(WORD address)
{
    BYTE value = bus_read(address);

    BYTE or = value & 0x01;

    value >>= 1;

    //置空第七位
    value &= 0x7F;
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    bus_write(address, value);
    set_nz(value);

    cpu.A ^= value;
    set_nz(cpu.A);
}

void handler_RRA(WORD address)
{
   handler_ROR(address);

   handler_ADC(address);
}

void handler_RLA(WORD address)
{
    handler_ROL(address);

    handler_AND(address);
}

void handler_ARR(WORD address)
{
    BYTE value = bus_read(address);
    cpu.A &= value;

    BYTE carry_in = (cpu.P & 0x01) ? 0x80 : 0x00;  // 进位标志位作为新最高位
    cpu.P = (cpu.P & ~0x01) | (cpu.A & 0x01);      // 将最低位移入进位标志位
    cpu.A = (cpu.A >> 1) | carry_in;               // 右移并结合进位标志位

    cpu.P = (cpu.P & ~0xC0) | (cpu.A & 0x80);      // 设置负标志位
    if (cpu.A == 0) {
        cpu.P |= 0x02;  // 设置零标志位
    } else {
        cpu.P &= ~0x02; // 清除零标志位
    }
    if (cpu.A & 0x40) {
        cpu.P |= 0x01;  // 设置进位标志位
    } else {
        cpu.P &= ~0x01; // 清除进位标志位
    }
    if ((cpu.A & 0x40) ^ ((cpu.A & 0x20) << 1)) {
        cpu.P |= 0x40;  // 设置溢出标志位
    } else {
        cpu.P &= ~0x40; // 清除溢出标志位
    }
}

void handler_AAC(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A &= value;

    if (cpu.A & 0x80) {
        cpu.P |= 0x80;  // 设置负标志
    } else {
        cpu.P &= ~0x80; // 清除负标志
    }

    if (cpu.A == 0) {
        cpu.P |= 0x02;  // 设置零标志
    } else {
        cpu.P &= ~0x02; // 清除零标志
    }

    if (cpu.A & 0x80) {
        cpu.P |= 0x01;  // 设置进位标志
    } else {
        cpu.P &= ~0x01; // 清除进位标志
    }
}

void handler_BIT(WORD address)
{
    BYTE value = bus_read(address);

    BYTE flag = test_bit(value, 6);
    if(flag) { set_bit(&cpu.P, 6); } else{
        clear_bit(&cpu.P, 6);
    }

    flag = test_bit(value, 7);
    if(flag) { set_bit(&cpu.P, 7); } else{
       clear_bit(&cpu.P, 7);
    }

    BYTE ret = cpu.A & value;
    if(ret == 0) {
        set_flag(ZERO);
        return;
    }

    clear_flag(ZERO);
}

void handler_JSR(WORD address)
{
    //返回地址压栈
    WORD return_address = PC + 2; // 当前 PC 加 2，因为我们希望返回时跳过 JSR 指令

    // 返回地址压栈
    push((return_address >> 8) & 0xFF); // 高字节
    push(return_address & 0xFF); // 低字节

    BYTE addr1 = bus_read(PC + 1);
    BYTE addr2 = bus_read(PC + 2);
    WORD addr = addr2 << 8 | addr1;

    PC = addr;
}

void handler_BRK(WORD address)
{
    //CPU 会把PC + 1 这条指令用作填充作用, 并且直接忽略
    PC++;
    push(PC >> 8);
    push(PC & 0xFF);

    //设置中断标志
    set_flag(BRK);
    push(cpu.P);
    set_flag(INT);

    //跳到0xFFFE 运行
    jump(IRQ_vector());
}

void handler_BPL(WORD address)
{
    if(test_flag(NEG)) return;

    //分支如果没有跨页, 则 + 1, 否则 + 2;
    ++cpu.cycle;
    if((address >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = address;
}

void handler_BEQ(WORD address)
{
    if(!test_flag(ZERO))  return;

    ++cpu.cycle;

    if((address >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = address;
}

void handler_BNE(WORD address)
{
    if(test_flag(ZERO)) return;

    ++cpu.cycle;
    if((address >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = address;
}

void handler_ASL_REG(WORD address)
{
    if(cpu.A & 0x80) {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    cpu.A <<= 1;

    set_nz(cpu.A);

    ++PC;
}

void handler_AXA(WORD address)
{
    cpu.X &= cpu.A;
    BYTE ret = cpu.X & 0x07;

    bus_write(address, ret);
}

void handler_CLC(WORD address)
{
    clear_flag(CARRY);

    ++PC;
}

void handler_KIL(WORD address)
{
    (void)address;
    cpu.is_lock = 1;
}

void handler_DOP(WORD address)
{
    (void)address;
}

void handler_NOP(WORD address)
{
    (void)address;
}

void handler_PHP(WORD address)
{
    //入栈的时候需要带上BRK 标记和 额外标记
    push(cpu.P | 0x30);

    ++PC;
}

void handler_PLP(WORD address)
{
    BYTE value = pop();

    //出栈后需要带上 brk 标记、忽略额外标记
    cpu.P = (value & 0xEF) | 0x20;

    ++PC;
}

void handler_ROL_REG_A(WORD address)
{
    BYTE or = cpu.A & 0x80;

    cpu.A <<= 1;

    if(test_flag(CARRY)) {
        cpu.A |= 0x01;
    }

    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    set_nz(cpu.A);

    ++PC;
}

void handler_RTI(WORD address)
{
    BYTE value = pop();

    cpu.P = value | 0x20;

    BYTE addr1 = pop();
    BYTE addr2 = pop();
    WORD addr = (addr2 << 8 | addr1);

    PC = addr;
}

void handler_BMI(WORD address)
{
    if(!test_flag(NEG)) return;

    ++cpu.cycle;
    if((address >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = address;
}

void handler_TOP(WORD address)
{
    (void)address;
}

void handler_LSR_REG_A(WORD address)
{
    if(cpu.A & 0x1 ) { set_flag(CARRY); }
    else {clear_flag(CARRY);}

    cpu.A >>= 1;
    set_nz(cpu.A);

    ++PC;
}

void handler_ASR(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A &= value;

    BYTE carry = cpu.A & 0x01;

    cpu.A >>= 1;

    if (carry) {
        cpu.P |= 0x01;
    } else {
        cpu.P &= ~0x01;
    }

    if (cpu.A == 0) {
        cpu.P |= 0x02;
    } else {
        cpu.P &= ~0x02;
    }

    cpu.P &= ~0x80;

    PC += 1;
}

void handler_BVC(WORD address)
{
    if(test_flag(OF)) return;

    ++cpu.cycle;
    if((address >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = address;
}

void handler_RTS(WORD address)
{
    BYTE addr1 = pop();
    BYTE addr2 = pop();

    WORD addr = addr2 << 8 | addr1;
    PC = addr + 1;
}

void handler_PLA(WORD address)
{
    cpu.A = pop();
    set_nz(cpu.A);

    ++PC;
}

void handler_ROR_REG_A(WORD address)
{
    BYTE or = cpu.A & 0x01;
    cpu.A >>= 1;

    if(test_flag(CARRY)) {
        cpu.A |= 0x80;
    }

    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    set_nz(cpu.A);

    ++PC;
}

void handler_BVS(WORD address)
{
    if(!test_flag(OF)) return;

    ++cpu.cycle;
    if((address >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = address;
}

void handler_SEI(WORD address)
{
    set_flag(INT);

    ++PC;
}

void handler_STA(WORD address)
{
    bus_write(address, cpu.A);
}

void handler_AAX(WORD address)
{
    BYTE value = cpu.X & cpu.A;

    bus_write(address, value);
}

void handler_STX(WORD address)
{
    bus_write(address, cpu.X);
}

void handler_TYA(WORD address)
{
    cpu.A = cpu.Y;
    set_nz(cpu.A);

    ++PC;
}

void handler_TXS(WORD address)
{
    cpu.SP = cpu.X;

    ++PC;
}

void handler_XAS(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A = cpu.A & value;
    set_nz(cpu.A);

    cpu.A = cpu.A & value;
    set_nz(cpu.A);
}

void handler_SYA(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A = cpu.A & value;
    set_nz(cpu.A);

    cpu.A = cpu.A & value;
    set_nz(cpu.A);
}

void handler_SXA(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A = cpu.A & value;
    set_nz(cpu.A);

    cpu.A = cpu.A & value;
    set_nz(cpu.A);
}

void handler_TAY(WORD address)
{
    cpu.Y = cpu.A;
    set_nz(cpu.Y);

    ++PC;
}

void handler_TAX(WORD address)
{
    cpu.X = cpu.A;
    set_nz(cpu.X);

    ++PC;
}

void handler_ATX(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A = value;
    cpu.X = cpu.A;

    if (cpu.A == 0) {
        cpu.P |= 0x02;
    } else {
        cpu.P &= ~0x02;
    }

    // 更新负标志位 (N)
    if (cpu.A & 0x80) {
        cpu.P |= 0x80;
    } else {
        cpu.P &= ~0x80;
    }

    PC += 1;
}

void handler_BCS(WORD address)
{
    if(!test_flag(CARRY)) return;

    //分支如果没有跨页, 则 + 1, 否则 + 2;
    ++cpu.cycle;
    if((address >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = address;
}

void handler_CLV(WORD address)
{
    clear_flag(OF);
    ++PC;
}

void handler_TSX(WORD address)
{
    cpu.X = cpu.SP;
    set_nz(cpu.X);

    ++PC;
}

void handler_LAR(WORD address)
{
    cpu.A = bus_read(address);
    set_nz(cpu.A);

    cpu.X = cpu.SP;
    set_nz(cpu.X);
    ++PC;
}

void handler_INY(WORD address)
{
    cpu.Y += 1;
    set_nz(cpu.Y);

    ++PC;
}

void handler_DEX(WORD address)
{
    char ret = cpu.X - 1;

    cpu.X = ret;
    set_nz(ret);

    ++PC;
}

void handler_AXS(WORD address)
{
    BYTE value = bus_read(address);

    WORD result = (cpu.A & cpu.X) - value;

    cpu.X = result & 0xFF;

    if (result <= 0xFF) {
        cpu.P |= 0x01;
    } else {
        cpu.P &= ~0x01;
    }

    if (cpu.X == 0) {
        cpu.P |= 0x02;
    } else {
        cpu.P &= ~0x02;
    }

    // 更新状态寄存器的负标志位 (N)
    if (cpu.X & 0x80) {
        cpu.P |= 0x80;
    } else {
        cpu.P &= ~0x80;
    }

    PC += 1;
}

void handler_CLD(WORD address)
{
    ++PC;
    clear_flag(DEC);
}

void handler_INX(WORD address)
{
    cpu.X += 1;
    set_nz(cpu.X);

    ++PC;
}

void handler_SED(WORD address)
{
    set_flag(DEC);

    ++PC;
}

void handler_XAA(WORD address)
{
    BYTE value = bus_read(address);

    cpu.A = cpu.A & value;
    set_nz(cpu.A);

    cpu.A = cpu.A & value;
    set_nz(cpu.A);
}

void handler_STY(WORD address)
{
    bus_write(address, cpu.Y);
}

void handler_DEY(WORD address)
{
    char ret = cpu.Y - 1;

    cpu.Y = ret;
    set_nz(ret);

    ++PC;
}

void handler_TXA(WORD address)
{
    cpu.A = cpu.X;
    set_nz(cpu.A);

    ++PC;
}

void handler_BCC(WORD address)
{
    if(test_flag(CARRY)) return;

    ++cpu.cycle;
    if((address >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = address;
}

void handler_SEC(WORD address)
{
    set_flag(CARRY);
    ++PC;
}

void handler_PHA(WORD address)
{
    push(cpu.A);

    ++PC;
}

void handler_CLI(WORD address)
{
    clear_flag(INT);
    ++PC;
}