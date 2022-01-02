# Currently only support x86, x86_64
ARCH := x86_64

# Arch-specific makefile needs to define (or add to) the following
CC :=
LD :=
OBJDUMP :=
OBJCOPY :=
GDB :=
QEMU :=
CFLAGS := -ffreestanding -fwrapv -fno-pic -fno-stack-protector -Wall -Werror -g -MMD -MP -I include
CFLAGS +=  
LDFLAGS :=
TOOLS_CFLAGS := -Werror -Wall -I include
KERNEL_CLFAGS :=

MKDIR_P := mkdir -p
HOST_CC := gcc
QEMUOPTS := -serial mon:stdio -m 512 -no-reboot -device isa-debug-exit  #-d int
QEMUTESTOPTS := -serial pipe:/tmp/osv-test -monitor none -m 512 -no-reboot -device isa-debug-exit  #-d int
GDBPORT := $(shell expr `id -u` % 5000 + 25000)
QEMUGDB := -gdb tcp::$(GDBPORT)

BUILD := build
OSV_IMG := $(BUILD)/osv.img
FS_IMG := $(BUILD)/fs.img
KERNEL_ELF := $(BUILD)/kernel/kernel.elf

# Some extra files for filesystem testing
LARGEFILE := $(BUILD)/largefile
README := $(BUILD)/README
SMALLFILE := $(BUILD)/smallfile

### Default rule ###
all: osv tools

### Arch-specific makefiles ###
BOOTLOADER :=
ARCH_KERNEL_OBJS :=
ARCH_KERNEL_LD :=
GDBINIT :=

ifeq ($(ARCH), x86)
include arch/x86/Rules.mk
else ifeq ($(ARCH), x86_64)
include arch/x86_64/Rules.mk
else
$(error Unsupported architecture $(ARCH))
endif

### Kernel makefile ###
include kernel/Rules.mk

### Library makefile ###
include lib/Rules.mk

### User makefile ###
include user/Rules.mk

### General rules ###
osv: $(OSV_IMG) $(FS_IMG)

$(OSV_IMG): $(BOOTLOADER) $(KERNEL_ELF)
	dd if=/dev/zero of=$@ count=10000
	dd if=$(BOOTLOADER) of=$@ conv=notrunc
	dd if=$(KERNEL_ELF) of=$@ seek=1 conv=notrunc

$(LARGEFILE):
	$(MKDIR_P) $(@D)
	cat /dev/zero | tr '\0' 'a' | dd of=$@ count=40

$(SMALLFILE):
	$(MKDIR_P) $(@D)
	echo "aaaaaaaaaabbbbbbbbbbccccc" > $@

$(README):
	$(MKDIR_P) $(@D)
	echo "*************************************************" > $@
	echo "***OSV, the Ultimate Teaching Operating System***" >> $@
	echo "*************************************************" >> $@

$(FS_IMG): $(BUILD)/tools/mkfs $(README) $(LARGEFILE) $(SMALLFILE) $(USER_OBJS)
	$(BUILD)/tools/mkfs $@ $(README) $(LARGEFILE) $(SMALLFILE) $(USER_BIN)

$(KERNEL_ELF): $(ARCH_KERNEL_OBJS) $(ARCH_KERNEL_LD) $(KERNEL_OBJS) $(KLIB_OBJS) $(ENTRY_AP)
	$(LD) $(LDFLAGS) -T $(ARCH_KERNEL_LD) -o $@ $(ARCH_KERNEL_OBJS) $(KERNEL_OBJS) $(KLIB_OBJS) -b binary $(ENTRY_AP)
	$(OBJDUMP) -S $@ > $(BUILD)/kernel/kernel.asm
	$(OBJDUMP) -t $@ > $(BUILD)/kernel/kernel.sym


$(ULIB_OBJS): $(BUILD)/ulib/%.o: lib/%.c
	$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BUILD)/user/%.o: user/%.c $(ARCH_USER_OBJS) $(ULIB_OBJS)
	$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) -o $@ -c $<
	$(LD) $(LDFLAGS) --no-omagic -e main -o $(subst .o, , $@) $@ $(ARCH_USER_OBJS) $(ULIB_OBJS)
	rm $@ $(subst .o,.d, $@)

$(BUILD)/%.o: %.c
	$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -o $@ -c $<

$(BUILD)/%.o: %.S
	$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) -o $@ -c $<

tools: $(BUILD)/tools/mkfs

$(BUILD)/tools/mkfs: tools/mkfs.c
	$(MKDIR_P) $(@D)
	$(HOST_CC) $(TOOLS_CFLAGS) -o $@ $<

ifndef CPUS
CPUS := 2
endif

# Dependency generation
DEPS := $(shell find $(BUILD)/ -type f -name '*.d')
-include $(DEPS)

clean:
	-rm -f .gdbinit $(VECTORS) $(ENTRY_AP)
	-rm -rf $(BUILD)

### QEMU and GDB ###
DRIVE_OPTS := -drive file=$(OSV_IMG),index=0,media=disk,format=raw -drive file=$(FS_IMG),index=1,media=disk,format=raw -smp $(CPUS)

qemu: osv
	$(QEMU) $(QEMUOPTS) $(DRIVE_OPTS) -nographic

qemu-test: osv
	$(QEMU) $(QEMUTESTOPTS) $(DRIVE_OPTS) -nographic

qemu-graphic: osv
	$(QEMU) $(QEMUOPTS) $(DRIVE_OPTS)

qemu-gdb: osv
	cp ~/.gdbinit .tmpgdb
	sed "s/localhost:21795/localhost:$(GDBPORT)/" < .tmpgdb > ~/.gdbinit
	rm .tmpgdb
	$(QEMU) $(QEMUOPTS) $(DRIVE_OPTS) -nographic -S $(QEMUGDB)

gdb: $(GDBINIT)
	ln -s -f $(GDBINIT) .gdbinit
	$(GDB)

