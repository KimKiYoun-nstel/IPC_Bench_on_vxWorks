ROOT_DIR ?= ..
include $(ROOT_DIR)/config.mk

OUT := bench_dkm.out
OBJDIR := build
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRCS))

CFLAGS := $(CFLAGS_DKM) $(INC_COMMON) -Iinclude
LDFLAGS := $(LDFLAGS_DKM)
LIBS := $(ROOT_DIR)/common/libbenchcommon.a

.PHONY: all clean

all: $(OUT)

$(OUT): $(OBJS) $(LIBS)
	$(LD_DKM) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC_DKM) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(OUT)
