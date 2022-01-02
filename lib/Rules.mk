D := $(dir $(lastword $(MAKEFILE_LIST)))
# remove trailing
D := $(D:/=)
O := $(BUILD)/$(D)
### Library objects ###

# library used by kernel, exclude malloc and stdio #
KLIB_SRCS := $(shell find $(D) -type f -name '*.c' ! -name "malloc*" ! -name "stdio*" ! -name "u*")

KLIB_OBJS := $(addprefix $(BUILD)/, $(patsubst %.c, %.o, $(KLIB_SRCS)))

# library accessible by user space, all files in lib/ #
ULIB_SRCS := $(shell find $(D) -type f -name '*.c')

ULIB_OBJS := $(addprefix $(BUILD)/u, $(patsubst %.c, %.o, $(ULIB_SRCS)))
