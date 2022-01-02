D := $(dir $(lastword $(MAKEFILE_LIST)))

### Library objects ###
USER_SRCS := $(shell find $(D) -type f -name '*.c')

USER_OBJS := $(addprefix $(BUILD)/, $(patsubst %.c, %.o, $(USER_SRCS)))

USER_BIN := $(patsubst %.o, %, $(USER_OBJS))
