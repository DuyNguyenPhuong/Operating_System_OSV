D := $(dir $(lastword $(MAKEFILE_LIST)))
# Remove trailing '/'
D := $(D:/=)
O := $(BUILD)/$(D)

### Kernel objects ###
KERNEL_SRCS := $(shell find $(D) -type f -name '*.c' -o -name '*.S')

KERNEL_OBJS := $(addprefix $(BUILD)/, $(patsubst %.c, %.o, $(patsubst %.S, %.o, $(KERNEL_SRCS))))
