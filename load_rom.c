#include "common.h"
#include "load_rom.h"
#include "string.h"
#include "disasm.h"
#include "cpu.h"
#include <assert.h>

ROM_HEADER *make_header()
{
    ROM_HEADER *header = (ROM_HEADER *)calloc(1, sizeof(ROM_HEADER));
    if(!header) return NULL;

    return header;
}

ROM *make_rom()
{
    ROM *rom = (ROM *)calloc(1, sizeof(ROM));
    if(!rom) return NULL;

    return rom;
}

ROM_HEADER *parse_header(FILE *fp)
{
    assert(fp);

    BYTE header[HEADER_LEN] = {0};
    size_t len = fread(header, 1, HEADER_LEN, fp);
    if(len != HEADER_LEN) {
        fprintf(stderr, "read game header error, read len:%I64u\n", len);
        return NULL;
    }

    //校验头部是否是NES
    if(memcmp(header, "NES", 3)) {
        fprintf(stderr, "error, is not an nes file!\n");
        return NULL;
    }

    if(header[3] != 0x1A) {
        fprintf(stderr, "error, is not an iNes file!\n");
        return NULL;
    }

    ROM_HEADER *rom_header = make_header();
    rom_header->version = 1;
    rom_header->prg_rom_count = header[4];

    if(rom_header->prg_rom_count == 0) {
        fprintf(stderr, "error, is not an nes file!\n");
        return NULL;
    }

    rom_header->chr_rom_count = header[5];

    rom_header->flag1 = header[6];

    //高四位第一位判断是否是1, 是则包含Trainer
    rom_header->type = rom_header->flag1 & (1 << 4);
    rom_header->flag2 = header[7];

    if ((header[7] & 0x0C) == 0x08) {
        fprintf(stderr, "error, Nes 2.0 is unsupported!\n");
        return NULL;
    }

    rom_header->mapper_number = (header[7] & 0xF0) | (header[6] >> 4);

    return rom_header;
}

ROM *parse_rom(FILE *fp)
{
    ROM_HEADER *header = parse_header(fp);
    if(!header) return NULL;

    ROM *rom = make_rom();
    if(!rom) {
        FREE(header);
        return NULL;
    }

    //假如包含Trainer数据, 先跳过
    if(header->type == 1) {
        fprintf(stdout, "skip trainer data!\n");
        fseek(fp, 512, SEEK_CUR);
    }

    size_t prg_rom_size = header->prg_rom_count * PRG_ROM_PAGE_SIZE;
    size_t chr_rom_size = header->chr_rom_count * CHR_ROM_PAGE_SIZE;

    size_t total_rom_size = prg_rom_size + chr_rom_size;
    rom->body = (BYTE*)calloc(total_rom_size + 1, 1);
    if(!rom->body) {
        fprintf(stderr, "error, malloc failed!\n");
        goto ERR_EXIT;
    }

    if (fread(rom->body, total_rom_size, 1, fp) != 1) {
        fprintf(stderr, "read nes rom error!\n");
        goto ERR_EXIT;
    }

    rom->prg_rom = rom->body;
    rom->chr_rom = rom->body + prg_rom_size;
    rom->header = header;

    return rom;

ERR_EXIT:
    FREE(header);
    FREE(rom);
    return NULL;
}

void show_header_info(ROM_HEADER *header)
{
    size_t prg_size = (header->prg_rom_count * PRG_ROM_PAGE_SIZE) >> 10;
    size_t chr_size = (header->chr_rom_count * CHR_ROM_PAGE_SIZE) >> 10;

    printf("PRG:%I64uk, CHR:%I64uK\n", prg_size, chr_size);
}

ROM *load_rom(const char *path)
{
    assert(path);

    FILE *fp = fopen(path, "rb");
    if(!fp) {
        fprintf(stderr, "error cannot open game file: %s!\n", path);
        return NULL;
    }

    ROM *rom = parse_rom(fp);
    fclose(fp);

    if(!rom) {
        fprintf(stderr, "parse rom %s failed!\n", path);
        exit(-1);
    }

    return rom;
}

ROM *get_current_rom()
{
    return current_rom;
}

void set_current_rom(ROM *rom)
{
    current_rom = rom;
}