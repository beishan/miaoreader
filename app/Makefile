# 妙阅 E-Reader PC 仿真 Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -DPC_SIMULATION -I. -I./shared/compat $(shell freetype-config --cflags)
LDFLAGS = -lSDL2 -lz $(shell freetype-config --libs)

TARGET = reader_sim

# 源文件
SOURCES = sim_main.c \
          renderer_sdl.c \
          shared/ui/page_mgr.c \
          shared/ui/widget.c \
          shared/ui/status_bar.c \
          shared/ui/page_home.c \
          shared/ui/page_bookshelf.c \
          shared/ui/page_reader.c \
          shared/ui/page_settings.c \
          shared/storage/reading_progress.c \
          shared/storage/book_storage.c \
          shared/mock/mock_rtc.c \
          shared/mock/mock_weather.c \
          shared/mock/mock_books.c \
          shared/mock/mock_wifi.c \
          shared/engine/book_parser.c \
          shared/engine/typesetter.c

OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
