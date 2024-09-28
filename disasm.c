#include "disasm.h"
#include "memory.h"
#include "cpu.h"

void display(BYTE *data, size_t count)
{
    size_t c = 0;
    for(c = 0; c < count; ++c){
        printf("%02X", data[c]);
        if((c + 1) % 32 == 0) { printf("\n"); continue; }
        if((c + 1) % 4 == 0) printf(" ");
    }
}

void parse_code()
{
    cpu.P = 0x24;
    cpu.cycle = 7;

    WORD addr = 0xC000;
    PC = 0xC000;

    size_t c = 0;

    while(addr) {

        BYTE code = bus_read(addr);
        if(!code_maps[code].op_name) {
            printf("addr:%04X, code:%02X \n", addr, code);
            break;
        }

        printf("%04X %02X A:%02X X:%02X Y:%02X P:%02X SP:%02X  PPU:  %d, %d CYC:%I64u\n", \
               PC, code, cpu.A, cpu.X, cpu.Y, cpu.P, cpu.SP, ppu.ppustatus, ppu.ppuctrl, cpu.cycle);

        code_maps[code].op_func(code);
        cpu.cycle += code_maps[code].cycle;

        addr = PC;

        ++c;
    }

}

void print_status_flags(char *buffer, uint8_t P)
{
    buffer[0] = (P & 0x80) ? 'N' : 'n'; // Negative Flag
    buffer[1] = (P & 0x40) ? 'V' : 'v'; // Overflow Flag
    buffer[2] = '-';                    // Unused Flag (always 1)
    buffer[3] = '-';
    buffer[4] = (P & 0x08) ? 'D' : 'd'; // Decimal Mode
    buffer[5] = (P & 0x04) ? 'I' : 'i'; // Interrupt Disable
    buffer[6] = (P & 0x02) ? 'Z' : 'z'; // Zero Flag
    buffer[7] = (P & 0x01) ? 'C' : 'c'; // Carry Flag
    buffer[8] = '\0';                   // Null terminator
}

void do_disassemble(WORD addr, BYTE opcode)
{
    char status_flags[9] = {0}; // 用于存储状态寄存器标志位的字符串
    print_status_flags(status_flags, cpu.P);

    printf("%04X  ", addr);

    BYTE operand_length = code_maps[opcode].op_len;
    char operand_buffer[16] = {0};
    for (int i = 0; i < operand_length; i++) {
        sprintf(operand_buffer + i * 3, "%02X ", bus_read(addr + i));
    }

    // 10个字符对齐
    printf("%-10s", operand_buffer);

    printf("  %-4s", code_maps[opcode].op_name);

    printf("    A:%02X X:%02X Y:%02X S:%02X P:%s V:%-4d   H:%-4d Fr:%d Cycle:%d\n", cpu.A, cpu.X, cpu.Y, cpu.SP, status_flags, ppu.scanline, ppu.cycle, ppu.frame_count, cpu.cycle);
}

void disassemble()
{
    cpu.P = 0x24;

    WORD addr = PC;
    while (addr) {

        BYTE opcode = bus_read(addr);

        do_disassemble(addr, opcode);

        code_maps[opcode].op_func(opcode);
        cpu.cycle += code_maps[opcode].cycle;

        addr = PC;
    }
}

void display_IRQ()
{
    BYTE addr1 = bus_read(0xFFFE);
    BYTE addr2 = bus_read(0xFFFF);

    WORD addr = addr2 << 8 | addr1;

    addr1 = bus_read(0xFFFC);
    addr2 = bus_read(0xFFFD);

    addr = addr2 << 8 | addr1;

    BYTE code = bus_read(addr);
    (void)code;

    addr1 = bus_read(0xFFFA);
    addr2 = bus_read(0xFFFB);

    addr = addr2 << 8 | addr1;

    /* nestest.nes 的NMI 地址 是 0xC5AF*/
    printf("NMI:%04X\n", addr);
}

void show_code()
{
    disassemble();

    // 显示部分IRQ 指令
    display_IRQ();
}