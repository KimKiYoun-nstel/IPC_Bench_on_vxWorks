ROOT_DIR ?= ..
include $(ROOT_DIR)/config.mk

VXE := bench_rtp.vxe
OBJDIR := build
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRCS))

CFLAGS := $(CFLAGS_RTP) $(INC_COMMON) -Iinclude
LDFLAGS := $(LDFLAGS_RTP)
LIBS := $(ROOT_DIR)/common/libbenchcommon.a

.PHONY: all clean

all: $(VXE)

$(VXE): $(OBJS) $(LIBS)
	$(LD_RTP) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC_RTP) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(VXE)
