# Toolchain / flags override (edit me)

# --- Common ---
CC_COMMON ?= $(CC)
AR_COMMON ?= $(AR)
CFLAGS_COMMON ?= -O2

# --- DKM (.out) ---
CC_DKM ?= $(CC)
LD_DKM ?= $(LD)
CFLAGS_DKM ?= -O2
LDFLAGS_DKM ?=

# --- RTP (.vxe) ---
CC_RTP ?= $(CC)
LD_RTP ?= $(LD)
CFLAGS_RTP ?= -O2
LDFLAGS_RTP ?=

# shared include
INC_COMMON ?= -I$(ROOT_DIR)/common/include
