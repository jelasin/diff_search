CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
STATIC_CFLAGS = -Wall -Wextra -std=c99 -O2 -static
INCLUDES = -I.
SRCDIR = .
LIBDIR = lib
TARGET = md5_scanner
STATIC_TARGET = md5_scanner_static

# Source files
MAIN_SRC = main.c
LIB_SRCS = $(LIBDIR)/calc_md5/calc_md5.c \
           $(LIBDIR)/list_file/list_file.c \
           $(LIBDIR)/cJSON/cJSON.c \
           $(LIBDIR)/json_diff/json_diff.c

# Object files
MAIN_OBJ = $(MAIN_SRC:.c=.o)
LIB_OBJS = $(LIB_SRCS:.c=.o)
ALL_OBJS = $(MAIN_OBJ) $(LIB_OBJS)

# Default target
all: $(TARGET)

# 构建GUI查看器
diff-ui:
	@echo "构建GUI查看器..."
	$(MAKE) -C diff-ui

# 构建所有组件
build-all: $(TARGET) diff-ui

# 构建所有静态版本（仅主程序）
build-all-static: $(STATIC_TARGET)

# Build the main executable
$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Build the static executable
static: $(STATIC_TARGET)

$(STATIC_TARGET): $(ALL_OBJS)
	$(CC) $(STATIC_CFLAGS) $(INCLUDES) -o $@ $^

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(ALL_OBJS) $(TARGET) $(STATIC_TARGET)
	$(MAKE) -C diff-ui clean

# Install to system
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	@if [ -f diff-ui/diff-viewer ]; then \
		sudo cp diff-ui/diff-viewer /usr/local/bin/; \
		echo "已安装 diff-viewer 到 /usr/local/bin/"; \
	fi

# Help target
help:
	@echo "可用的目标:"
	@echo "  all           - 构建主程序（默认）"
	@echo "  static        - 构建主程序静态版本"
	@echo "  diff-ui       - 构建GUI查看器"
	@echo "  build-all     - 构建所有组件"
	@echo "  build-all-static - 构建主程序静态版本"
	@echo "  clean         - 清理所有生成文件"
	@echo "  install       - 安装程序到系统"
	@echo "  help          - 显示此帮助信息"

.PHONY: all static clean help diff-ui build-all build-all-static install
