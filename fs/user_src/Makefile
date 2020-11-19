
CC := gcc
CFLAGS :=-nostdlib -m32 -fno-stack-protector -Wall -fno-builtin -g -ffreestanding
CXXFLAGS := $(CFLAGS)
ASFLAGS := $(CFLAGS)
LDFLAGS:=-static -g

OBJS_SRC := ./start_common.S
OBJS := ./start_common.o

SRC := $(shell find . -name '*.c' -or -name '*.S' -or -name '*.s')
SRC := $(filter-out $(OBJS_SRC),$(SRC))
TARGETS := $(basename $(SRC))

ALL := $(TARGETS)

# See: https://www.cmcrossroads.com/article/printing-value-makefile-variable
# See: https://stackoverflow.com/questions/16467718/how-to-print-out-a-variable-in-makefile
print-%  : ; @echo $* = $($*)

# $(TARGETS): $(SRC) $(OBJS) Makefile
# 	$(CC) $(CFLAGS) $(OBJS) $(SRC) -o $@
all: $(OBJS) $(TARGETS) Makefile syscall.h

%: %.c Makefile syscall.h
	$(CC) $(CFLAGS) $(OBJS) $< -o $@ $(LDFLAGS)

$(OBJS): $(OBJS_SRC)
	$(CC) $(CFLAGS) -c $^ -o $@ $(LDFLAGS)

# hello: hello_world.c syscall.h
# 	gcc hello_world.c -m32 -Wall -nostdlib -ffreestanding -o hello

.PHONY: clean
clean:
	@$(RM) $(TARGETS) $(OBJS)