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

static void set_nz(char value)
{
    //0 清空负数标记, 打上0 标记
    if(value == 0) {
        clear_flag(NEG);
        set_flag(ZERO);
        return;
    }

    //正数, 清空负数和 0 标记
    if(value > 0) {
        clear_flag(NEG);
        clear_flag(ZERO);
        return;
    }

    //负数 打上负数标记和清空 0 标记
    set_flag(NEG);    
    clear_flag(ZERO);
}

static void push(BYTE data)
{
    printf("push data:%02X\n", data);
    getchar();

    write_byte(cpu.SP, data);
    --cpu.SP;
}

static inline BYTE pop()
{
    BYTE bt = read_byte(++cpu.SP);
    printf("pop data:%02X\n", bt);
    getchar();

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
    ++PC;

    return addr;
}

#if 0
//间接 X 寻址
static inline WORD indirext_x_address()
{
    WORD address = read_byte(PC + 1) & 0xFF;
    PC += 1;

    BYTE addr1 = read_byte(address + cpu.X);
    BYTE addr2 = read_byte(address + cpu.X + 1);
    WORD addr = (addr2 << 8) | addr1;

    return addr;
}
#endif

#if 0
//现在用不到
// 间接 Y 寻址
static inline WORD indirext_y_address()
{
    WORD address = read_byte(PC + 1);

    BYTE addr1 = read_byte(address + cpu.Y);
    BYTE addr2 = read_byte(address + cpu.Y + 1);
    WORD addr = (addr2 << 8) | addr1;

    return addr;
}
#endif

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
static inline WORD absolute_X_indexed_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
    PC += 3;

    WORD addr = (addr2 << 8) | addr1;

    return (addr + cpu.X);
}

//绝对 Y 变址
static inline WORD absolute_Y_indexed_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
    PC += 3;

    WORD addr = (addr2 >> 8) | addr1;

    return (addr + cpu.Y);
}

//零页 X 间接寻址
static inline WORD zero_X_indexed_addressing()
{
    BYTE addr = read_byte(PC + 1);
    addr += cpu.X;

    return (addr & 0xFF);
}

//零页 Y 间接 寻址
static inline WORD zero_Y_indexed_addressing()
{
    BYTE addr = read_byte(PC + 1);
    addr += cpu.Y;

    return (addr & 0xFF);
}

static inline WORD indirect_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
    PC += 3;

    WORD addr = (addr2 << 8) | addr1;
    return read_word(addr);
}

//间接 x 变址寻址
static inline WORD indirect_X_indexed_addressing()
{
    BYTE base_addr = read_byte(PC + 1);

    BYTE addr1 = read_byte(cpu.X + base_addr);
    BYTE addr2 = read_byte(cpu.X + base_addr + 1);

    WORD addr = (addr2 << 8) | addr1;
    return addr;
}

//间接 Y 变址寻址
static inline WORD indirect_Y_indexed_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    addr1 &= 0xFF;

    BYTE addr2 = addr1 + 1;
    WORD addr = (read_byte(addr2) << 8) | read_byte(addr1);

    return addr;
}

static inline WORD relative_addressing()
{
    BYTE addr = (char)read_byte(PC + 1);

    if(!test_flag(ZERO)) { return PC - addr; }

    return PC + addr;
}

//BRK 中断
void BRK_00()
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

void ORA_01()
{
    WORD addr = indirect_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    ++PC;

    cpu.A = cpu.A || bt;
}

void ORA_05()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    ++PC;

    cpu.A = cpu.A || bt;
}

void ASL_06()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    ++PC;

    bt <<= 1;

    write_byte(addr, bt);
}

void PHP_08()
{
    push(cpu.P);
    ++PC;
}

void ORA_09()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A || bt;
}

void ASL_0A()
{
    cpu.A <<= 1;
}

void ORA_0D()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A || bt;
}

void ASL_0E()
{
    WORD addr = absolute_addressing();
    
    BYTE bt = read_byte(addr);
    bt <<= 1;

    write_byte(addr, bt);
}

void BPL_10()
{
    char of = read_byte(PC + 1);
    PC += 2;

    //TODO
    set_flag(NEG);

    if(test_flag(NEG)) return;

    PC += of;
}

void ORA_11()
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A || bt;
}

