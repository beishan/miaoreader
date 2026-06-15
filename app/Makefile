# 妙阅 E-Reader PC 仿真 Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -DPC_SIMULATION
LDFLAGS = -lSDL2

TARGET = reader_sim
SOURCES = sim_main.c renderer_sdl.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean run install-deps

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

# macOS 安装依赖
install-deps-mac:
	brew install sdl2

# Ubuntu/Debian 安装依赖
install-deps-linux:
	sudo apt-get update
	sudo apt-get install -y libsdl2-dev

# Windows MSYS2 安装依赖
install-deps-windows:
	pacman -S mingw-w64-x86_64-SDL2
