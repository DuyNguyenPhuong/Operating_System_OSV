D := $(dir $(lastword $(MAKEFILE_LIST)))
# Remove trailing '/'
D := $(D:/=)
O := $(BUILD)/$(D)

# UNAME_S	:= $(shell uname -s)
# ifeq ($(UNAME_S),Darwin)
# CC := clang -target $(ARCH)-linux-gnu
# CFLAGS += -Wno-initializer-overrides
# TOOLPREFIX ?= $(ARCH)-linux-gnu-
# else
CC := gcc
# endif

LD := $(TOOLPREFIX)ld
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump
GDB := $(TOOLPREFIX)gdb

QEMU := qemu-system-x86_64
QEMUOPTS += 
CFLAGS += -I $(D)/include -D X64
TOOLS_CFLAGS += -I $(D)/include
GDBINIT := $(D)/gdbinit
KERNEL_CFLAGS += -mcmodel=kernel -mno-red-zone -mtls-direct-seg-refs

### Bootloader ###
# Compile with '-Os' to fit in one sector
BOOTLOADER_MAIN_SRC := $(D)/boot/bootmain.c
BOOTLOADER_MAIN_OBJ := $(O)/boot/bootmain.o
BOOTLOADER_ASM_SRC := $(D)/boot/bootasm.S
BOOTLOADER_ASM_OBJ := $(O)/boot/bootasm.o
BOOTLOADER := $(O)/boot/bootloader
BOOTLOADER_OBJS := $(BOOTLOADER_ASM_OBJ) $(BOOTLOADER_MAIN_OBJ)
BOOTLOADER_ASM := $(O)/boot/bootloader.asm
SIGN := $(D)/boot/sign.py

$(BOOTLOADER): $(BOOTLOADER).o
	$(OBJCOPY) -S -O binary -j .text $< $@
	$(SIGN) $@

$(BOOTLOADER).o: $(BOOTLOADER_OBJS)
	$(LD) -m elf_i386 -N -e start -Ttext 0x7C00 -o $@ $^
	$(OBJDUMP) -S $@ > $(BOOTLOADER_ASM)

$(BOOTLOADER_MAIN_OBJ): $(BOOTLOADER_MAIN_SRC)
	$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) -m32 -c -Os -o $@ -c $<

$(BOOTLOADER_ASM_OBJ): $(BOOTLOADER_ASM_SRC)
	$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) -m32 -c -Os -o $@ -c $<

#### Entry_ap  ###
ENTRY_AP_OBJ := $(O)/kernel/entry_ap.o
ENTRY_AP := entry_ap

$(ENTRY_AP): $(ENTRY_AP_OBJ)
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o $<.out $<
	$(OBJCOPY) -S -O binary -j .text $<.out $@

CFLAG += -m64
LDFLAGS += -m elf_x86_64 -nodefaultlibs

### Vectors ###
VECTORS := $(D)/kernel/vectors.S
VECTORS_GEN := $(D)/kernel/vectors.py
$(VECTORS): $(VECTORS_GEN)
	python3 $(VECTORS_GEN) > $@

### Kernel objects ###
ARCH_KERNEL_SRCS := $(shell find $(D)/kernel -type f -name '*.c' -o -name '*.S' ! -name 'entry_ap.*' )
ARCH_KERNEL_SRCS := $(sort $(ARCH_KERNEL_SRCS) $(VECTORS)) # Remove duplicates

ARCH_KERNEL_OBJS := $(addprefix $(BUILD)/, $(patsubst %.c, %.o, $(patsubst %.S, %.o, $(ARCH_KERNEL_SRCS))))
ARCH_KERNEL_LD := $(D)/kernel/kernel.ld

### User objects ###
ARCH_USER_SRCS := $(shell find $(D)/user -type f -name '*.c' -o -name '*.S')
ARCH_USER_OBJS := $(addprefix $(BUILD)/, $(patsubst %.c, %.o, $(patsubst %.S, %.o, $(ARCH_USER_SRCS))))

$(ARCH_USER_OBJS): $(O)/user/%.o: $(D)/user/%.S
	$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) -o $@ -c $<
