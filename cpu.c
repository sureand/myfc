#include "cpu.h"
#include "load_rom.h"
#include "memory.h"
#include "disasm.h"

#define op(c, s, n, p, func) \
    code_maps[0x##c].op_name = s; \
    code_maps[0x##c].op_len = n; \
    code_maps[0x##c].cycle = p; \
    code_maps[0x##c].op_func = func; \

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
        printf("%04X:%02X ", addr + spp, read_byte(addr + spp));
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

static void set_overflow(short value)
{
    if(value > 127 || value < -128) {
        set_flag(OF);
        return;
    }

    clear_flag(OF);
}

static void push(BYTE data)
{
    WORD addr = 0x0100 + cpu.SP;
    write_byte(addr, data);

    --cpu.SP;
}

static inline BYTE pop()
{
    ++cpu.SP;
    WORD addr = 0x0100 + cpu.SP;

    BYTE bt = read_byte(addr);
    return bt;
}

/**
 * 指令设计开始
 * IP 永远指向函数的开始地方
 * 因此需要识别指令长度, 调整 IP 的值
 */

void jump(WORD address)
{
    BYTE addr1 = read_byte(address);
    BYTE addr2 = read_byte(address + 1);

    PC = addr2 << 8 | addr1;
}

static inline WORD IRQ_vector()
{
    return 0xFFFE;
}

//立即寻址
static inline WORD immediate_addressing()
{
    WORD addr = PC + 1;
    PC += 2;

    return addr;
}

//绝对寻址
static inline WORD absolute_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
    WORD addr = (addr2 << 8 | addr1);
    PC += 3;

    return addr;
}

//绝对零页寻址
static inline WORD zero_absolute_addressing()
{
    BYTE addr = read_byte(PC + 1);
    PC += 2;

    return (addr & 0xFF);
}

//绝对 X 变址
static inline WORD absolute_X_indexed_addressing(BYTE op)
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
    PC += 3;

    WORD addr = (addr2 << 8) | addr1;

    if( code_maps[op].cycle < 5 && (addr >> 8) != ((addr + cpu.X) >> 8) )
        ++cpu.cycle;

    return (addr + cpu.X);
}

//绝对 Y 变址
static inline WORD absolute_Y_indexed_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
    PC += 3;

    WORD addr = (addr2 << 8) | addr1;

    if((addr >> 8) != ((addr + cpu.Y) >> 8)) ++cpu.cycle;

    return (addr + cpu.Y);
}

//零页 X 间接寻址
static inline WORD zero_X_indexed_addressing()
{
    BYTE addr = read_byte(PC + 1);
    addr += cpu.X;
    PC += 2;

    return (addr & 0xFF);
}

//零页 Y 间接 寻址
static inline WORD zero_Y_indexed_addressing()
{
    BYTE addr = read_byte(PC + 1);
    addr += cpu.Y;
    PC += 2;

    return (addr & 0xFF);
}

static inline WORD indirect_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
    PC += 3;

    WORD addr = (addr2 << 8) | addr1;

    //这个是6502CPU的Bug
    if((addr & 0xFF) == 0xFF) {
        addr = (read_byte(addr & 0xFF00) << 8) + read_byte(addr);
        return addr;
    }

    BYTE pc1 = read_byte(addr);
    BYTE pc2 = read_byte(addr + 1);

    return (pc2 << 8 )| pc1;
}

// x 变址间接 寻址
static inline WORD indexed_X_indirect_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    addr1 += cpu.X;

    BYTE addr2 = addr1 + 1;
    WORD addr = (read_byte(addr2) << 8) | read_byte(addr1);

    PC += 2;

    return addr;
}

//Y 间接 变址寻址
static inline WORD indirect_Y_indexed_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = addr1 + 1;
    WORD addr = (read_byte(addr2) << 8) | read_byte(addr1);
    PC += 2;

    if( (addr >> 8) != (addr + cpu.Y) >> 8) ++cpu.cycle;

    return addr + cpu.Y;
}

static inline WORD relative_addressing()
{
    //先算下一条指令的地址, 再算偏移
    char of = (char)read_byte(PC + 1);
    PC += 2;

    WORD addr = PC + of;

    return addr;
}

//BRK 中断
void BRK_00(BYTE op)
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

void ORA_01(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A | bt;

    set_nz(cpu.A);
}

void ORA_05(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A | bt;

    set_nz(cpu.A);
}

void ASL_06(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    BYTE or = bt & 0x80;
    bt <<= 1;

    //置空第0位
    bt &= 0xFE;

    //第七位进入进位
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void PHP_08(BYTE op)
{
    //入栈的时候需要带上BRK 标记和 额外标记
    push(cpu.P | 0x30);

    ++PC;
}

void ORA_09(BYTE op)
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A | bt;

    set_nz(cpu.A);
}

