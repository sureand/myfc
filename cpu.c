#include "cpu.h"
#include "handler.h"
#include "memory.h"

//立即寻址
static inline WORD immediate_addressing()
{
    WORD addr = PC + 1;
    PC += 1;

    return addr;
}

//绝对寻址
static inline WORD absolute_addressing()
{
    BYTE addr1 = bus_read(PC + 1);
    BYTE addr2 = bus_read(PC + 2);
    WORD addr = (addr2 << 8 | addr1);
    PC += 3;

    return addr;
}

//绝对零页寻址
static inline WORD zero_absolute_addressing()
{
    BYTE addr = bus_read(PC + 1);
    PC += 2;

    return (addr & 0xFF);
}

//绝对 X 变址
static inline WORD absolute_X_indexed_addressing(BYTE op)
{
    BYTE addr1 = bus_read(PC + 1);
    BYTE addr2 = bus_read(PC + 2);
    PC += 3;

    WORD addr = (addr2 << 8) | addr1;

    if( code_maps[op].cycle < 5 && (addr >> 8) != ((addr + cpu.X) >> 8) )
        ++cpu.cycle;

    return (addr + cpu.X);
}

//绝对 Y 变址
static inline WORD absolute_Y_indexed_addressing(BYTE is_store_instr)
{
    BYTE addr1 = bus_read(PC + 1);
    BYTE addr2 = bus_read(PC + 2);
    PC += 3;

    WORD addr = (addr2 << 8) | addr1;

    if (!is_store_instr) {
        if((addr >> 8) != ((addr + cpu.Y) >> 8)) ++cpu.cycle;
    }

    return (addr + cpu.Y);
}

//零页 X 间接寻址
static inline WORD zero_X_indexed_addressing()
{
    BYTE addr = bus_read(PC + 1);
    addr += cpu.X;
    PC += 2;

    return (addr & 0xFF);
}

//零页 Y 间接 寻址
static inline WORD zero_Y_indexed_addressing()
{
    BYTE addr = bus_read(PC + 1);
    addr += cpu.Y;
    PC += 2;

    return (addr & 0xFF);
}

static inline WORD indirect_addressing()
{
    BYTE addr1 = bus_read(PC + 1);
    BYTE addr2 = bus_read(PC + 2);
    PC += 3;

    WORD addr = (addr2 << 8) | addr1;

    //这个是6502CPU的Bug
    if((addr & 0xFF) == 0xFF) {
        addr = (bus_read(addr & 0xFF00) << 8) + bus_read(addr);
        return addr;
    }

    BYTE pc1 = bus_read(addr);
    BYTE pc2 = bus_read(addr + 1);

    return (pc2 << 8 )| pc1;
}

// x 变址间接 寻址
static inline WORD indexed_X_indirect_addressing()
{
    BYTE addr1 = bus_read(PC + 1);
    addr1 += cpu.X;

    BYTE addr2 = addr1 + 1;
    WORD addr = (bus_read(addr2) << 8) | bus_read(addr1);

    PC += 2;

    return addr;
}

//Y 间接 变址寻址
static inline WORD indirect_Y_indexed_addressing(BYTE is_store_instr)
{
    BYTE addr1 = bus_read(PC + 1);
    BYTE addr2 = (addr1 + 1) & 0xFF;  // 确保在页面边界正确处理
    WORD addr = (bus_read(addr2) << 8) | bus_read(addr1);
    PC += 2;

    if (!is_store_instr) {
        WORD addr_plus_Y = addr + cpu.Y;
        if ((addr & 0xFF00) != (addr_plus_Y & 0xFF00)) {
            ++cpu.cycle;
        }
    }

    return addr + cpu.Y;
}

static inline WORD relative_addressing()
{
    //先算下一条指令的地址, 再算偏移
    int8_t of = (int8_t)bus_read(PC + 1);
    PC += 2;

    WORD addr = PC + of;

    return addr;
}

//BRK 中断
void BRK_00(BYTE op)
{
    //这里的addr 没啥用
    WORD addr = 0;

    handler_BRK(addr);
}

void ORA_01(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_ORA(addr);
}

