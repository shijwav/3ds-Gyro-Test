ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO, for example: export DEVKITPRO=/opt/devkitpro)
endif

DEVKITARM  ?= $(DEVKITPRO)/devkitARM
LIBCTRU    := $(DEVKITPRO)/libctru
PORTLIBS   := $(DEVKITPRO)/portlibs/3ds

NAME       := gyro_test
TITLE      := Snowball Gyro Test
AUTHOR     := Snowballers
DESC       := Gyroscope test

CC         := $(DEVKITARM)/bin/arm-none-eabi-gcc
ARCH       := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS     := -g -Wall -O2 -mword-relocations -ffunction-sections $(ARCH) -D__3DS__ \
              -I$(LIBCTRU)/include -I$(PORTLIBS)/include
LDFLAGS    := -specs=3dsx.specs -g $(ARCH) -L$(LIBCTRU)/lib -L$(PORTLIBS)/lib
LIBS       := -lcitro2d -lcitro3d -lctru -lm

SMDHTOOL   := $(DEVKITPRO)/tools/bin/smdhtool
DSXTOOL    := $(DEVKITPRO)/tools/bin/3dsxtool

all: $(NAME).3dsx

$(NAME).elf: source/main.c
	$(CC) $(CFLAGS) source/main.c $(LDFLAGS) $(LIBS) -o $@

$(NAME).smdh:
	$(SMDHTOOL) --create "$(TITLE)" "$(DESC)" "$(AUTHOR)" $(LIBCTRU)/default_icon.png $@

$(NAME).3dsx: $(NAME).elf $(NAME).smdh
	$(DSXTOOL) $(NAME).elf $@ --smdh=$(NAME).smdh

clean:
	rm -f $(NAME).elf $(NAME).smdh $(NAME).3dsx

.PHONY: all clean