void ASL_0A(BYTE op)
{
    if(cpu.A & 0x80) { set_flag(CARRY); } else {clear_flag(CARRY);}

    cpu.A <<= 1;

    set_nz(cpu.A);

    ++PC;
}

void ORA_0D(BYTE op)
{
    WORD addr = absolute_addressing();
    char bt = read_byte(addr);
    cpu.A = cpu.A | bt;

    set_nz(cpu.A);
}

void ASL_0E(BYTE op)
{
    WORD addr = absolute_addressing();
    char bt = read_byte(addr);

    BYTE or = bt & 0x80;
    bt <<= 1;

    //置空第0位
    bt &= 0xFE;

    //第七位进入进位
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void BPL_10(BYTE op)
{
    WORD addr = relative_addressing();
    if(test_flag(NEG)) return;

    //分支如果没有跨页, 则 + 1, 否则 + 2;
    ++cpu.cycle;
    if((addr >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = addr;
}

void ORA_11(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A | bt;

    set_nz(cpu.A);
}

void ORA_15(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A | bt;

    set_nz(cpu.A);
}

void ASL_16(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    BYTE or = bt & 0x80;
    bt <<= 1;

    //置空第0位
    bt &= 0xFE;

    //第七位进入进位
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void CLC_18(BYTE op)
{
    clear_flag(CARRY);
    ++PC;
}

void ORA_19(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A | bt;

    set_nz(cpu.A);
}

void ORA_1D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A | bt;

    set_nz(cpu.A);
}

void ASL_1E(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);

    BYTE or = bt & 0x80;
    bt <<= 1;

    //置空第0位
    bt &= 0xFE;

    //第七位进入进位
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void JSR_20(BYTE op)
{
    WORD cur_pc = PC;
    PC += 2;

    //返回地址压栈
    BYTE *bp = (BYTE*)&PC;
    push(*(bp + 1));
    push(*(bp));

    BYTE addr1 = read_byte(cur_pc + 1);
    BYTE addr2 = read_byte(cur_pc + 2);
    WORD addr = addr2 << 8 | addr1;

    PC = addr;
}

void AND_21(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A & bt;

    set_nz(cpu.A);
}

void BIT_24(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    BYTE flag = test_bit(bt, 6);
    if(flag) { set_bit(&cpu.P, 6); } else{
        clear_bit(&cpu.P, 6);
    }

    flag = test_bit(bt, 7);
    if(flag) { set_bit(&cpu.P, 7); } else{
       clear_bit(&cpu.P, 7);
    }

    BYTE ret = cpu.A & bt;
    if(ret == 0) {
        set_flag(ZERO);
        return;
    }

    clear_flag(ZERO);
}

void AND_25(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A & bt;

    set_nz(cpu.A);
}

void ROL_26(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    BYTE or = bt & 0x80;

    bt <<= 1;

    if(test_flag(CARRY)) {
        bt |= 0x01;
    } else {
        bt |= 0x00;
    }

    if(or) {
        set_flag(CARRY);
    }else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void PLP_28(BYTE op)
{
    BYTE bt = pop();

    //出栈后需要带上 brk 标记、忽略额外标记
    cpu.P = (bt & 0xEF) | 0x20;

    ++PC;
}

void AND_29(BYTE op)
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A & bt;

    set_nz(cpu.A);
}

void ROL_2A(BYTE op)
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

void BIT_2C(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    BYTE flag = test_bit(bt, 6);
    if(flag) { set_bit(&cpu.P, 6); } else{
        clear_bit(&cpu.P, 6);
    }

    flag = test_bit(bt, 7);
    if(flag) { set_bit(&cpu.P, 7); } else{
       clear_bit(&cpu.P, 7);
    }

    BYTE ret = cpu.A & bt;
    if(ret == 0) {
        set_flag(ZERO);
        return;
    }

    clear_flag(ZERO);
}

void AND_2D(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A & bt;

    set_nz(cpu.A);
}

void ROL_2E(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    BYTE or = bt & 0x80;

    bt <<= 1;

    if(test_flag(CARRY)) {
        bt |= 0x01;
    } else {
        bt |= 0x00;
    }

    if(or) {
        set_flag(CARRY);
    }else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void BMI_30(BYTE op)
{
    WORD addr = relative_addressing();
    if(!test_flag(NEG)) return;

    ++cpu.cycle;
    if((addr >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = addr;
}

void AND_31(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A & bt;

    set_nz(cpu.A);
}

void AND_35(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A & bt;

    set_nz(cpu.A);
}

void ROL_36(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    BYTE or = bt & 0x80;

    bt <<= 1;

    if(test_flag(CARRY)) {
        bt |= 0x01;
    } else {
        bt |= 0x00;
    }

    if(or) {
        set_flag(CARRY);
    }else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void SEC_38(BYTE op)
{
    set_flag(CARRY);
    ++PC;
}

void AND_39(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A & bt;

    set_nz(cpu.A);
}

void AND_3D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A & bt;

    set_nz(cpu.A);
}

void ROL_3E(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);

    BYTE or = bt & 0x80;

    bt <<= 1;

    if(test_flag(CARRY)) {
        bt |= 0x01;
    } else {
        bt |= 0x00;
    }

    if(or) {
        set_flag(CARRY);
    }else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void RTI_40(BYTE op)
{
    BYTE bt = pop();

    cpu.P = bt | 0x20;

    BYTE addr1 = pop();
    BYTE addr2 = pop();
    WORD addr = (addr2 << 8 | addr1);

    PC = addr;
}

void EOR_41(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;

    set_nz(cpu.A);
}

void EOR_45(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;

    set_nz(cpu.A);
}

void LSR_46(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    BYTE or = bt & 0x01;
    bt >>= 1;

    //置空第七位
    bt &= 0x7F;
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void PHA_48(BYTE op)
{
    push(cpu.A);

    ++PC;
}

void EOR_49(BYTE op)
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;

    set_nz(cpu.A);
}

void LSR_4A(BYTE op)
{
    if(cpu.A & 0x1 ) { set_flag(CARRY); }
    else {clear_flag(CARRY);}

    cpu.A >>= 1;
    set_nz(cpu.A);

    ++PC;
}

void JMP_4C(BYTE op)
{
    WORD addr = absolute_addressing();

    PC = addr;
}

void EOR_4D(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;

    set_nz(cpu.A);
}

void LSR_4E(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    BYTE or = bt & 0x01;

    bt >>= 1;

    //置空第七位
    bt &= 0x7F;
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void BVC_50(BYTE op)
{
    WORD addr = relative_addressing();
    if(test_flag(OF)) return;

    ++cpu.cycle;
    if((addr >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = addr;
}

void EOR_51(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;

    set_nz(cpu.A);
}

void EOR_55(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;

    set_nz(cpu.A);
}

void LSR_56(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    BYTE or = bt & 0x01;
    bt >>= 1;

    //置空第七位
    bt &= 0x7F;
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void CLI_58(BYTE op)
{
    clear_flag(INT);
    ++PC;
}

void EOR_59(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;

    set_nz(cpu.A);
}

void EOR_5D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;

    set_nz(cpu.A);
}

void LSR_5E(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);
    BYTE or = bt & 0x01;
    bt >>= 1;

    //置空第七位
    bt &= 0x7F;
    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void RTS_60(BYTE op)
{
    BYTE addr1 = pop();
    BYTE addr2 = pop();

    WORD addr = addr2 << 8 | addr1;
    PC = addr + 1;
}

void ADC_61(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();
    BYTE bt = read_byte(addr);

    WORD ret = cpu.A + bt + (test_flag(CARRY) ? 1 : 0);

    cpu.A = ret & 0xFF;

    set_nz(cpu.A);

    BYTE of = (bt ^ ret) & (cpu.A & ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    BYTE cf = ret >> 8;
    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void ADC_65(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    //三目运算要加括号，不然会踩坑
    WORD ret = cpu.A + bt + (test_flag(CARRY) ? 1 : 0);

    cpu.A = ret & 0xFF;

    set_nz(cpu.A);

    BYTE of = (bt ^ ret) & (cpu.A & ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    BYTE cf = ret >> 8;
    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void ROR_66(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    BYTE or = bt & 0x01;

    bt >>= 1;
    if(test_flag(CARRY)) {
        bt |= 0x80;
    }

    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void PLA_68(BYTE op)
{
    cpu.A = pop();
    set_nz(cpu.A);

    ++PC;
}

void ADC_69(BYTE op)
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);

    //三目运算要加括号，不然会踩坑
    WORD ret = cpu.A + bt + (test_flag(CARRY) ? 1 : 0);

    cpu.A = ret & 0xFF;

    set_nz(cpu.A);

    BYTE of = (bt ^ ret) & (cpu.A & ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    BYTE cf = ret >> 8;
    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void ROR_6A(BYTE op)
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

void JMP_6C(BYTE op)
{
    WORD addr = indirect_addressing();

    PC = addr;
}

void ADC_6D(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    //三目运算要加括号，不然会踩坑
    WORD ret = cpu.A + bt + (test_flag(CARRY) ? 1 : 0);

    cpu.A = ret & 0xFF;

    set_nz(cpu.A);

    BYTE of = (bt ^ ret) & (cpu.A & ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    BYTE cf = ret >> 8;
    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void ROR_6E(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    BYTE or = bt & 0x01;
    bt >>= 1;

    if(test_flag(CARRY)) {
        bt |= 0x80;
    }

    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void BVS_70(BYTE op)
{
    WORD addr = relative_addressing();
    if(!test_flag(OF)) return;

    ++cpu.cycle;
    if((addr >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = addr;
}

void ADC_71(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    //三目运算要加括号，不然会踩坑
    WORD ret = cpu.A + bt + (test_flag(CARRY) ? 1 : 0);

    cpu.A = ret & 0xFF;

    set_nz(cpu.A);

    BYTE of = (bt ^ ret) & (cpu.A & ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    BYTE cf = ret >> 8;
    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void ADC_75(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    //三目运算要加括号，不然会踩坑
    WORD ret = cpu.A + bt + (test_flag(CARRY) ? 1 : 0);

    cpu.A = ret & 0xFF;

    set_nz(cpu.A);

    BYTE of = (bt ^ ret) & (cpu.A & ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    BYTE cf = ret >> 8;
    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void ROR_76(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    BYTE or = bt & 0x01;

    bt >>= 1;
    if(test_flag(CARRY)) {
        bt |= 0x80;
    }

    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void SEI_78(BYTE op)
{
    set_flag(INT);
    ++PC;
}

void ADC_79(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    //三目运算要加括号，不然会踩坑
    WORD ret = cpu.A + bt + (test_flag(CARRY) ? 1 : 0);

    cpu.A = ret & 0xFF;

    set_nz(cpu.A);

    BYTE of = (bt ^ ret) & (cpu.A & ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    BYTE cf = ret >> 8;
    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void ADC_7D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);

    //三目运算要加括号，不然会踩坑
    WORD ret = cpu.A + bt + (test_flag(CARRY) ? 1 : 0);

    cpu.A = ret & 0xFF;

    set_nz(cpu.A);

    BYTE of = (bt ^ ret) & (cpu.A & ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    BYTE cf = ret >> 8;
    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void ROR_7E(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);
    BYTE or = bt & 0x01;

    bt >>= 1;
    if(test_flag(CARRY)) {
        bt |= 0x80;
    }

    if(or)  {
        set_flag(CARRY);
    } else {
        clear_flag(CARRY);
    }

    write_byte(addr, bt);

    set_nz(bt);
}

void STA_81(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();
    write_byte(addr, cpu.A);
}

void STY_84(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    write_byte(addr, cpu.Y);
}

void STA_85(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    write_byte(addr, cpu.A);
}

void STX_86(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    write_byte(addr, cpu.X);
}

void DEY_88(BYTE op)
{
    char ret = cpu.Y - 1;
    cpu.Y = ret;
    set_nz(ret);

    ++PC;
}

void TXA_8A(BYTE op)
{
    cpu.A = cpu.X;
    set_nz(cpu.A);

    ++PC;
}

void STY_8C(BYTE op)
{
    WORD addr = absolute_addressing();
    write_byte(addr, cpu.Y);
}

void STA_8D(BYTE op)
{
    WORD addr = absolute_addressing();
    write_byte(addr, cpu.A);
}

void STX_8E(BYTE op)
{
    WORD addr = absolute_addressing();
    write_byte(addr, cpu.X);
}

void BCC_90(BYTE op)
{
    WORD addr = relative_addressing();
    if(test_flag(CARRY)) return;

    ++cpu.cycle;
    if((addr >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = addr;
}

void STA_91(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing();
    write_byte(addr, cpu.A);
}

void STY_94(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    write_byte(addr, cpu.Y);
}

void STA_95(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    write_byte(addr, cpu.A);
}

void STX_96(BYTE op)
{
    WORD addr = zero_Y_indexed_addressing();
    write_byte(addr, cpu.X);
}

void TYA_98(BYTE op)
{
    cpu.A = cpu.Y;
    set_nz(cpu.A);

    ++PC;
}

void STA_99(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing();
    write_byte(addr, cpu.A);
}

void TXS_9A(BYTE op)
{
    cpu.SP = cpu.X;

    ++PC;
}

void STA_9D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    write_byte(addr, cpu.A);
}

void LDY_A0(BYTE op)
{
    WORD addr = immediate_addressing();
    cpu.Y = read_byte(addr);

    set_nz(cpu.Y);
}

void LDA_A1(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();
    cpu.A = read_byte(addr);

    set_nz(cpu.A);
}

void LDX_A2(BYTE op)
{
    WORD addr = immediate_addressing();
    cpu.X = read_byte(addr);

    set_nz(cpu.X);
}

void LDY_A4(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    cpu.Y = bt;

    set_nz(bt);
}

void LDA_A5(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = bt;

    set_nz(bt);
}

void LDX_A6(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    cpu.X = read_byte(addr);

    set_nz(cpu.X);
}

void TAY_A8(BYTE op)
{
    cpu.Y = cpu.A;
    set_nz(cpu.Y);

    ++PC;
}

void LDA_A9(BYTE op)
{
    WORD addr = immediate_addressing();
    cpu.A = read_byte(addr);

    set_nz(cpu.A);
}

void TAX_AA(BYTE op)
{
    cpu.X = cpu.A;
    set_nz(cpu.X);

    ++PC;
}

void LDY_AC(BYTE op)
{
    WORD addr = absolute_addressing();
    cpu.Y = read_byte(addr);

    set_nz(cpu.Y);
}

void LDA_AD(BYTE op)
{
    WORD addr = absolute_addressing();
    cpu.A = read_byte(addr);

    set_nz(cpu.A);
}

void LDX_AE(BYTE op)
{
    WORD addr = absolute_addressing();
    cpu.X = read_byte(addr);

    set_nz(cpu.X);
}

void BCS_B0(BYTE op)
{
    WORD addr = relative_addressing();

    if(!test_flag(CARRY)) return;

    //分支如果没有跨页, 则 + 1, 否则 + 2;
    ++cpu.cycle;
    if((addr >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = addr;
}

void LDA_B1(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing();
    cpu.A = read_byte(addr);

    set_nz(cpu.A);
}

void LDY_B4(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    cpu.Y = read_byte(addr);

    set_nz(cpu.Y);
}

void LDA_B5(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    cpu.A = read_byte(addr);

    set_nz(cpu.A);
}

void LDX_B6(BYTE op)
{
    WORD addr = zero_Y_indexed_addressing();
    cpu.X = read_byte(addr);

    set_nz(cpu.X);
}

void CLV_B8(BYTE op)
{
    clear_flag(OF);
    ++PC;
}

void LDA_B9(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing();
    cpu.A = read_byte(addr);

    set_nz(cpu.A);
}

void TSX_BA(BYTE op)
{
    cpu.X = cpu.SP;
    set_nz(cpu.X);

    ++PC;
}

void LDY_BC(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    cpu.Y = read_byte(addr);

    set_nz(cpu.Y);
}

void LDA_BD(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    cpu.A = read_byte(addr);

    set_nz(cpu.A);
}

void LDX_BE(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing();
    cpu.X  = read_byte(addr);

    set_nz(cpu.X);
}

void CPY_C0(BYTE op)
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.Y - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void CMP_C1(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.A - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void CPY_C4(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.Y - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void CMP_C5(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.A - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void DEC_C6(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = bt - 1;
    write_byte(addr, ret);

    set_nz(ret);
}

void INY_C8(BYTE op)
{
    cpu.Y += 1;
    set_nz(cpu.Y);

    ++PC;
}

void CMP_C9(BYTE op)
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.A - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void DEX_CA(BYTE op)
{
    char ret = cpu.X - 1;
    cpu.X = ret;
    set_nz(ret);

    ++PC;
}

void CPY_CC(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.Y - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void CMP_CD(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.A - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void DEC_CE(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = bt - 1;
    write_byte(addr, ret);

    set_nz(ret);
}

void BNE_D0(BYTE op)
{
    WORD addr = relative_addressing();
    if(test_flag(ZERO)) return;

    ++cpu.cycle;
    if((addr >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = addr;
}

void CMP_D1(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.A - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void CMP_D5(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.A - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void DEC_D6(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    char ret = bt - 1;
    write_byte(addr, ret);

    set_nz(ret);
}

void CLD_D8(BYTE op)
{
    ++PC;
    clear_flag(DEC);
}

void CMP_D9(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.A - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void CMP_DD(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);
    short ret = cpu.A - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void DEC_DE(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    char bt = read_byte(addr);
    bt -= 1;

    write_byte(addr, bt);

    set_nz(bt);
}

void CPX_E0(BYTE op)
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.X - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void SBC_E1(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();
    BYTE bt = read_byte(addr);

    WORD ret = cpu.A - bt - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ bt) & (cpu.A ^ ret) & 0x80;
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

void CPX_E4(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.X - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void SBC_E5(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    WORD ret = cpu.A - bt - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ bt) & (cpu.A ^ ret) & 0x80;
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

void INC_E6(BYTE op)
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = bt + 1;
    write_byte(addr, ret);

    set_nz(ret);
}

void INX_E8(BYTE op)
{
    cpu.X += 1;
    set_nz(cpu.X);

    ++PC;
}

void SBC_E9(BYTE op)
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    WORD ret = cpu.A - bt - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ bt) & (cpu.A ^ ret) & 0x80;
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

void NOP_EA(BYTE op)
{
    ++PC;
    //do nothing
}

void CPX_EC(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.X - bt;

    set_nz(ret);

    char cf = (ret >= 0) ? 1 : 0;

    if(cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
}

void SBC_ED(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    WORD ret = cpu.A - bt - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ bt) & (cpu.A ^ ret) & 0x80;
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

void INC_EE(BYTE op)
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = bt + 1;
    write_byte(addr, ret);

    set_nz(ret);
}

void BEQ_F0(BYTE op)
{
    WORD addr = relative_addressing();
    if(!test_flag(ZERO))  return;

    ++cpu.cycle;

    if((addr >> 8) != (PC >> 8)) ++cpu.cycle;

    PC = addr;
}

void SBC_F1(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    WORD ret = cpu.A - bt - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ bt) & (cpu.A ^ ret) & 0x80;
    if(of) { set_flag(OF); } else { clear_flag(OF); }

    cpu.A = ret & 0xFF;
    set_nz(cpu.A);

    BYTE cf = ret >> 8;
    if(!cf) {
        set_flag(CARRY);
        return;
    }

    clear_flag(CARRY);
    set_overflow(ret);
}

void SBC_F5(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    WORD ret = cpu.A - bt - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ bt) & (cpu.A ^ ret) & 0x80;
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

void INC_F6(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    char ret = bt + 1;
    write_byte(addr, ret);

    set_nz(ret);
}

void SED_F8(BYTE op)
{
    set_flag(DEC);

    ++PC;
}

void SBC_F9(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    WORD ret = cpu.A - bt - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ bt) & (cpu.A ^ ret) & 0x80;
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

void SBC_FD(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);
    WORD ret = cpu.A - bt - (test_flag(CARRY) ? 0 : 1);

    BYTE of = (cpu.A ^ bt) & (cpu.A ^ ret) & 0x80;
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

void INC_FE(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);
    BYTE bt = read_byte(addr);
    char ret = bt + 1;
    write_byte(addr, ret);

    set_nz(ret);
}

void init_code()
{
    op(00, "BRK", 1, 7, BRK_00)
    op(01, "ORA", 2, 6, ORA_01)
    op(05, "ORA", 2, 3, ORA_05)
    op(06, "ASL", 2, 5, ASL_06)
    op(08, "PHP", 1, 3, PHP_08)
    op(09, "ORA", 2, 2, ORA_09)
    op(0A, "ASL", 1, 2, ASL_0A)
    op(0D, "ORA", 3, 4, ORA_0D)
    op(0E, "ASL", 3, 6, ASL_0E)

    op(10, "BPL", 2, 2, BPL_10)
    op(11, "ORA", 2, 5, ORA_11)
    op(15, "ORA", 2, 4, ORA_15)
    op(16, "ASL", 2, 6, ASL_16)
    op(18, "CLC", 1, 2, CLC_18)
    op(19, "ORA", 3, 4, ORA_19)
    op(1D, "ORA", 3, 4, ORA_1D)
    op(1E, "ASL", 3, 7, ASL_1E)

    op(20, "JSR", 3, 6, JSR_20)
    op(21, "AND", 2, 6, AND_21)
    op(24, "BIT", 2, 3, BIT_24)
    op(25, "AND", 2, 3, AND_25)
    op(26, "ROL", 2, 5, ROL_26)
    op(28, "PLP", 1, 4, PLP_28)
    op(29, "AND", 2, 2, AND_29)
    op(2A, "ROL", 1, 2, ROL_2A)
    op(2C, "BIT", 3, 4, BIT_2C)
    op(2D, "AND", 3, 4, AND_2D)
    op(2E, "ROL", 3, 6, ROL_2E)

    op(30, "BMI", 2, 2, BMI_30)
    op(31, "AND", 2, 5, AND_31)
    op(35, "AND", 2, 4, AND_35)
    op(36, "ROL", 2, 6, ROL_36)
    op(38, "SEC", 1, 2, SEC_38)
    op(39, "AND", 3, 4, AND_39)
    op(3D, "AND", 3, 4, AND_3D)
    op(3E, "ROL", 3, 7, ROL_3E)

    op(40, "RTI", 1, 6, RTI_40)
    op(41, "EOR", 2, 6, EOR_41)
    op(45, "EOR", 2, 3, EOR_45)
    op(46, "LSR", 2, 5, LSR_46)
    op(48, "PHA", 1, 3, PHA_48)
    op(49, "EOR", 2, 2, EOR_49)
    op(4A, "LSR", 1, 2, LSR_4A)
    op(4C, "JMP", 3, 3, JMP_4C)
    op(4D, "EOR", 3, 4, EOR_4D)
    op(4E, "LSR", 3, 6, LSR_4E)

    op(50, "BVC", 2, 2, BVC_50)
    op(51, "EOR", 2, 5, EOR_51)
    op(55, "EOR", 2, 4, EOR_55)
    op(56, "LSR", 2, 6, LSR_56)
    op(58, "CLI", 1, 2, CLI_58)
    op(59, "EOR", 3, 4, EOR_59)
    op(5D, "EOR", 3, 4, EOR_5D)
    op(5E, "LSR", 3, 7, LSR_5E)

    op(60, "RTS", 1, 6, RTS_60)
    op(61, "ADC", 2, 6, ADC_61)
    op(65, "ADC", 2, 3, ADC_65)
    op(66, "ROR", 2, 5, ROR_66)
    op(68, "PLA", 1, 4, PLA_68)
    op(69, "ADC", 2, 2, ADC_69)
    op(6A, "ROR", 1, 2, ROR_6A)
    op(6C, "JMP", 3, 5, JMP_6C)
    op(6D, "ADC", 3, 4, ADC_6D)
    op(6E, "ROR", 3, 6, ROR_6E)

    op(70, "BVS", 2, 2, BVS_70)
    op(71, "ADC", 2, 5, ADC_71)
    op(75, "ADC", 2, 4, ADC_75)
    op(76, "ROR", 3, 6, ROR_76)
    op(78, "SEI", 1, 2, SEI_78)
    op(79, "ADC", 3, 4, ADC_79)
    op(7D, "ADC", 3, 4, ADC_7D)
    op(7E, "ROR", 3, 7, ROR_7E)

    op(81, "STA", 2, 6, STA_81)
    op(84, "STY", 2, 3, STY_84)
    op(85, "STA", 2, 3, STA_85)
    op(86, "STX", 2, 3, STX_86)
    op(88, "DEY", 1, 2, DEY_88)
    op(8A, "TXA", 1, 2, TXA_8A)
    op(8C, "STY", 3, 4, STY_8C)
    op(8D, "STA", 3, 4, STA_8D)
    op(8E, "STX", 3, 4, STX_8E)

    op(90, "BCC", 2, 2, BCC_90)
    op(91, "STA", 2, 6, STA_91)
    op(94, "STY", 2, 4, STY_94)
    op(95, "STA", 2, 4, STA_95)
    op(96, "STX", 2, 4, STX_96)
    op(98, "TYA", 1, 2, TYA_98)
    op(99, "STA", 3, 5, STA_99)
    op(9A, "TXS", 1, 2, TXS_9A)
    op(9D, "STA", 3, 5, STA_9D)

    op(A0, "LDY", 2, 2, LDY_A0)
    op(A1, "LDA", 2, 6, LDA_A1)
    op(A2, "LDX", 2, 2, LDX_A2)
    op(A4, "LDY", 2, 3, LDY_A4)
    op(A5, "LDA", 2, 3, LDA_A5)
    op(A6, "LDX", 2, 3, LDX_A6)
    op(A8, "TAY", 1, 2, TAY_A8)
    op(A9, "LDA", 2, 2, LDA_A9)
    op(AA, "TAX", 1, 2, TAX_AA)
    op(AC, "LDY", 3, 4, LDY_AC)
    op(AD, "LDA", 3, 4, LDA_AD)
    op(AE, "LDX", 3, 4, LDX_AE)

    op(B0, "BCS", 2, 2, BCS_B0)
    op(B1, "LDA", 2, 5, LDA_B1)
    op(B4, "LDY", 2, 4, LDY_B4)
    op(B5, "LDA", 2, 4, LDA_B5)
    op(B6, "LDX", 2, 4, LDX_B6)
    op(B8, "CLV", 1, 2, CLV_B8)
    op(B9, "LDA", 3, 4, LDA_B9)
    op(BA, "TSX", 1, 2, TSX_BA)
    op(BC, "LDY", 3, 4, LDY_BC)
    op(BD, "LDA", 3, 4, LDA_BD)
    op(BE, "LDX", 3, 4, LDX_BE)

    op(C0, "CPY", 2, 2, CPY_C0)
    op(C1, "CMP", 2, 6, CMP_C1)
    op(C4, "CPY", 2, 3, CPY_C4)
    op(C5, "CMP", 2, 3, CMP_C5)
    op(C6, "DEC", 2, 5, DEC_C6)
    op(C8, "INY", 1, 2, INY_C8)
    op(C9, "CMP", 2, 2, CMP_C9)
    op(CA, "DEX", 1, 2, DEX_CA)
    op(CC, "CPY", 3, 4, CPY_CC)
    op(CD, "CMP", 3, 4, CMP_CD)
    op(CE, "DEC", 3, 6, DEC_CE)

    op(D0, "BNE", 2, 2, BNE_D0)
    op(D1, "CMP", 2, 5, CMP_D1)
    op(D5, "CMP", 2, 4, CMP_D5)
    op(D6, "DEC", 2, 6, DEC_D6)
    op(D8, "CLD", 1, 2, CLD_D8)
    op(D9, "CMP", 3, 4, CMP_D9)
    op(DD, "CMP", 3, 4, CMP_DD)
    op(DE, "DEC", 3, 7, DEC_DE)

    op(E0, "CPX", 2, 2, CPX_E0)
    op(E1, "SBC", 2, 6, SBC_E1)
    op(E4, "CPX", 2, 3, CPX_E4)
    op(E5, "SBC", 2, 3, SBC_E5)
    op(E6, "INC", 2, 5, INC_E6)
    op(E8, "INX", 1, 2, INX_E8)
    op(E9, "SBC", 2, 2, SBC_E9)
    op(EA, "NOP", 1, 2, NOP_EA)
    op(EC, "CPX", 3, 4, CPX_EC)
    op(ED, "SBC", 3, 4, SBC_ED)
    op(EE, "INC", 3, 6, INC_EE)

    op(F0, "BEQ", 2, 2, BEQ_F0)
    op(F1, "SBC", 2, 5, SBC_F1)
    op(F5, "SBC", 2, 4, SBC_F5)
    op(F6, "INC", 2, 6, INC_F6)
    op(F8, "SED", 1, 2, SED_F8)
    op(F9, "SBC", 3, 4, SBC_F9)
    op(FD, "SBC", 3, 4, SBC_FD)
    op(FE, "INC", 3, 7, INC_FE)

}

void init_cpu()
{
    cpu.IP = 0xC004;
    cpu.SP = 0xFD;
    cpu.X = 0;
    cpu.Y = 0;
    cpu.P = 0;
    cpu.A = 0;
}

void read_code(WORD addr)
{
    BYTE op = read_byte(addr);

    printf("code[%s]\n", code_maps[op].op_name);
    //cpu->X = 
}

void parse_code()
{
    BYTE addr1 = read_byte(0xFFFC);
    BYTE addr2 = read_byte(0xFFFD);

    WORD addr = addr2 << 8 | addr1;
    PC = addr;

    cpu.P = 0x24;
    cpu.cycle = 7;

    addr = 0xC000;
    PC = 0xC000;

    size_t c = 0;

    while(addr) {

       // if(c == 8992) break;

        BYTE code = read_byte(addr);
        if(!code_maps[code].op_name) {
            printf("addr:%04X, code:%02X \n", addr, code);
            break;
        }

        printf("%04X %02X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%zu\n", \
               PC, code, cpu.A, cpu.X, cpu.Y, cpu.P, cpu.SP, cpu.cycle);

        code_maps[code].op_func(code);
        cpu.cycle += code_maps[code].cycle;

        addr = PC;

        ++c;
    }

}

void show_code(ROM *rom)
{
    init_cpu();

    init_code();

    _MEM *mem = (_MEM*)malloc(sizeof(_MEM));

    mem->PROG_ROM_LOWER = rom->prg_rom;
    mem->PROG_ROM_UPPER = rom->prg_rom;

    ROM_HEADER *header = rom->header;
    if(header->prg_rom_count > 1) {
        mem->PROG_ROM_UPPER = rom->prg_rom + 0x8000;
    }

    mem_init(mem);

    parse_code();

    if(1) return;


    BYTE addr1 = read_byte(0xFFFE);
    BYTE addr2 = read_byte(0xFFFF);

    WORD addr = addr2 << 8 | addr1;

    printf("IRQ/BRK:%04X\n", addr);

    addr1 = read_byte(0xFFFC);
    addr2 = read_byte(0xFFFD);

    addr = addr2 << 8 | addr1;

    printf("RESET:%04X\n", addr);

    BYTE code = read_byte(addr);
    printf("code:%02X\n", code);
    getchar();

    printf("code:%s\n", code_maps[code].op_name);

    addr1 = read_byte(0xFFFA);
    addr2 = read_byte(0xFFFB);

    addr = addr2 << 8 | addr1;

    printf("NMI:%04X\n", addr);
}