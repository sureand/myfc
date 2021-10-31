#include "cpu.h"
#include "load_rom.h"
#include "memory.h"
#include "disasm.h"


static inline BYTE test_bit(BYTE data, BYTE nr)
{
    return data & ( 1 << nr);
}

static inline BYTE test_flag(BYTE flag)
{
    return test_bit(cpu.P, flag);
}

static INLINE_VOID set_bit(volatile BYTE *addr, BYTE nr)
{
    (*addr) |= ( 1 << nr);
}

static INLINE_VOID clear_bit(volatile BYTE *addr, BYTE nr)
{
    (*addr) &= ~(1 << nr);
}

static INLINE_VOID set_flag(BYTE flag)
{
    set_bit(&cpu.P, flag);
}

static INLINE_VOID clear_flag(BYTE flag)
{
    clear_bit(&cpu.P, flag);
}

static INLINE_VOID push(BYTE data)
{
    write_byte(cpu.SP, data);
    --cpu.SP;
}

static inline BYTE pop()
{
    ++cpu.SP;
    return read_byte(cpu.SP);
}

/**
 * 指令设计开始
 * IP 永远指向函数的开始地方
 * 因此需要识别指令长度, 调整 IP 的值
 */

INLINE_VOID jump(WORD address)
{
    PC = read_word(address);
}

static inline WORD IRQ_vector()
{
    return read_word(0xFFFE);
}

//立即寻址
static inline WORD immediate_addressing()
{
    WORD addr = PC + 1;
    return addr;
}

//间接 X 寻址
static inline WORD indirext_x_address(WORD address)
{
    BYTE addr1 = read_byte(address + cpu.X);
    BYTE addr2 = read_byte(address + cpu.X + 1);
    WORD addr = (addr2 << 8) | addr1;

    return addr;
}

//绝对寻址
static inline WORD absolute_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
    WORD addr = (addr2 << 8 | addr1);

    return addr;
}

//零页寻址
static inline WORD zero_absolute_addressing()
{
    BYTE addr = read_byte(PC + 2);
    return (addr & 0xFF);
}

//绝对 X 变址
static inline WORD absolute_X_indexed_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
    WORD addr = (addr2 >> 8) | addr1;

    return (addr + cpu.X);
}

//绝对 Y 变址
static inline WORD absolute_X_indexed_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);
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
static inline WORD absolute_X_indexed_addressing()
{
    BYTE addr = read_byte(PC + 1);
    addr += cpu.Y;

    return (addr & 0xFF);
}

//间接寻址
static inline WORD indirect_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    BYTE addr2 = read_byte(PC + 2);

    WORD addr = (addr2 << 8) | addr1;
    return read_word(addr);
}

//间接 x 变址寻址
static inline WORD indirect_x_indexed_addressing()
{
    BYTE base_addr = read_byte(PC + 1);

    BYTE addr1= read_byte(cpu.X + base_addr);
    BYTE addr2= read_byte(cpu.X + base_addr + 1);

    WORD addr = (addr2 << 8) | addr1;
    return addr2;
}

//间接 Y 变址寻址
static inline WORD indirect_x_indexed_addressing()
{
    BYTE addr1 = read_byte(PC + 1);
    addr1 &= 0xFF;

    BYTE addr2 = addr1 + 1;
    WORD addr = (read_byte(addr2) << 8) || read_byte(addr1);

    return addr;
}

//相对寻址
static inline WORD relative_addressing()
{
    BYTE addr = (char)read_byte(PC + 1);
    addr = PC + addr;

    return addr;
}

//BRK 中断
INLINE_VOID BRK_00()
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

INLINE_VOID ORA_01()
{
    WORD base_addr = read_word(PC + 1);
    WORD addr = indirext_x_address(base_addr);

    cpu.A = cpu.A || read_byte(addr);
}

INLINE_VOID ORA_05()
{
    
}

INLINE_VOID ASL_06()
{
    
}

INLINE_VOID PHP_08()
{
    push(cpu.P);
}

INLINE_VOID ORA_09()
{
    BYTE bt = read_byte(PC + 1);
    ++PC;

    cpu.A = cpu.A || bt;
}

INLINE_VOID ASL_0A()
{

}

INLINE_VOID ASL_0D()
{
    
}

INLINE_VOID ASL_0E()
{
    
}

INLINE_VOID BPL_10()
{
    WORD addr = read_word(PC + 1);

    if(test_flag(ZERO)) { PC = addr; return; }
}

