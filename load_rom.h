#ifndef __HEADER__LOAD_ROM__
#define __HEADER__LOAD_ROM__
#include "common.h"

ROM *make_rom();
void FREE(void *p);
ROM_HEADER *parse_header(FILE *fp);
ROM *parse_rom(FILE *fp);
ROM *load_rom(const char *path);
int load_data(const char *path);

#endif