ROOT_DIR ?= ..
include $(ROOT_DIR)/config.mk

LIB_DKM := libbenchcommon_dkm.a
LIB_RTP := libbenchcommon_rtp.a
OBJDIR_DKM := build_dkm
OBJDIR_RTP := build_rtp
SRCS := $(wildcard src/*.c)
SRCS_DKM := $(filter-out src/transport_local.c,$(SRCS))
SRCS_RTP := $(SRCS)
OBJS_DKM := $(patsubst src/%.c,$(OBJDIR_DKM)/%.o,$(SRCS_DKM))
OBJS_RTP := $(patsubst src/%.c,$(OBJDIR_RTP)/%.o,$(SRCS_RTP))

CPPFLAGS := $(INC_COMMON)

.PHONY: all clean

all: $(LIB_DKM) $(LIB_RTP)

$(LIB_DKM): $(OBJS_DKM)
	$(AR_COMMON) rcs $@ $^

$(LIB_RTP): $(OBJS_RTP)
	$(AR_COMMON) rcs $@ $^

$(OBJDIR_DKM)/%.o: src/%.c | $(OBJDIR_DKM)
	$(CC_COMMON) $(CPPFLAGS) $(CFLAGS_DKM) -c $< -o $@

$(OBJDIR_RTP)/%.o: src/%.c | $(OBJDIR_RTP)
	$(CC_COMMON) $(CPPFLAGS) $(CFLAGS_RTP) -c $< -o $@

$(OBJDIR_DKM):
	mkdir -p $(OBJDIR_DKM)

$(OBJDIR_RTP):
	mkdir -p $(OBJDIR_RTP)

clean:
	rm -rf $(OBJDIR_DKM) $(OBJDIR_RTP) $(LIB_DKM) $(LIB_RTP)
