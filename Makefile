CC = gcc

CFLAGS = -Wall -g -std=c99

TARGET = fc

SRCDIR = ./
DEST_DIR = example

SRCS = $(wildcard $(SRCDIR)/*.c)

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(IDIR) $(LDIR) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(IDIR) -c -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
