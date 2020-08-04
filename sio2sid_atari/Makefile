# Makefile
CL ?= cl65
MACH ?= atari
TARGET ?= bin/sio2sid
TARGETFILENAME ?= $(TARGET).com
EMULATOR ?= atari800
ATRFOLDER ?= ~/ATARI/ATR
ROM ?= ~/ATARI/ATARIXL.ROM
ROMSWITCH ?= -xlxe_rom
MACHINE ?= xe
DEFINES ?= RETROBIT
ATARICPU ?= atarixl

ifeq ($(ATARICPU),atari)
	ROM = ~/ATARI/ATARIOSB.ROM
	ROMSWITCH = -osb_rom
	MACHINE =
	DEFINES += -DATARI800 \
	
endif

EMUSWITCHES = \
	-win-width 800 \
	-win-height 600 \
	$(ROMSWITCH) $(ROM) \
	-$(MACHINE) \
	-scanlines 50 \
	-scanlinesint \
	$(TARGETFILENAME)

C_SOURCES = \
	src/sid2pokey.c \

OPTFLAGS = -Osir

ASM_SOURCES = \
	asm/sp-common.asm \

LDCONFIG = \
#	-C src/atari-boot-load.cfg


all: version $(TARGET)

$(TARGET):
	$(CL) $(OPTFLAGS) -t $(MACH) -o $(TARGETFILENAME) $(LDCONFIG) -Iinc -D$(DEFINES) $(C_SOURCES) $(ASM_SOURCES)

build: $(TARGET)
	$(EMULATOR) $(EMUSWITCHES)

atr: $(TARGET)
	cp $(TARGETFILENAME) $(ATRFOLDER)/build/mypdos-406.ar0
	dir2atr -a -b MyPicoDos406 $(ATRFOLDER)/$(TARGET).atr $(ATRFOLDER)/build

boot: $(TARGET)
	$(CL) $(OPTFLAGS) -t $(MACH) --mapfile $(TARGET).map -o $(TARGETFILENAME).atr -C $(LDCONFIG) -Iinc -D$(DEFINES) $(C_SOURCES) $(ASM_SOURCES)

	
version: $(TARGET)
	$(CL) --version
	
clean:
	rm $(TARGETFILENAME)