void ORA_15()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A || bt;
}

void ASL_16()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    bt <<= 1;

    write_byte(addr, bt);
}

void CLC_18()
{
    clear_flag(CARRY);
}

void ORA_19()
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A || bt;
}

void ORA_1D()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A || bt;
}

void ASL_1E()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    bt <<= 1;

    write_byte(addr, bt);
}

//TODO
void JSR_20()
{
    WORD cur_pc = PC;
    BYTE *bp = (BYTE*)&PC;
    push(*(bp + 1));
    push(*(bp));

    BYTE addr1 = read_byte(cur_pc + 1);
    BYTE addr2 = read_byte(cur_pc + 2);
    WORD addr = addr2 << 8 | addr1;
    PC = addr;

    printf("read addr1:%02X, addr2:%02X\n", addr1, addr2);

    printf("jump to:%4X\n", PC);

    getchar();
}

void AND_21()
{
    WORD addr = indirect_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A && bt;
}

//TODO:
void BIT_24()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    printf("not complete! :%02X\n", bt);
}

void AND_25()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A && bt;
}

void ROL_26()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    bt <<= 1;
}

void PLP_28()
{
    cpu.P = pop();
}

void AND_29()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A && bt;
}

void ROL_2A()
{
    cpu.A <<= 1;
}

//TODO
void BIT_2C()
{
    //what the fuck
}

void AND_2D()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A && bt;
}

void ROL_2E()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    bt <<= 1;
    write_byte(addr, bt);
}

void BMI_30()
{
    char addr = (char)read_byte(PC + 1);
    if(test_flag(NEG)) {
        PC += addr;
        return;
    }

    ++PC;
}

void AND_31()
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A && bt;
}

void AND_35()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A && bt;
}

void ROL_36()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    bt <<= 1;

    write_byte(addr, bt);
}

void SEC_38()
{
    set_flag(CARRY);
}

void AND_39()
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A && bt;
}

void AND_3D()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A && bt;
}

void ROL_3E()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    bt <<= 1;

    write_byte(addr, bt);
}

void RTI_40()
{
    BYTE addr1 = pop();
    set_flag(addr1);

    BYTE addr2 = pop();

    WORD addr = (addr2 << 8 | addr1);
    WORD ret_addr = read_word(addr);

    PC = ret_addr;
}

void EOR_41()
{
    WORD addr = indirect_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A ^ bt;
}

void EOR_45()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A ^ bt;
}

void LSR_46()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    bt >>= 1;

    write_byte(addr, bt);
}

void PHA_48()
{
    push(cpu.A);
}

void EOR_49()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A ^ bt;
}

void LSR_4A()
{
    cpu.A >>= 1;
}

void JMP_4C()
{
    WORD addr = absolute_addressing();
    PC = read_word(addr);
}

void EOR_4D()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A ^ bt;
}

void LSR_4E()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    bt >>= 1;

    write_byte(addr, bt);
}

void BVC_50()
{
    PC = relative_addressing();
}

void EOR_51()
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A ^ bt;
}

void EOR_55()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = cpu.A ^ bt;
}

void LSR_56()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    bt >>= 1;

    write_byte(addr, bt);
}

void CLI_58()
{
    clear_flag(INT);
}

//TODO
void EOR_59()
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;
}

void EOR_5D()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    cpu.A = cpu.A ^ bt;
}

void LSR_5E()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    bt >>= 1;

    write_byte(addr, bt);
}

void RTS_60()
{
    BYTE addr1 = pop();
    BYTE addr2 = pop();

    WORD addr = addr2 << 8 | addr1;

    PC = addr + 1;
}

void ADC_61()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    short ret = cpu.A + bt;
    cpu.A = ret;

    if(ret > 0xFF) { set_flag(CARRY); }
}

void ADC_65()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    short ret = cpu.A + bt;
    cpu.A = ret;

    if(ret > 0xFF) { set_flag(CARRY); }
}

void ROR_66()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    bt >>= 1;

    write_byte(addr, bt);
}

void PLA_68()
{
    cpu.A = pop();
}

void ADC_69()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);

    short ret = cpu.A + bt;
    cpu.A = ret;

    if(ret > 0xFF) { set_flag(CARRY); }
}

void ROR_6A()
{
    cpu.A >>= 1;
}

