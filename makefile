# Makefile for AVR target

# Define directories.
  INCDIR = .
  LIBDIR = .
    LIBS =

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
    
# LUFA compile
    BOARD    = POLARIC
    MCU      = at90usb1287    
    LUFADEFS = -DBOARD=BOARD_$(BOARD) -DUSE_NONSTANDARD_DESCRIPTOR_NAMES -DNO_STREAM_CALLBACKS
    LUFADEFS += -DUSB_DEVICE_ONLY -DUSE_STATIC_OPTIONS="(USB_DEVICE_OPT_FULLSPEED | USB_OPT_REG_ENABLED | USB_OPT_AUTO_PLL)"

# MCU name and clock speed
	MCU = at90usb1287
        F_CPU = 8000000
        
        
# Output format. Can be [srec|ihex].
    FORMAT = ihex

# Target file name (without extension).
	TARGET = test

# List C source files here.
	SRC = usb_descriptors.c \
              LUFA/Drivers/USB/LowLevel/LowLevel.c LUFA/Drivers/USB/HighLevel/USBTask.c \
              LUFA/Drivers/USB/HighLevel/USBInterrupt.c LUFA/Drivers/USB/HighLevel/Events.c \
              LUFA/Drivers/USB/LowLevel/DevChapter9.c LUFA/Drivers/USB/LowLevel/Endpoint.c \
              LUFA/Drivers/USB/HighLevel/StdDescriptors.c config.c ui.c\
              kernel/kernel.c kernel/timer.c kernel/stream.c uart.c gps.c transceiver.c \
              afsk_tx.c afsk_rx.c hdlc_encoder.c hdlc_decoder.c fbuf.c ax25.c usb.c commands.c adc.c \
              monitor.c tracker.c main.c 

# List Assembler source files here.
	ASRC =
# setjmp/setjmp.s 

# Compiler flags.
	CPFLAGS = $(LUFADEFS) -DF_CPU=$(F_CPU)UL -ggdb -funsigned-char --std=gnu99 -Wall -Wstrict-prototypes -Wa,-ahlms=$(<:.c=.lst)

# Assembler flags.
    ASFLAGS = -Wa,-ahlms=$(<:.s=.lst),--gstabs 

# Linker flags (passed via GCC).
	LDFLAGS = -L$(LIBDIR) -Wl,-Map=$(TARGET).map,--cref,-u,vfprintf,-u,vfscanf -lprintf_flt  -lscanf_flt -lm -lc -lm

# Additional library flags (-lm = math library).
#	LIBFLAGS = -l$(LIBS)

# Define all project specific object files.
	OBJ	= $(SRC:.c=.o) $(ASRC:.s=.o) 	

# Define all listing files.
	LST = $(ASRC:.s=.lst) $(SRC:.c=.lst)

# Compiler flags to generate dependency files.
	GENDEPFLAGS = -Wp,-M,-MP,-MT,$(*F).o,-MF,.dep/$(@F).d

# Add target processor to flags.
	CPFLAGS += -mmcu=$(MCU) $(GENDEPFLAGS)
	ASFLAGS += -mmcu=$(MCU)
	LDFLAGS += -mmcu=$(MCU)	

.PHONY : build
build: $(TARGET).elf $(TARGET).hex $(TARGET).lss $(TARGET).bin line1 overallsize line2

.PHONY : BuildAll
buildall: clean $(TARGET).elf $(TARGET).hex $(TARGET).lss $(TARGET).bin line1 overallsize line2

.PHONY : overallsize
overallsize:
	@echo Elf size:
	$(ELFSIZE)

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

# Create final output files (.hex, .lss) from ELF output file.
%.hex: %.elf
	$(OBJCOPY) -O $(FORMAT) -R .eeprom $< $@

# Create extended listing file from ELF output file.
%.lss: %.elf
	$(OBJDUMP) -h -S $< > $@

# Link: create ELF output file from object files.
.SECONDARY : $(TRG).elf
.PRECIOUS : $(OBJ)
%.elf: $(OBJ)
	$(COMPILE) $(LDFLAGS) $(OBJ) $(LIBFLAGS) --output $@

# Compile: create object files from C source files.
%.o : %.c
	$(COMPILE) -c $(CPFLAGS) -I$(INCDIR) $< -o $@ 

# Compile: create assembler files from C source files.
%.s : %.c
	$(COMPILE) -S -fverbose-asm $(CPFLAGS) -I$(INCDIR) $< -o $@ 

# Assemble: create object files from assembler files.

%.o : %.s
	$(ASSEMBLE) -c $(ASFLAGS) $< -o $@

# device firmware upload via usb 
dfu: $(TARGET).hex
	dfu-programmer $(MCU) erase
	dfu-programmer $(MCU) flash $(TARGET).hex
	dfu-programmer $(MCU) start


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