INLINE_VOID ORA_11()
{
    
}

INLINE_VOID ORA_15()
{
    
}

INLINE_VOID ASL_16()
{
    
}

INLINE_VOID CLC_18()
{
    
}

INLINE_VOID ORA_19()
{
    
}

INLINE_VOID ORA_1D()
{
    
}

INLINE_VOID ASL_1E()
{
    
}

INLINE_VOID AND_21()
{
    
}

INLINE_VOID BIT_24()
{
    
}

INLINE_VOID AND_25()
{
    
}

INLINE_VOID ROL_26()
{
    
}

INLINE_VOID PLP_28()
{
    
}

INLINE_VOID AND_29()
{
    
}

INLINE_VOID ORL_2A()
{
    
}

INLINE_VOID BIT_2C()
{
    
}

INLINE_VOID AND_2D()
{

}

INLINE_VOID ROL_2E()
{

}

INLINE_VOID BMI_30()
{

}

INLINE_VOID AND_31()
{

}

INLINE_VOID AND_35()
{

}

INLINE_VOID ROL_36()
{

}

INLINE_VOID SEC_38()
{

}

INLINE_VOID SEC_39()
{

}

INLINE_VOID AND_3D()
{

}

INLINE_VOID ROL_3E()
{

}

INLINE_VOID RTI_40()
{

}

INLINE_VOID EOR_41()
{

}

INLINE_VOID EOR_45()
{

}

INLINE_VOID LSR_46()
{

}

INLINE_VOID PHA_48()
{

}

INLINE_VOID EOR_49()
{

}

INLINE_VOID LSR_4A()
{

}

INLINE_VOID JMP_4C()
{

}

INLINE_VOID EOR_4D()
{

}

INLINE_VOID LSR_4E()
{

}

INLINE_VOID BVC_50()
{

}

INLINE_VOID EOR_51()
{
    
}

INLINE_VOID EOR_55()
{
    
}

INLINE_VOID LSR_56()
{
    
}

INLINE_VOID CLI_58()
{

}

INLINE_VOID EOR_59()
{

}

INLINE_VOID EOR_5D()
{

}

INLINE_VOID LSR_5E()
{

}

INLINE_VOID RTS_60()
{

}

INLINE_VOID ADC_61()
{

}

INLINE_VOID ADC_65()
{

}

INLINE_VOID ROR_66()
{

}

INLINE_VOID PLA_68()
{
    cpu.A = pop();
}

INLINE_VOID ADC_69()
{

}

INLINE_VOID ROA_6A()
{

}

INLINE_VOID JMP_6C()
{

}

INLINE_VOID ADC_6D()
{

}

INLINE_VOID ROR_6E()
{

}

INLINE_VOID BVS_70()
{

}

INLINE_VOID ADC_71()
{

}

INLINE_VOID ADC_75()
{

}

INLINE_VOID ROR_76()
{

}

INLINE_VOID SEI_78()
{
    set_flag(INT);
}

INLINE_VOID ADC_79()
{

}

INLINE_VOID ADC_7D()
{

}

INLINE_VOID ROR_7E()
{

}

INLINE_VOID STA_81()
{

}

INLINE_VOID STY_84()
{
    
}

INLINE_VOID STA_85()
{
    
}

INLINE_VOID STX_86()
{
    
}

INLINE_VOID DEY_88()
{
    
}

INLINE_VOID STY_8C()
{
    
}

INLINE_VOID STA_8D()
{
    WORD addr = read_word(PC + 1);

    BYTE bt = read_byte(addr);
    cpu.A = bt;
}

INLINE_VOID STX_8E()
{
    WORD addr = read_word(PC + 1);

    BYTE bt = read_byte(addr);
    cpu.X = bt;
}

INLINE_VOID BCC_90()
{
    
}

INLINE_VOID STA_91()
{
    
}

INLINE_VOID STY_94()
{
    
}

INLINE_VOID STA_95()
{
    
}

INLINE_VOID STX_96()
{
    
}

INLINE_VOID TYA_98()
{
    
}

INLINE_VOID STA_99()
{
    
}

INLINE_VOID TXS_9A()
{
    write_byte(cpu.SP, cpu.X);
    --cpu.SP;
}

INLINE_VOID STA_9D()
{
    
}

INLINE_VOID LDY_A0()
{

}

INLINE_VOID LDA_A1()
{
    
}

INLINE_VOID LDX_A2()
{
    BYTE bt = read_byte(PC + 1);
    cpu.X = bt;
}