void JMP_6C()
{
    WORD addr = indirect_addressing();
    PC = addr;
}

void ADC_6D()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    short ret = cpu.A + bt;
    cpu.A = ret;

    if(ret) { set_flag(CARRY); }
}

void ROR_6E()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    bt >>= 1;
    write_byte(addr, bt);
}

void BVS_70()
{
    PC = relative_addressing();
}

void ADC_71()
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    short x = cpu.A + bt;
    cpu.A = x;

    if(x > 0xFF) { set_flag(CARRY); }
}

void ADC_75()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    short x = cpu.A + bt;
    cpu.A = x;

    if(x > 0xFF) { set_flag(CARRY); }
}

void ROR_76()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    bt >>= 1;

    write_byte(addr, bt);
}

void SEI_78()
{
    ++PC;

    set_flag(INT);
}

void ADC_79()
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    short x = cpu.A + bt;
    cpu.A = x;

    if(x > 0xFF) { set_flag(CARRY); }
}

void ADC_7D()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    short x = cpu.A + bt;
    cpu.A = x;

    if(x > 0xFF) { set_flag(CARRY); }
}

void ROR_7E()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    bt >>= 1;

    write_byte(addr, bt);
}

void STA_81()
{
    WORD addr = indirect_X_indexed_addressing();
    write_byte(addr, cpu.A);
}

void STY_84()
{
    WORD addr = zero_absolute_addressing();
    write_byte(addr, cpu.Y);
}

void STA_85()
{
    WORD addr = zero_absolute_addressing();
    write_byte(addr, cpu.A);
}

void STX_86()
{
    WORD addr = zero_absolute_addressing();
    write_byte(addr, cpu.X);
}

void DEY_88()
{
    cpu.Y -= 1;
    ++PC;

    set_nz(cpu.Y);
}

void TXA_8A()
{
    cpu.Y -= 1;
}

void STY_8C()
{
    WORD addr = zero_absolute_addressing();
    write_byte(addr, cpu.Y);
    ++PC;
}

void STA_8D()
{
    WORD addr = absolute_addressing();
    printf("STA_8D next_PC:%04X, addr:%04X\n", PC, addr);
    write_byte(addr, cpu.A);
}

void STX_8E()
{
    WORD addr = absolute_addressing();
    printf("next PC:%04X, addr:%04X\n", PC, addr);
    write_byte(addr, cpu.X);
}

void BCC_90()
{
    char of = read_byte(PC + 1);
    if(test_flag(CARRY)) {
        PC += of;
        return;
    }
    ++PC;
}

void STA_91()
{
    WORD addr = indirect_Y_indexed_addressing();
    write_byte(addr, cpu.A);
}

void STY_94()
{
    WORD addr = zero_X_indexed_addressing();
    write_byte(addr, cpu.Y);
}

void STA_95()
{
    WORD addr = zero_X_indexed_addressing();
    write_byte(addr, cpu.A);
}

void STX_96()
{
    WORD addr = zero_Y_indexed_addressing();
    write_byte(addr, cpu.X);
}

void TYA_98()
{
    cpu.A = cpu.Y;
}

void STA_99()
{
    WORD addr = absolute_Y_indexed_addressing();
    write_byte(addr, cpu.A);
}

void TXS_9A()
{
    ++PC;

    cpu.SP = cpu.X;
}

void STA_9D()
{
    WORD addr = absolute_X_indexed_addressing();
    write_byte(addr, cpu.A);
}

void LDY_A0()
{
    WORD addr = immediate_addressing();
    ++PC;

    BYTE bt = read_byte(addr);
    set_nz(bt);

    cpu.Y = bt;
}

void LDA_A1()
{
    WORD addr = indirect_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    ++PC;

    cpu.A= bt;
}

void LDX_A2()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    ++PC;

    set_nz(bt);

    cpu.X = bt;
}

void LDY_A4()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);

    cpu.Y = bt;
}

void LDA_A5()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = bt;
}

void LDX_A6()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    cpu.X = bt;
}

void TAY_A8()
{
    cpu.Y = cpu.A;
}

void LDA_A9()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    ++PC;

    set_nz(bt);

    cpu.A = bt;
}

void TAX_AA()
{
    cpu.X = cpu.A;
}

