CURRENTDIR	= .
SRCDIR		= $(CURRENTDIR)

INCLUDEDIR	= $(SRCDIR)/../mach-$(ARCH)/includes/registers

# Linker script
BASE_ADDR	?= 0x00000000
BOOT_LAYOUT	= $(SRCDIR)/link.lds


CROSS_COMPILE ?= arm-linux-gnueabi-

AS	= $(CROSS_COMPILE)as
CC	= $(CROSS_COMPILE)gcc
LD	= $(CROSS_COMPILE)ld
CPP	= $(CROSS_COMPILE)cpp
STRIP	= $(CROSS_COMPILE)strip
OBJCOPY	= $(CROSS_COMPILE)objcopy
OBJDUMP	= $(CROSS_COMPILE)objdump

CFLAGS 	= -g -Wall -I$(INCLUDEDIR) -I$(INCLUDEDIR)/mach -I./ -O -D$(MEM_TYPE)
LDFLAGS = -static -nostdlib -T $(BOOT_LAYOUT)

# Generic code
CORE_OBJS = holitest.o debug.o

all: holitest


%.o: %.S
	$(CC) -c $(CFLAGS) -D__ASSEMBLY__ -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

#
# Make targets
#
.PHONY: all build clean

holitest: $(BOOT_LAYOUT) $(CORE_OBJS)
	$(LD) -o $@ $(CORE_OBJS) $(LDFLAGS)

clean:
	@echo Cleaning...
	@echo Files:
	@echo Build output:
	rm -rf *.o
	rm -f holitest
