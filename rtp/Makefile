ROOT_DIR ?= ..
include $(ROOT_DIR)/config.mk

VXE := bench_rtp.vxe
OBJDIR := build
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRCS))

CPPFLAGS := $(INC_COMMON) -Iinclude
CFLAGS := $(CFLAGS_RTP)
LDFLAGS := $(LDFLAGS_RTP)
LIBS := $(ROOT_DIR)/common/libbenchcommon_rtp.a

.PHONY: all clean

all: $(VXE)

$(VXE): $(OBJS) $(LIBS)
	$(LD_RTP) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC_RTP) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(VXE)