void LDY_AC()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    cpu.Y = bt;
}

void LDA_AD()
{
    WORD addr = absolute_addressing();

    BYTE bt = read_byte(addr);
    set_nz(bt);

    cpu.A = bt;
}

void LDX_AE()
{
    WORD addr = absolute_addressing();
    ++PC;

    BYTE bt = read_byte(addr);

    cpu.X = bt;
}

void BCS_B0()
{
    char of = (char)read_byte(PC + 1);
    PC += 2;

    if(!test_flag(CARRY)) return;

    PC += of;
}

void LDA_B1()
{
    WORD addr = indirect_Y_indexed_addressing();
    PC += 2;

    BYTE bt = read_byte(addr);
    set_nz(bt);

    cpu.A = bt;
}

void LDY_B4()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.Y = bt;
}

void LDA_B5()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = bt;
}

void LDX_B6()
{
    WORD addr = zero_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.X = bt;
}

void CLV_B8()
{
    clear_flag(OF);
}

void LDA_B9()
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.A = bt;
}

void TSX_BA()
{
    ++PC;
    cpu.X = cpu.SP;
}

void LDY_BC()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.Y = bt;
}

void LDA_BD()
{
    WORD addr = absolute_X_indexed_addressing();

    BYTE bt = read_byte(addr);

    cpu.A = bt;

    set_nz(cpu.A);
}

void LDX_BE()
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    cpu.X = bt;
}

void CPY_C0()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.Y - bt;

    if (ret < 0) { set_flag(NEG); }
}

void CMP_C1()
{
    WORD addr = indirect_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.A - bt;

    if(ret < 0) { set_flag(NEG); }

}

void CPY_C4()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.Y - bt;

    if(ret < 0 ) { set_flag(NEG); }
}

void CMP_C5()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.A - bt;

    if(ret < 0) { set_flag(NEG); }

}

void DEC_C6()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    write_byte(addr, bt - 1);
}

void INY_C8()
{
    ++PC;
    cpu.Y += 1;
}

void CMP_C9()
{
    WORD addr = immediate_addressing();
    ++PC;

    BYTE bt = read_byte(addr);
    char ret = cpu.A - bt;

    if(ret < 0) { set_flag(NEG); }
}

void DEX_CA()
{
    cpu.X -= 1;
    ++PC;

    set_nz(cpu.X);
}

void CPY_CC()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.Y- bt;

    if(ret < 0) { set_flag(NEG); }
}

void CMP_CD()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.A - bt;

    if(ret < 0) { set_flag(NEG); }
}

void DEC_CE()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    write_byte(addr, bt - 1);
}


void BNE_D0()
{
    char of = read_byte(PC + 1);
    WORD last = PC;

    PC += 2;

    printf("PC:%04X, next:%4X\n", last, PC);

    if(test_flag(ZERO)) return;

    PC += of;
}

void CMP_D1()
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.A - bt;

    if(ret < 0) { set_flag(NEG); }
}

void CMP_D5()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.A - bt;

    if(ret < 0) { set_flag(NEG); }
}

void DEC_D6()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    write_byte(addr, bt - 1);
}

void CLD_D8()
{
    ++PC;

    clear_flag(DEC);
}

void CMP_D9()
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.A - bt;

    if(ret < 0) { set_flag(NEG); }
}

void CMP_DD()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.X - bt;

    if(ret < 0) { set_flag(NEG); }
}

void DEC_DE()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.X - bt;

    if(ret < 0) { set_flag(NEG); }
}

void CPX_E0()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);
    ++PC;

    char ret = cpu.X - bt;
    set_nz(ret);

    if(ret >= 0) set_flag(CARRY);
}

void SBC_E1()
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    char ret = cpu.X - bt;
    cpu.X = ret;

    if(ret < 0 ) { set_flag(NEG); }
}

void CPX_E4()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);
    char ret = cpu.X - bt;

    if(ret < 0) { set_flag(NEG); }
}

void SBC_E5()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    char ret = cpu.A - bt;
    cpu.A = ret;

    if(ret < 0) { set_flag(NEG); }

}

void INC_E6()
{
    WORD addr = zero_absolute_addressing();
    BYTE bt = read_byte(addr);

    write_byte(addr, bt + 1);
}

