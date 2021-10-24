#include "common.h"
#include "load_rom.h"
#include "disasm.h"
#include <assert.h>

#define FREE(p) \
do { \
    if(!(p)) break;\
    free((p)); \
    (p) = NULL; \
}while(0); \

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

ROM_HEADER *parse_header(const FILE *fp)
{
    assert(fp);

    BYTE header[HEADER_LEN] = {0};
    size_t len = fread(header, 1, HEADER_LEN, fp);
    if(len != HEADER_LEN) {
        fprintf(stderr, "read game header error, read len:%lu\n", len);
        return NULL;
    }

    //校验头部是否是NES
    if(memcmp(header, "NES", 3)) {
        fprintf(stderr, "error, is not an nes file!\n");
        return NULL;
    }
    if(header[3] != 0x1A) {
        fprintf(stderr, "error, is not an nes file!\n");
        return NULL;
    }

    ROM_HEADER *rom_header = make_header();
    rom_header->version = 1;
    rom_header->prg_rom_count = (BYTE)header[4];
    rom_header->chr_rom_count = (BYTE)header[5];
    rom_header->flag1 = (BYTE)header[6];

    //高四位第一位判断是否是1, 是则包含Trainer
    rom_header->type = rom_header->flag1 & (1 << 4);
    rom_header->flag2 = (BYTE)header[7];

    return rom_header;
}

ROM *parse_rom(const FILE *fp)
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

    size_t len1 = header->prg_rom_count * 0x4000;
    size_t len2 = header->chr_rom_count * 0x2000;

    rom->body = (BYTE*)calloc(len1 + len2, 1);
    if(!rom->body) {
        fprintf(stderr, "error, malloc failed!\n");
        goto ERR_EXIT;
    }

    if(!fread(rom->body, len1 + len2, 1, fp)) {
        fprintf(stderr, "read nes rom error!\n");
        goto ERR_EXIT;
    }

    rom->prg_rom = rom->body;
    rom->chr_rom = rom->body + len1;
    rom->header = header;

    return rom;

ERR_EXIT:
    FREE(header);
    FREE(rom);
    return NULL;
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
        fprintf(stderr, "parse rom failed!\n", path);
        return NULL;
    }

    return rom;
}

int load_data(const char *path)
{
    ROM *rom = load_rom(path);
    if(!rom) return -1;

    ROM_HEADER *header = rom->header;

    size_t prg_size = (header->prg_rom_count * 0x4000) >> 10;
    size_t chr_size = (header->prg_rom_count * 0x2000) >> 10;

    printf("PRG:%dk, CHR:%dK\n", prg_size, chr_size);

    show_code(rom);

    FREE(rom->header);
    FREE(rom);

    return 0;
}