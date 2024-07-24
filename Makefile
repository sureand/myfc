CC = gcc

# 定义编译选项
CFLAGS = -Wall -g -std=c99 -I"SDL2/include"

# 定义链接选项
LDFLAGS = -L"SDL2/lib" -lSDL2 -lSDL2main

TARGET = fc

SRCDIR = ./
DEST_DIR = example

# 使用通配符获取所有源文件
SRCS = $(wildcard $(SRCDIR)/*.c)

# 生成对象文件列表
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean