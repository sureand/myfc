CC = gcc

# 定义编译选项
CFLAGS = -Wall -g -std=c99 -I"SDL2/include" -Dmain=SDL_main

# 定义链接选项，添加 -static 以静态链接依赖项
LDFLAGS = -L"SDL2/lib" -lSDL2 -lSDL2main

TARGET = fc.exe

SRCDIR = ./
DEST_DIR = build

# 指定生成的文件存放的目录
OBJDIR = $(DEST_DIR)

# 使用通配符获取所有源文件
SRCS = $(wildcard $(SRCDIR)/*.c)

# 生成对象文件列表，并指定在哪个目录下生成
OBJS = $(addprefix $(OBJDIR)/, $(SRCS:.c=.o))

# 指定最终目标文件的存放位置
TARGET_PATH = $(DEST_DIR)/$(TARGET)

all: $(TARGET_PATH)

# 确保目标文件的目录存在
$(TARGET_PATH): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

# 编译目标，生成可执行文件
$(TARGET_PATH): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# 编译单个源文件，生成对象文件
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# 清理编译生成的文件
clean:
	rm -f $(OBJDIR)/*.o

.PHONY: all clean