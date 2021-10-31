CFLAGS=-Wall -Werror -Wno-strict-aliasing -std=gnu11 -g -I -O0
OBJS=cpu.o load_rom.o memory.o disasm.o
LIBS=-lm

fc: load_rom.h disasm.h memory.h cpu.h main.o $(OBJS)
	cc -o $@ main.o $(OBJS) $(CFLAGS) $(LIBS)

$(OBJS) main.o: load_rom.h

clean:
	rm *.o fc