void INX_E8()
{
    cpu.X += 1;
    ++PC;

    set_nz(cpu.X);
}

void SBC_E9()
{
    WORD addr = immediate_addressing();
    BYTE bt = read_byte(addr);

    char ret = cpu.A - bt;
    cpu.A = ret;

    if(ret < 0) { set_flag(ZERO); }
}

void NOP_EA()
{
    fprintf(stderr, "nop!");
    //do nothing
}

void CPX_EC()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    char ret = cpu.A - bt;
    if(ret < 0) { set_flag(ZERO); }
}

void SBC_ED()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    char ret = cpu.A - bt;
    cpu.A = ret;

    if(ret < 0) { set_flag(NEG); }
}

void INC_EE()
{
    WORD addr = absolute_addressing();
    BYTE bt = read_byte(addr);

    write_byte(addr, bt + 1);
}

void BEQ_F0()
{
    char of = read_byte(PC + 1);
    PC += 2;

    if(!test_flag(ZERO))  return;

    PC += of;
}

void SBC_F1()
{
    WORD addr = indirect_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    char ret = cpu.A - bt;
    cpu.A = ret;

    if(ret < 0) { set_flag(NEG); }
}

void SBC_F5()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    PC += 3;

    char ret = cpu.A - bt;
    cpu.A = ret;

    if(ret < 0) { set_flag(NEG); }
}

void INC_F6()
{
    WORD addr = zero_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    write_byte(addr, bt + 1);
}

void SED_F8()
{
    set_flag(DEC);
}

void SBC_F9()
{
    WORD addr = absolute_Y_indexed_addressing();
    BYTE bt = read_byte(addr);

    char ret = cpu.A - bt;
    cpu.A = ret;

    if(ret < 0) { set_flag(NEG); }
}

void SBC_FD()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    char ret = cpu.A - bt;
    cpu.A = ret;

    if(ret < 0) { set_flag(NEG); }
}

void INC_FE()
{
    WORD addr = absolute_X_indexed_addressing();
    BYTE bt = read_byte(addr);

    write_byte(addr, bt + 1);
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
    op(24, "BIT", 2, 4, BIT_24)
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
    op(D6, "DEC", 2, 6, DEC_C6)
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
    op(F6, "INC", 2, 6, INC_E6)
    op(F8, "SED", 1, 2, SED_F8)
    op(F9, "SBC", 3, 4, SBC_F9)
    op(FD, "SBC", 3, 4, SBC_FD)
    op(FE, "INC", 3, 7, INC_FE)

}

void execute_code()
{
    BYTE code = 0x78;

    #define INC_PC() PC += code_maps[code].op_len
    for(;;) {
        if(!code_maps[code].op_name) {
            return;
        }
        code_maps[code].op_func();
        INC_PC();
    }
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

    int state = 0;
    while(addr) {
        BYTE code = read_byte(addr);
        if(!code_maps[code].op_name) {
            printf("addr:%04X, code:%02X \n", addr, code);
            break;
        }
        printf("PC:%02X, A:%02X, X:%02X, Y:%02X, P:%02X, SP:%02X, code:%s_%02X, len:%d\n", 
            cpu.IP, cpu.A, cpu.X, cpu.Y, cpu.P, cpu.SP, code_maps[code].op_name, code, code_maps[code].op_len);
        display_reg();
        printf("\n");

        code_maps[code].op_func();
        addr = PC;

        (void)state;
        /*
        if(addr == 0xC06C) {
            state = 1;
        }
        if(state)
        getchar();
        */
        

       // getchar();
        //addr += code_maps[code].op_len;
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


    BYTE addr1 = read_byte(0xFFFE);
    BYTE addr2 = read_byte(0xFFFF);

    WORD addr = addr2 << 8 | addr1;

    printf("IRQ/BRK:%04X\n", addr);

    addr1 = read_byte(0xFFFC);
    addr2 = read_byte(0xFFFD);

    addr = addr2 << 8 | addr1;

    printf("RESET:%04X\n", addr);

    BYTE code = read_byte(addr);
    printf("code:%s\n", code_maps[code].op_name);

    addr1 = read_byte(0xFFFA);
    addr2 = read_byte(0xFFFB);

    addr = addr2 << 8 | addr1;

    printf("NMI:%04X\n", addr);
}