void KIL_02(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void SLO_03(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_SLO(addr);
}

void DOP_04(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_DOP(addr);
}

void ORA_05(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_ORA(addr);
}

void ASL_06(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_ASL(addr);
}

void SLO_07(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_SLO(addr);
}

void PHP_08(BYTE op)
{
    WORD addr = 0;

    handler_PHP(addr);
}

void ORA_09(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_ORA(addr);
}

void ASL_0A(BYTE op)
{
    WORD addr = 0;

    handler_ASL_REG(addr);
}

void AAC_0B(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_AAC(addr);
}

void NOP_0C(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_NOP(addr);
}

void ORA_0D(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_ORA(addr);
}

void ASL_0E(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_ASL(addr);
}

void SLO_0F(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_SLO(addr);
}

void BPL_10(BYTE op)
{
    WORD addr = relative_addressing();

    handler_BPL(addr);
}

void ORA_11(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_ORA(addr);
}

void KIL_12(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void SLO_13(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_SLO(addr);
}

void DOP_14(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    (void)addr;
}

void ORA_15(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_ORA(addr);
}

void ASL_16(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_ASL(addr);
}

void SLO_17(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_SLO(addr);
}

void CLC_18(BYTE op)
{
    WORD addr = 0;

    handler_CLC(addr);
}

void ORA_19(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_ORA(addr);
}

void NOP_1A(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_NOP(addr);
}

void SLO_1B(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_SLO(addr);
}

void TOP_1C(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_TOP(addr);
}

void ORA_1D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_ORA(addr);
}

void ASL_1E(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_ASL(addr);
}

void SLO_1F(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_SLO(addr);
}

void JSR_20(BYTE op)
{
    WORD addr = 0;

    handler_JSR(addr);
}

void AND_21(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_AND(addr);
}

void KIL_22(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void RLA_23(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_RLA(addr);
}

void BIT_24(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_BIT(addr);
}

void AND_25(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_AND(addr);
}

void ROL_26(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_ROL(addr);
}

void RLA_27(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_RLA(addr);
}

void PLP_28(BYTE op)
{
    WORD addr = 0;

    handler_PLP(addr);
}

void AND_29(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_AND(addr);
}

void ROL_2A(BYTE op)
{
    WORD addr = 0;

    handler_ROL_REG_A(addr);
}

void AAC_2B(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_AAC(addr);
}

void BIT_2C(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_BIT(addr);
}

void AND_2D(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_AND(addr);
}

void ROL_2E(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_ROL(addr);
}

void RLA_2F(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_RLA(addr);
}

void BMI_30(BYTE op)
{
    WORD addr = relative_addressing();

    handler_BMI(addr);
}

void AND_31(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_AND(addr);
}

void KIL_32(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void RLA_33(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_RLA(addr);
}

void DOP_34(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_DOP(addr);
}

void AND_35(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_AND(addr);
}

void ROL_36(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_ROL(addr);
}

void RLA_37(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_RLA(addr);
}

void SEC_38(BYTE op)
{
    WORD addr = 0;

    handler_SEC(addr);
}

void AND_39(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_AND(addr);
}

void NOP_3A(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_NOP(addr);
}

void RLA_3B(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_RLA(addr);
}

void TOP_3C(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_TOP(addr);
}

void AND_3D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_AND(addr);
}

void ROL_3E(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_ROL(addr);
}

void RLA_3F(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_RLA(addr);
}

void RTI_40(BYTE op)
{
    WORD addr = 0;

    handler_RTI(addr);
}

void EOR_41(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_EOR(addr);
}

void KIL_42(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void SRE_43(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_SRE(addr);
}

void DOP_44(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_DOP(addr);
}

void EOR_45(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_EOR(addr);
}

void LSR_46(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_LSR(addr);
}

void SRE_47(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_SRE(addr);
}

void PHA_48(BYTE op)
{
    WORD addr = 0;

    handler_PHA(addr);
}

void EOR_49(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_EOR(addr);
}

void LSR_4A(BYTE op)
{
    WORD addr = 0;

    handler_LSR_REG_A(addr);
}

void ASR_4B(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_ASR(addr);
}

void JMP_4C(BYTE op)
{
    WORD addr = absolute_addressing();

    PC = addr;
}

void EOR_4D(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_EOR(addr);
}

void LSR_4E(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_LSR(addr);
}

void SRE_4F(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_SRE(addr);
}

void BVC_50(BYTE op)
{
    WORD addr = relative_addressing();

    handler_BVC(addr);
}

void EOR_51(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_EOR(addr);
}

void KIL_52(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void SRE_53(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_SRE(addr);
}

void DOP_54(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_DOP(addr);
}

void EOR_55(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_EOR(addr);
}

void LSR_56(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_LSR(addr);
}

void SRE_57(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_SRE(addr);
}

void CLI_58(BYTE op)
{
    WORD addr = 0;

    handler_CLI(addr);
}

void EOR_59(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_EOR(addr);
}

void NOP_5A(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_NOP(addr);
}

void SRE_5B(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_SRE(addr);
}

void TOP_5C(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_TOP(addr);
}

void EOR_5D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_EOR(addr);
}

void LSR_5E(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_LSR(addr);
}

void SRE_5F(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_SRE(addr);
}

void RTS_60(BYTE op)
{
    WORD addr = 0;

    handler_RTS(addr);
}

void ADC_61(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_ADC(addr);
}

void KIL_62(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void RRA_63(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_RRA(addr);
}

void DOP_64(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_DOP(addr);
}

void ADC_65(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_ADC(addr);
}

void ROR_66(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_ROR(addr);
}

void RRA_67(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_RRA(addr);
}

void PLA_68(BYTE op)
{
    WORD addr = 0;

    handler_PLA(addr);
}

void ADC_69(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_ADC(addr);
}

void ROR_6A(BYTE op)
{
    WORD addr = 0;

    handler_ROR_REG_A(addr);
}

void ARR_6B(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_ARR(addr);

    PC += 1;
}

void JMP_6C(BYTE op)
{
    WORD addr = indirect_addressing();

    PC = addr;
}

void ADC_6D(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_ADC(addr);
}

void ROR_6E(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_ROR(addr);
}

void RRA_6F(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_RRA(addr);
}

void BVS_70(BYTE op)
{
    WORD addr = relative_addressing();

    handler_BVS(addr);
}

void ADC_71(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_ADC(addr);
}

void KIL_72(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void RRA_73(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_RRA(addr);
}

void DOP_74(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_DOP(addr);
}

void ADC_75(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_ADC(addr);
}

void ROR_76(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_ROR(addr);
}

void RRA_77(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_RRA(addr);
}

void SEI_78(BYTE op)
{
    WORD addr = 0;

    handler_SEI(addr);
}

void ADC_79(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_ADC(addr);
}

void NOP_7A(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_NOP(addr);
}

void RRA_7B(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_RRA(addr);
}

void TOP_7C(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_TOP(addr);
}

void ADC_7D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_ADC(addr);
}

void ROR_7E(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_ROR(addr);
}

void RRA_7F(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_RRA(addr);
}

void DOP_80(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_DOP(addr);
}

void STA_81(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_STA(addr);
}

void DOP_82(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_DOP(addr);
}

void AAX_83(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_AAX(addr);
}

void STY_84(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_STY(addr);
}

void STA_85(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_STA(addr);
}

void STX_86(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_STX(addr);
}

void AAX_87(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_AAX(addr);
}

void DEY_88(BYTE op)
{
    WORD addr = 0;

    handler_DEY(addr);
}

void DOP_89(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_DOP(addr);
}

void TXA_8A(BYTE op)
{
    WORD addr = 0;

    handler_TXA(addr);
}

void STY_8C(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_STY(addr);
}

void XAA_8B(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_XAA(addr);
}

void STA_8D(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_STA(addr);
}

void STX_8E(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_STX(addr);
}

void AAX_8F(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_AAX(addr);
}

void BCC_90(BYTE op)
{
    WORD addr = relative_addressing();

    handler_BCC(addr);
}

void STA_91(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(1);

    handler_STA(addr);
}

void KIL_92(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void AXA_93(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_AXA(addr);
}

void STY_94(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    bus_write(addr, cpu.Y);
}

void STA_95(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_STA(addr);
}

void STX_96(BYTE op)
{
    WORD addr = zero_Y_indexed_addressing();

    handler_STX(addr);
}

void AAX_97(BYTE op)
{
    WORD addr = zero_Y_indexed_addressing();

    handler_AAX(addr);
}

void TYA_98(BYTE op)
{
    WORD addr = 0;

    handler_TYA(addr);
}

void STA_99(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(1);

    handler_STA(addr);
}

void TXS_9A(BYTE op)
{
    WORD addr = 0;

    handler_TXS(addr);
}

void XAS_9B(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_XAS(addr);
}

//TODO: 暂时还没搞懂, 怎么实现的
void SYA_9C(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_SYA(addr);
}

void STA_9D(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_STA(addr);
}

//TODO: 暂时还没搞懂, 怎么实现的
void SXA_9E(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_SXA(addr);
}

void AXA_9F(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_AXA(addr);
}

void LDY_A0(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_LDY(addr);
}

void LDA_A1(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_LDA(addr);
}

void LDX_A2(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_LDX(addr);
}

void LAX_A3(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_LAX(addr);
}

void LDY_A4(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_LDY(addr);
}

void LDA_A5(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_LDA(addr);
}

void LDX_A6(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_LDX(addr);
}

void LAX_A7(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_LAX(addr);
}

void TAY_A8(BYTE op)
{
    WORD addr = 0;

    handler_TAY(addr);
}

void LDA_A9(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_LDA(addr);
}

void TAX_AA(BYTE op)
{
    WORD addr = 0;

    handler_TAX(addr);
}

void ATX_AB(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_ATX(addr);
}

void LDY_AC(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_LDY(addr);
}

void LDA_AD(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_LDA(addr);
}

void LDX_AE(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_LDX(addr);
}

void LAX_AF(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_LAX(addr);
}

void BCS_B0(BYTE op)
{
    WORD addr = relative_addressing();

    handler_BCS(addr);
}

void LDA_B1(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_LDA(addr);
}

void KIL_B2(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void LAX_B3(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(1);

    handler_LAX(addr);
}

void LDY_B4(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_LDY(addr);
}

void LDA_B5(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_LDA(addr);
}

void LDX_B6(BYTE op)
{
    WORD addr = zero_Y_indexed_addressing();

    handler_LDX(addr);
}

void LAX_B7(BYTE op)
{
    WORD addr = zero_Y_indexed_addressing();

    handler_LAX(addr);
}

void CLV_B8(BYTE op)
{
    WORD addr = 0;

    handler_CLV(addr);
}

void LDA_B9(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_LDA(addr);
}

void TSX_BA(BYTE op)
{
    WORD addr = 0;

    handler_TSX(addr);
}

void LAR_BB(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_LAR(addr);
}

void LDY_BC(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_LDY(addr);
}

void LDA_BD(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_LDA(addr);
}

void LDX_BE(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_LDX(addr);
}

void LAX_BF(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_LAX(addr);
}

void CPY_C0(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_CPY(addr);
}

void CMP_C1(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_CMP(addr);
}

void DOP_C2(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_DOP(addr);

    PC += 1;
}

void DCP_C3(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_DCP(addr);
}

void CPY_C4(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_CPY(addr);
}

void CMP_C5(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_CMP(addr);
}

void DEC_C6(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_DEC(addr);
}

void DCP_C7(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_DCP(addr);
}

void INY_C8(BYTE op)
{
    WORD addr = 0;

    handler_INY(addr);
}

void CMP_C9(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_CMP(addr);
}

void DEX_CA(BYTE op)
{
    WORD addr = 0;

    handler_DEX(addr);
}

void AXS_CB(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_AXS(addr);
}

void CPY_CC(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_CPY(addr);
}

void CMP_CD(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_CMP(addr);
}

void DEC_CE(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_DEC(addr);
}

void DCP_CF(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_DCP(addr);
}

void BNE_D0(BYTE op)
{
    WORD addr = relative_addressing();

    handler_BNE(addr);
}

void CMP_D1(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_CMP(addr);
}

void KIL_D2(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void DCP_D3(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_DCP(addr);
}

void DOP_D4(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_DOP(addr);
}

void CMP_D5(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_CMP(addr);
}

void DEC_D6(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_DEC(addr);
}

void DCP_D7(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_DCP(addr);
}

void CLD_D8(BYTE op)
{
    WORD addr = 0;

    handler_CLD(addr);
}

void CMP_D9(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_CMP(addr);
}

void NOP_DA(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_NOP(addr);
}

void DCP_DB(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_DCP(addr);
}

void TOP_DC(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_TOP(addr);
}

void CMP_DD(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_CMP(addr);
}

void DEC_DE(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_DEC(addr);
}

void DCP_DF(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_DCP(addr);
}

void CPX_E0(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_CPX(addr);
}

void SBC_E1(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_SBC(addr);
}

void DOP_E2(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_DOP(addr);
}

void ISC_E3(BYTE op)
{
    WORD addr = indexed_X_indirect_addressing();

    handler_ISC(addr);
}

void CPX_E4(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_CPX(addr);
}

void SBC_E5(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_SBC(addr);
}

void INC_E6(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_INC(addr);
}

void ISC_E7(BYTE op)
{
    WORD addr = zero_absolute_addressing();

    handler_ISC(addr);
}

void INX_E8(BYTE op)
{
    WORD addr = 0;

    handler_INX(addr);
}

void SBC_E9(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_SBC(addr);
}

void NOP_EA(BYTE op)
{
    WORD addr = 0;
    handler_NOP(addr);

    ++PC;
}

void SBC_EB(BYTE op)
{
    WORD addr = immediate_addressing();
    PC += 1;

    handler_SBC(addr);
}

void CPX_EC(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_CPX(addr);
}

void SBC_ED(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_SBC(addr);
}

void INC_EE(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_INC(addr);
}

void ISC_EF(BYTE op)
{
    WORD addr = absolute_addressing();

    handler_ISC(addr);
}

void BEQ_F0(BYTE op)
{
    WORD addr = relative_addressing();

    handler_BEQ(addr);
}

void SBC_F1(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_SBC(addr);
}

void KIL_F2(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_KIL(addr);
}

void ISC_F3(BYTE op)
{
    WORD addr = indirect_Y_indexed_addressing(0);

    handler_ISC(addr);
}

void DOP_F4(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_DOP(addr);
}

void SBC_F5(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_SBC(addr);
}

void INC_F6(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_INC(addr);
}

void ISC_F7(BYTE op)
{
    WORD addr = zero_X_indexed_addressing();

    handler_ISC(addr);
}

void SED_F8(BYTE op)
{
    WORD addr = 0;

    handler_SED(addr);
}

void SBC_F9(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_SBC(addr);
}

void NOP_FA(BYTE op)
{
    WORD addr = immediate_addressing();

    handler_NOP(addr);
}

void ISC_FB(BYTE op)
{
    WORD addr = absolute_Y_indexed_addressing(0);

    handler_ISC(addr);
}

void TOP_FC(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_TOP(addr);
}

void SBC_FD(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_SBC(addr);
}

void INC_FE(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_INC(addr);
}

void ISC_FF(BYTE op)
{
    WORD addr = absolute_X_indexed_addressing(op);

    handler_ISC(addr);
}

static void init_code()
{
    op(00, "BRK", 1, 7, BRK_00)
    op(01, "ORA", 2, 6, ORA_01)
    op(02, "KIL", 1, 0, KIL_02)
    op(03, "SLO", 2, 8, SLO_03)
    op(04, "DOP", 2, 3, DOP_04)
    op(05, "ORA", 2, 3, ORA_05)
    op(06, "ASL", 2, 5, ASL_06)
    op(07, "SLO", 2, 5, SLO_07)
    op(08, "PHP", 1, 3, PHP_08)
    op(09, "ORA", 2, 2, ORA_09)
    op(0A, "ASL", 1, 2, ASL_0A)
    op(0B, "AAC", 2, 2, AAC_0B)
    op(0C, "TOP", 3, 4, NOP_0C)
    op(0D, "ORA", 3, 4, ORA_0D)
    op(0E, "ASL", 3, 6, ASL_0E)
    op(0F, "SLO", 3, 6, SLO_0F)

    op(10, "BPL", 2, 2, BPL_10)
    op(11, "ORA", 2, 5, ORA_11)
    op(12, "KIL", 1, 0, KIL_12)
    op(13, "SLO", 2, 7, SLO_13)
    op(14, "DOP", 2, 4, DOP_14)
    op(15, "ORA", 2, 4, ORA_15)
    op(16, "ASL", 2, 6, ASL_16)
    op(17, "SLO", 2, 6, SLO_17)
    op(18, "CLC", 1, 2, CLC_18)
    op(19, "ORA", 3, 4, ORA_19)
    op(1A, "NOP", 1, 2, NOP_1A)
    op(1B, "SLO", 3, 6, SLO_1B)
    op(1C, "TOP", 3, 4, TOP_1C)
    op(1D, "ORA", 3, 4, ORA_1D)
    op(1E, "ASL", 3, 7, ASL_1E)
    op(1F, "SLO", 3, 7, SLO_1F)

    op(20, "JSR", 3, 6, JSR_20)
    op(21, "AND", 2, 6, AND_21)
    op(22, "KIL", 1, 0, KIL_22)
    op(23, "RLA", 2, 8, RLA_23)
    op(24, "BIT", 2, 3, BIT_24)
    op(25, "AND", 2, 3, AND_25)
    op(26, "ROL", 2, 5, ROL_26)
    op(27, "RLA", 2, 5, RLA_27)
    op(28, "PLP", 1, 4, PLP_28)
    op(29, "AND", 2, 2, AND_29)
    op(2A, "ROL", 1, 2, ROL_2A)
    op(2B, "ROL", 1, 2, AAC_2B)
    op(2C, "BIT", 3, 4, BIT_2C)
    op(2D, "AND", 3, 4, AND_2D)
    op(2E, "ROL", 3, 6, ROL_2E)
    op(2F, "RLA", 3, 6, RLA_2F)

    op(30, "BMI", 2, 2, BMI_30)
    op(31, "AND", 2, 5, AND_31)
    op(32, "KIL", 1, 0, KIL_32)
    op(33, "RLA", 2, 7, RLA_33)
    op(34, "DOP", 2, 4, DOP_34)
    op(35, "AND", 2, 4, AND_35)
    op(36, "ROL", 2, 6, ROL_36)
    op(37, "RLA", 2, 6, RLA_37)
    op(38, "SEC", 1, 2, SEC_38)
    op(39, "AND", 3, 4, AND_39)
    op(3A, "NOP", 1, 2, NOP_3A)
    op(3B, "RLA", 3, 6, RLA_3B)
    op(3C, "TOP", 3, 4, TOP_3C)
    op(3D, "AND", 3, 4, AND_3D)
    op(3E, "ROL", 3, 7, ROL_3E)
    op(3F, "RLA", 3, 7, RLA_3F)

    op(40, "RTI", 1, 6, RTI_40)
    op(41, "EOR", 2, 6, EOR_41)
    op(42, "KIL", 1, 0, KIL_42)
    op(43, "SRE", 2, 8, SRE_43)
    op(44, "DOP", 2, 3, DOP_44)
    op(45, "EOR", 2, 3, EOR_45)
    op(46, "LSR", 2, 5, LSR_46)
    op(47, "SRE", 2, 5, SRE_47)
    op(48, "PHA", 1, 3, PHA_48)
    op(49, "EOR", 2, 2, EOR_49)
    op(4A, "LSR", 1, 2, LSR_4A)
    op(4B, "ASR", 2, 2, ASR_4B)
    op(4C, "JMP", 3, 3, JMP_4C)
    op(4D, "EOR", 3, 4, EOR_4D)
    op(4E, "LSR", 3, 6, LSR_4E)
    op(4F, "SRE", 3, 6, SRE_4F)

    op(50, "BVC", 2, 2, BVC_50)
    op(51, "EOR", 2, 5, EOR_51)
    op(52, "KIL", 1, 0, KIL_52)
    op(53, "SRE", 2, 7, SRE_53)
    op(54, "DOP", 2, 4, DOP_54)
    op(55, "EOR", 2, 4, EOR_55)
    op(56, "LSR", 2, 6, LSR_56)
    op(57, "SRE", 2, 6, SRE_57)
    op(58, "CLI", 1, 2, CLI_58)
    op(59, "EOR", 3, 4, EOR_59)
    op(5A, "NOP", 1, 2, NOP_5A)
    op(5B, "SRE", 3, 6, SRE_5B)
    op(5C, "TOP", 3, 4, TOP_5C)
    op(5D, "EOR", 3, 4, EOR_5D)
    op(5E, "LSR", 3, 7, LSR_5E)
    op(5F, "SRE", 3, 7, SRE_5F)

    op(60, "RTS", 1, 6, RTS_60)
    op(61, "ADC", 2, 6, ADC_61)
    op(62, "KIL", 1, 0, KIL_62)
    op(63, "RRA", 2, 8, RRA_63)
    op(64, "DOP", 2, 3, DOP_64)
    op(65, "ADC", 2, 3, ADC_65)
    op(66, "ROR", 2, 5, ROR_66)
    op(67, "RRA", 2, 5, RRA_67)
    op(68, "PLA", 1, 4, PLA_68)
    op(69, "ADC", 2, 2, ADC_69)
    op(6A, "ROR", 1, 2, ROR_6A)
    op(6B, "ARR", 2, 2, ARR_6B)
    op(6C, "JMP", 3, 5, JMP_6C)
    op(6D, "ADC", 3, 4, ADC_6D)
    op(6E, "ROR", 3, 6, ROR_6E)
    op(6F, "RRA", 3, 6, RRA_6F)

    op(70, "BVS", 2, 2, BVS_70)
    op(71, "ADC", 2, 5, ADC_71)
    op(72, "KIL", 1, 0, KIL_72)
    op(73, "RRA", 2, 7, RRA_73)
    op(74, "DOP", 2, 4, DOP_74)
    op(75, "ADC", 2, 4, ADC_75)
    op(76, "ROR", 3, 6, ROR_76)
    op(77, "RRA", 2, 6, RRA_77)
    op(78, "SEI", 1, 2, SEI_78)
    op(79, "ADC", 3, 4, ADC_79)
    op(7A, "NOP", 1, 2, NOP_7A)
    op(7B, "RRA", 3, 6, RRA_7B)
    op(7C, "TOP", 3, 4, TOP_7C)
    op(7D, "ADC", 3, 4, ADC_7D)
    op(7E, "ROR", 3, 7, ROR_7E)
    op(7F, "RRA", 3, 7, RRA_7F)

    op(80, "DOP", 2, 2, DOP_80)
    op(81, "STA", 2, 6, STA_81)
    op(82, "DOP", 2, 2, DOP_82)
    op(83, "SAX", 2, 6, AAX_83)
    op(84, "STY", 2, 3, STY_84)
    op(85, "STA", 2, 3, STA_85)
    op(86, "STX", 2, 3, STX_86)
    op(87, "SAX", 2, 3, AAX_87)
    op(88, "DEY", 1, 2, DEY_88)
    op(89, "DOP", 2, 2, DOP_89)
    op(8A, "TXA", 1, 2, TXA_8A)
    op(8B, "XAA", 2, 2, XAA_8B)
    op(8C, "STY", 3, 4, STY_8C)
    op(8D, "STA", 3, 4, STA_8D)
    op(8E, "STX", 3, 4, STX_8E)
    op(8F, "SAX", 3, 4, AAX_8F)

    op(90, "BCC", 2, 2, BCC_90)
    op(91, "STA", 2, 6, STA_91)
    op(92, "KIL", 1, 0, KIL_92)
    op(93, "AXA", 2, 6, AXA_93)
    op(94, "STY", 2, 4, STY_94)
    op(95, "STA", 2, 4, STA_95)
    op(96, "STX", 2, 4, STX_96)
    op(97, "SAX", 2, 4, AAX_97)
    op(98, "TYA", 1, 2, TYA_98)
    op(99, "STA", 3, 5, STA_99)
    op(9A, "TXS", 1, 2, TXS_9A)
    op(9B, "XAS", 3, 5, XAS_9B)
    op(9C, "SYA", 3, 5, SYA_9C)
    op(9D, "STA", 3, 5, STA_9D)
    op(9E, "SXA", 3, 5, SXA_9E)
    op(9F, "AXA", 3, 5, AXA_9F)

    op(A0, "LDY", 2, 2, LDY_A0)
    op(A1, "LDA", 2, 6, LDA_A1)
    op(A2, "LDX", 2, 2, LDX_A2)
    op(A3, "LAX", 2, 6, LAX_A3)
    op(A4, "LDY", 2, 3, LDY_A4)
    op(A5, "LDA", 2, 3, LDA_A5)
    op(A6, "LDX", 2, 3, LDX_A6)
    op(A7, "LAX", 2, 3, LAX_A7)
    op(A8, "TAY", 1, 2, TAY_A8)
    op(A9, "LDA", 2, 2, LDA_A9)
    op(AA, "TAX", 1, 2, TAX_AA)
    op(AB, "ATX", 2, 2, ATX_AB)
    op(AC, "LDY", 3, 4, LDY_AC)
    op(AD, "LDA", 3, 4, LDA_AD)
    op(AE, "LDX", 3, 4, LDX_AE)
    op(AF, "LAX", 3, 4, LAX_AF)

    op(B0, "BCS", 2, 2, BCS_B0)
    op(B1, "LDA", 2, 5, LDA_B1)
    op(B2, "KIL", 1, 0, KIL_B2)
    op(B3, "LAX", 2, 5, LAX_B3)
    op(B4, "LDY", 2, 4, LDY_B4)
    op(B5, "LDA", 2, 4, LDA_B5)
    op(B6, "LDX", 2, 4, LDX_B6)
    op(B7, "LAX", 2, 4, LAX_B7)
    op(B8, "CLV", 1, 2, CLV_B8)
    op(B9, "LDA", 3, 4, LDA_B9)
    op(BA, "TSX", 1, 2, TSX_BA)
    op(BB, "LAR", 3, 4, LAR_BB)
    op(BC, "LDY", 3, 4, LDY_BC)
    op(BD, "LDA", 3, 4, LDA_BD)
    op(BE, "LDX", 3, 4, LDX_BE)
    op(BF, "LAX", 3, 4, LAX_BF)

    op(C0, "CPY", 2, 2, CPY_C0)
    op(C1, "CMP", 2, 6, CMP_C1)
    op(C2, "DOP", 2, 2, DOP_C2)
    op(C3, "DCP", 2, 8, DCP_C3)
    op(C4, "CPY", 2, 3, CPY_C4)
    op(C5, "CMP", 2, 3, CMP_C5)
    op(C6, "DEC", 2, 5, DEC_C6)
    op(C7, "DCP", 2, 5, DCP_C7)
    op(C8, "INY", 1, 2, INY_C8)
    op(C9, "CMP", 2, 2, CMP_C9)
    op(CA, "DEX", 1, 2, DEX_CA)
    op(CB, "AXS", 2, 2, AXS_CB)
    op(CC, "CPY", 3, 4, CPY_CC)
    op(CD, "CMP", 3, 4, CMP_CD)
    op(CE, "DEC", 3, 6, DEC_CE)
    op(CF, "DCP", 3, 6, DCP_CF)

    op(D0, "BNE", 2, 2, BNE_D0)
    op(D1, "CMP", 2, 5, CMP_D1)
    op(D2, "KIL", 1, 0, KIL_D2)
    op(D3, "DCP", 2, 7, DCP_D3)
    op(D4, "DOP", 2, 4, DOP_D4)
    op(D5, "CMP", 2, 4, CMP_D5)
    op(D6, "DEC", 2, 6, DEC_D6)
    op(D7, "DCP", 2, 6, DCP_D7)
    op(D8, "CLD", 1, 2, CLD_D8)
    op(D9, "CMP", 3, 4, CMP_D9)
    op(DA, "NOP", 1, 2, NOP_DA)
    op(DB, "DCP", 1, 6, DCP_DB)
    op(DC, "TOP", 3, 4, TOP_DC)
    op(DD, "CMP", 3, 4, CMP_DD)
    op(DE, "DEC", 3, 7, DEC_DE)
    op(DF, "DCP", 3, 7, DCP_DF)

    op(E0, "CPX", 2, 2, CPX_E0)
    op(E1, "SBC", 2, 6, SBC_E1)
    op(E2, "DOP", 2, 6, DOP_E2)
    op(E3, "ISC", 2, 8, ISC_E3)
    op(E4, "CPX", 2, 3, CPX_E4)
    op(E5, "SBC", 2, 3, SBC_E5)
    op(E6, "INC", 2, 5, INC_E6)
    op(E7, "ISC", 2, 5, ISC_E7)
    op(E8, "INX", 1, 2, INX_E8)
    op(E9, "SBC", 2, 2, SBC_E9)
    op(EA, "NOP", 1, 2, NOP_EA)
    op(EB, "SBC", 2, 2, SBC_EB)
    op(EC, "CPX", 3, 4, CPX_EC)
    op(ED, "SBC", 3, 4, SBC_ED)
    op(EE, "INC", 3, 6, INC_EE)
    op(EF, "ISC", 3, 6, ISC_EF)

    op(F0, "BEQ", 2, 2, BEQ_F0)
    op(F1, "SBC", 2, 5, SBC_F1)
    op(F2, "KIL", 1, 0, KIL_F2)
    op(F3, "ISC", 2, 7, ISC_F3)
    op(F4, "DOP", 2, 4, DOP_F4)
    op(F5, "SBC", 2, 4, SBC_F5)
    op(F6, "INC", 2, 6, INC_F6)
    op(F7, "ISC", 2, 6, ISC_F7)
    op(F8, "SED", 1, 2, SED_F8)
    op(F9, "SBC", 3, 4, SBC_F9)
    op(FA, "NOP", 1, 2, NOP_FA)
    op(FB, "ISC", 3, 6, ISC_FB)
    op(FC, "TOP", 3, 4, TOP_FC)
    op(FD, "SBC", 3, 4, SBC_FD)
    op(FE, "INC", 3, 7, INC_FE)
    op(FF, "ISC", 3, 7, ISC_FF)
}

static void init_reg()
{
    cpu.IP = bus_read(0xFFFC) | (bus_read(0xFFFD) << 8);
    cpu.SP = 0xFD;
    cpu.X = 0;
    cpu.Y = 0;
    cpu.P = 0x24;
    cpu.A = 0;
    cpu.is_lock = 0;
    cpu.cycle = 8;
}

// 读取复位向量的函数
uint16_t get_reset_vector()
{
    uint8_t low_byte = bus_read(0xFFFC);
    uint8_t high_byte = bus_read(0xFFFD);

    return (high_byte << 8) | low_byte;
}

// 读取 NMI 向量的函数
uint16_t get_nmi_vector()
{
    uint8_t low_byte = bus_read(0xFFFA);
    uint8_t high_byte = bus_read(0xFFFB);

    return (high_byte << 8) | low_byte;
}

// 读取 IRQ 向量的函数
uint16_t get_irq_vector()
{
    uint8_t low_byte = bus_read(0xFFFE);
    uint8_t high_byte = bus_read(0xFFFF);

    return (high_byte << 8) | low_byte;
}

// NMI 中断处理函数
void handle_nmi()
{
    uint16_t nmi_address = get_nmi_vector();
    // 将 CPU 程序计数器设置为 NMI 向量地址
    cpu.IP = nmi_address;
    // 处理 NMI 中断的其他操作，如压栈、更新状态等
}

// IRQ 中断处理函数
void handle_irq()
{
    if (!(cpu.P & 0x04)) { // 检查 I 标志是否被清除
        uint16_t irq_address = get_irq_vector();
        // 将 CPU 程序计数器设置为 IRQ 向量地址
        cpu.IP = irq_address;
        // 处理 IRQ 中断的其他操作，如压栈、更新状态等
    }
}

// Reset 处理函数
void handle_reset()
{
    uint16_t reset_address = get_reset_vector();
    cpu.IP = reset_address;
}

WORD get_start_address()
{
    BYTE addr1 = bus_read(0xFFFC);
    BYTE addr2 = bus_read(0xFFFD);

    return addr2 << 8 | addr1;
}

void cpu_init()
{
    init_code();

    init_reg();
}

void cpu_reset()
{
    init_reg();
}

void cpu_interrupt_NMI()
{
    // 将当前的 IP 压栈
    push((cpu.IP >> 8) & 0xFF);  // 高字节
    push(cpu.IP & 0xFF);  // 低字节

    // 将 P 状态寄存器压栈（清除 B 标志）
    push(cpu.P & ~0x30);  // 清除 B 和未使用标志（位 4 和位 5）

    // 设置禁用中断标志
    cpu.P |= 0x04;

    // 取出 NMI 向量地址
    WORD nmi_vector = bus_read(0xFFFA) | (bus_read(0xFFFB) << 8);

    // 加载新的程序计数器地址
    cpu.IP = nmi_vector;

    // 清除NMI 标志
    cpu.interrupt &= 0xFE;

    // 更新 CPU 周期
    cpu.cycle += 7;  // NMI 处理大约需要 7 个周期
}

BYTE cpu_read_byte(WORD address)
{
    return cpu.ram[address];
}

void cpu_write_byte(WORD address, BYTE data)
{
    cpu.ram[address] = data;
}

WORD cpu_read_word(WORD address)
{
    //小端 低字节在前、高字节在后
    BYTE low_byte = cpu.ram[address];
    BYTE high_byte = cpu.ram[address + 1];

    WORD data = low_byte | ( high_byte << 8);

    return data;
}

void cpu_write_word(WORD address, WORD data)
{
    BYTE *ptr = (BYTE*)&data;

    //小端 低字节在前、高字节在后
    cpu.ram[address] = ptr[1];
    cpu.ram[address + 1] = ptr[0];
}

void set_nmi()
{
    cpu.interrupt |= 0x1;
}

BYTE step_cpu()
{
    // 初始的周期数
    int initial_cycles = cpu.cycle;

    //触发NMI 中断, 直接执行读取中断向量操作
    if (cpu.interrupt & 0x1) {
        cpu_interrupt_NMI();
        return cpu.cycle - initial_cycles;
    }

    BYTE opcode = bus_read(PC);
    //do_disassemble(PC, opcode);

    // 执行操作码对应的操作函数
    code_maps[opcode].op_func(opcode);

    cpu.cycle += code_maps[opcode].cycle;

    // 计算指令消耗的实际周期数
    int cycles_consumed = cpu.cycle - initial_cycles;

    return cycles_consumed;
}
