# Makefile for Polaric Tracker

# Target file name (without extension).
TARGET = firmware

# Output format for .hex files, can be [srec|ihex]. Note that we have
# a special .img target for binary output.
FORMAT = ihex

# MCU name and clock speed
MCU = at90usb1287
F_CPU = 8000000

# Define programs.
SHELL = sh
AR = ar
COMPILE = avr-gcc
ASSEMBLE = avr-gcc -x assembler-with-cpp
REMOVE = rm -f
COPY = cp
MOVE = mv
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
HEXSIZE = @avr-size --target=$(FORMAT) $(TARGET).hex
ELFSIZE = @avr-size $(TARGET).elf
LISP = clisp

# LUFA definitions
USB_PATH = LUFA/Drivers/USB
USB_CFLAGS = \
	-I$(USB_PATH) -DF_CLOCK=$(F_CPU)UL -DUSE_NONSTANDARD_DESCRIPTOR_NAMES \
        -DNO_STREAM_CALLBACKS -DUSB_DEVICE_ONLY \
	-DUSE_STATIC_OPTIONS="(USB_DEVICE_OPT_FULLSPEED | USB_OPT_REG_ENABLED | USB_OPT_AUTO_PLL)"
USB_SRC = usb_descriptors.c usb.c \
          $(USB_PATH)/LowLevel/LowLevel.c $(USB_PATH)/HighLevel/USBTask.c \
          $(USB_PATH)/HighLevel/USBInterrupt.c $(USB_PATH)/HighLevel/Events.c \
          $(USB_PATH)/LowLevel/DevChapter9.c $(USB_PATH)/LowLevel/Endpoint.c \
          $(USB_PATH)/HighLevel/StdDescriptors.c 
USB_INCLUDE = -I$(USB_PATH)

# List of C source files which should be preprocessed
PSRC = commands.c

# List C source files here.
SRC = main.c config.c ui.c kernel/kernel.c kernel/timer.c		\
      kernel/stream.c uart.c gps.c  afsk_tx.c afsk_rx.c	\
      hdlc_encoder.c hdlc_decoder.c fbuf.c ax25.c adc.c monitor.c	\
      tracker.c radio.c transceiver.c $(PSRC) $(USB_SRC)


# List Assembler source files here.
#     Make them always end in a capital .S.  Files ending in a lowercase .s
#     will not be considered source files but generated files (assembler
#     output from the compiler), and will be deleted upon "make clean"!
#     Even though the DOS/Win* filesystem matches both .s and .S the same,
#     it will preserve the spelling of the filenames, and gcc itself does
#     care about how the name is spelled on its command-line.
ASRC =


# Compiler flags.
CFLAGS = -O2 $(USB_CFLAGS) -I. -DF_CPU=$(F_CPU)UL -funsigned-char --std=gnu99 -Wall -Wa,-ahlms=$(<:.c=.lst) -fno-strict-aliasing -fshort-enums

# Assembler flags.
ASFLAGS = -Wa,-ahlms=$(<:.s=.lst),--gstabs 

# Linker flags (passed via GCC).
LDFLAGS = -Wl,-Map=$(TARGET).map,--cref,-u,vfprintf,-u,vfscanf,--section-start=.sign=0xd000 -lprintf_flt -lscanf_flt -lm -lc -lm
# LDFLAGS = -Wl,--script=$(TARGET).x,-Map=$(TARGET).map,--cref,-u,vfprintf,-u,vfscanf -lprintf_flt -lscanf_flt -lm -lc -lm

# Define all project specific object files.
OBJ = $(SRC:.c=.o) $(ASRC:.s=.o) 	

# Define all listing files.
LST = $(ASRC:.s=.lst) $(SRC:.c=.lst)

# Compiler flags to generate dependency files.
		GENDEPFLAGS = -Wp,-M,-MP,-MT,$(*F).o,-MF,.dep/$(@F).d

# Add target processor to flags.
CFLAGS += -mmcu=$(MCU) $(GENDEPFLAGS)
ASFLAGS += -mmcu=$(MCU)
LDFLAGS += -mmcu=$(MCU)	

.PHONY : build
build: $(TARGET).elf $(TARGET).hex $(TARGET).img $(TARGET).lss $(TARGET).bin line1 overallsize line2

.PHONY : BuildAll
buildall: clean build

.PHONY : overallsize
overallsize:
	@echo Elf size:
	$(ELFSIZE)


%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

# Create final output files (.hex, .img, .lss) from ELF output file.
%.hex: %.elf
	$(OBJCOPY) -O $(FORMAT) -R .eeprom $< $@

%.img: %.elf
	$(OBJCOPY) -O binary -R .eeprom $< $@

# Create extended listing file from ELF output file.
%.lss: %.elf
	$(OBJDUMP) -h -S $< > $@

# Link: create ELF output file from object files.
.SECONDARY : $(TRG).elf
.PRECIOUS : $(OBJ)
%.elf: $(OBJ)
	$(COMPILE) $(OBJ) $(LDFLAGS)  --output $@

# I need to specify this rule explicit for some reason - la7dja
$(TARGET).elf: $(OBJ)
	$(COMPILE) $(OBJ) $(LDFLAGS) --output $@

# Compile: create object files from C source files.
%.o : %.c
	$(COMPILE) -c $(CFLAGS) $< -o $@ 

# Compile: create assembler files from C source files.
%.s : %.c
	$(COMPILE) -S -fverbose-asm $(CFLAGS) $< -o $@ 

# Assemble: create object files from assembler files.

%.o : %.s
	$(ASSEMBLE) -c $(ASFLAGS) $< -o $@

# device firmware upload via usb 
dfu: $(TARGET).hex
	dfu-programmer $(MCU) erase
	dfu-programmer $(MCU) flash $(TARGET).hex
	dfu-programmer $(MCU) start

# device firmware upload via JTAG
JTAGID=jtagmkII
JTAGDEV=usb
jtag: $(TARGET).hex
	avrdude -p$(MCU) -P$(JTAGDEV) -c$(JTAGID) -Uflash:w:$(TARGET).hex


# Target: line1 project.
.PHONY : line1
line1 :
	@echo ---------------------------------------------------------------------------------------------------

# Target: line2 project.
.PHONY : line2
line2 :
	@echo ---------------------------------------------------------------------------------------------------

# Target: clean project.
.SILENT : Clean
.PHONY : Clean
clean :
	$(REMOVE) $(TARGET).hex
	$(REMOVE) $(TARGET).obj
	$(REMOVE) $(TARGET).elf
	$(REMOVE) $(TARGET).bin
	$(REMOVE) $(TARGET).map
	$(REMOVE) $(TARGET).obj
	$(REMOVE) $(TARGET).sym
	$(REMOVE) $(TARGET).lnk
	$(REMOVE) $(TARGET).lss
	$(REMOVE) $(OBJ)
	$(REMOVE) $(LST)
	$(REMOVE) $(SRC:.c=.s)



# Include the dependency files.
-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)


# List assembly only source file dependencies here:

