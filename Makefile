TARGET    := exword_mp3
MODNAME   := EXMP3
APPTITLE  := MP3/WAV Player Walkman
APPID     := EXMP3

SRCS_C    := src/main.c $(wildcard src/libc/*.c)
SRCS_S    := $(wildcard src/libc/*.s)
OBJS      := $(SRCS_C:.c=.o) $(SRCS_S:.s=.o)

CC        := sh-elf-gcc
OBJCOPY   := sh-elf-objcopy
LIBEXWORD := libexword

CFLAGS    := -O2 -m4-nofpu -I. -I$(DEVKITPRO)/libdataplus/include -Isrc/libc/include
LDFLAGS   := -L$(DEVKITPRO)/libdataplus/lib -lgraphics -lc

all: ja_build

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

ja_build: $(OBJS)
	mkdir -p build/ja
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o build/ja/$(TARGET).elf
	$(OBJCOPY) -O binary build/ja/$(TARGET).elf build/ja/$(TARGET).bin
	cp html/ja/menu.html build/ja/
	$(LIBEXWORD) pack build/ja/ $(APPID) "$(APPTITLE)"

clean:
	rm -rf build $(OBJS)
