#ifndef __MEMORY_HEADER
#define __MEMORY_HEADER
#include "common.h"

BYTE bus_read(WORD address);
void bus_write(WORD address, BYTE data);

#endif