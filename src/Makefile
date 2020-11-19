# jprx

# Good makefile reference:
# https://spin.atomicobject.com/2016/08/26/makefile-c-projects/

CC:=gcc
# MMD and MP are for dependencies:
CFLAGS:=-nostdlib -m32 -fno-stack-protector -Wall -fno-builtin -MMD -g -ffreestanding -fPIE -ftree-vectorize
CXXFLAGS := $(CFLAGS)
ASFLAGS := $(CFLAGS)
LDFLAGS:=-static -g

TARGET=kernel
IMAGE=jprxOS.img
IMAGE_DIR=../fs/

IMAGE_DIR:=$(IMAGE_DIR)$(IMAGE)

SRC := $(shell find . -name '*.c' -or -name '*.S' -or -name '*.s')

# See GNU Makefile manual section 8.3
# Set all SRC files to .o instead
# start.o needs to be first! (for multiboot magic without explicit linker script, order matters)
OBJS := start.o
OBJS += $(filter-out ./start.o,$(addsuffix .o, $(basename $(SRC))))

# See GNU Makefile manual section 6.3.1
# .d is a "dependency" and helps with header inclusion as a dependency
DEPS := $(OBJS:.o=.d)

# See: https://www.cmcrossroads.com/article/printing-value-makefile-variable
# See: https://stackoverflow.com/questions/16467718/how-to-print-out-a-variable-in-makefile
print-%  : ; @echo $* = $($*)

# $@ is the file being generated
$(TARGET): $(OBJS) Makefile linker.ld filesys
	$(CC) $(CFLAGS) $(OBJS) -T linker.ld -o $@
	@./linkfs.sh

.PHONY: filesys
filesys: ../fs
	./makefs.sh

.PHONY: clean
clean:
	@$(RM) $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)