INLINE_VOID LDA_A4()
{
    
}

INLINE_VOID LDA_A5()
{
    
}

INLINE_VOID LDX_A6()
{
    
}

INLINE_VOID TAY_A8()
{
    
}

INLINE_VOID LDA_A9()
{
    BYTE bt = read_byte(PC + 1);

    cpu.A = bt;
}

INLINE_VOID TAX_AA()
{
    
}

INLINE_VOID LDA_AD()
{
    WORD addr = read_word(PC + 1);

    BYTE bt = read_byte(addr);
    cpu.A = bt;
}

INLINE_VOID LDX_AE()
{
    
}

INLINE_VOID BCS_80()
{
    
}

INLINE_VOID LDA_B1()
{
    
}

INLINE_VOID LDY_B4()
{
    
}

INLINE_VOID LDA_B5()
{
    
}

INLINE_VOID LDX_B6()
{
    
}

INLINE_VOID CLV_B8()
{
    
}

INLINE_VOID LDA_B9()
{
    
}

INLINE_VOID TSX_BA()
{
    
}

INLINE_VOID LDY_BC()
{
    
}

INLINE_VOID LDA_BD()
{
    
}

INLINE_VOID LDX_BE()
{
    
}

INLINE_VOID CPY_C0()
{
    
}

INLINE_VOID CMP_C1()
{
    
}

INLINE_VOID CPY_C4()
{
    
}

INLINE_VOID CMP_C5()
{
    
}

INLINE_VOID DEC_C6()
{
    
}

INLINE_VOID INY_C8()
{
    
}

INLINE_VOID CMP_C9()
{
    
}

INLINE_VOID DEX_CA()
{
    
}

INLINE_VOID CPY_CC()
{
    
}

INLINE_VOID CMP_CD()
{
    
}

INLINE_VOID DEC_CE()
{
    
}


INLINE_VOID BNE_D0()
{
    WORD addr = read_word(PC + 1);

    BYTE bt = read_byte(addr);
    cpu.X = bt;
}

INLINE_VOID CMP_D1()
{
    
}

INLINE_VOID CMP_D5()
{
    
}

INLINE_VOID DEC_D6()
{
    
}

INLINE_VOID CLD_D8()
{
    clear_flag(DEC);
}

INLINE_VOID CMP_D9()
{
    
}

INLINE_VOID CMP_DD()
{
    
}

INLINE_VOID DEC_DE()
{
    
}

INLINE_VOID CPX_E0()
{
    WORD addr = read_word(PC + 1);

    BYTE bt = read_byte(addr);
    cpu.X = bt;
}

INLINE_VOID SBC_E1()
{
    
}

INLINE_VOID CPX_E4()
{
    
}

INLINE_VOID SBC_E5()
{
    
}

INLINE_VOID INC_E6()
{
    
}

INLINE_VOID INX_E8()
{
    
}

INLINE_VOID SBC_E9()
{
    
}

INLINE_VOID NOP_EA()
{
    
}

INLINE_VOID CPX_EC()
{
    
}

INLINE_VOID SBC_ED()
{
    
}

INLINE_VOID INC_EE()
{
    
}

INLINE_VOID BEQ_F0()
{
    
}

INLINE_VOID SBC_F1()
{
    
}

INLINE_VOID SBC_F5()
{
    
}

INLINE_VOID INC_F6()
{
    
}

INLINE_VOID SED_F8()
{
    
}

INLINE_VOID SBC_F9()
{
    
}

INLINE_VOID SBC_FD()
{
    BYTE bt = read_byte(PC + 1);
    cpu.A -= bt;

}

INLINE_VOID INC_FE()
{
    
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
    cpu.SP = 0xFF;
    cpu.X = 0;
    cpu.Y = 0;
    cpu.P = 0;
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

    printf("RESET:%04X\n", addr);

    while(addr) {
        BYTE code = read_byte(addr);
        if(!code_maps[code].op_name) {
            printf("addr:%04X, code:%02X \n", addr, code);
            break;
        }
        printf("code:%s:%02X len:%d\n", code_maps[code].op_name, code, code_maps[code].op_len);
        addr += code_maps[code].op_len;
    }

}

void show_code(ROM *rom)
{
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

    /*

    - $FFFA-FFFB = NMI  ' 这里 - 指的是地址，不是减号'
    - $FFFC-FFFD = RESET
    - $FFFE-FFFF = IRQ/BRK

    */

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