ROOT_DIR ?= ..
include $(ROOT_DIR)/config.mk

LIB := libbenchcommon.a
OBJDIR := build
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRCS))

CFLAGS := $(CFLAGS_COMMON) $(INC_COMMON) -I$(ROOT_DIR)/common/include

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJS)
	$(AR_COMMON) rcs $@ $^

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC_COMMON) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(LIB)
