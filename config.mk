# VxWorks makefile configuration (edit as needed)

# ---- Build mode ----
MODE ?= release
ifeq ($(MODE),debug)
OPTFLAGS := -O0 -g
else
OPTFLAGS := -O2
endif

# ---- VxWorks base (from env or override here) ----
WIND_BASE ?= $(WIND_BASE)
VSB_DIR ?= $(WIND_CC_SYSROOT)
WIND_BASE_POSIX := $(subst \,/,$(WIND_BASE))
VSB_DIR_POSIX := $(subst \,/,$(VSB_DIR))
LLVM_ROOT_POSIX := $(subst \,/,$(LLVM_ROOT))

# ---- Toolchain ----
VX_TOOLCHAIN_BIN ?= $(WIND_BASE_POSIX)/host/x86-win64/bin
WR_CC ?= $(if $(WIND_BASE),$(VX_TOOLCHAIN_BIN)/wr-cc,wr-cc)
LLVM_AR_PATH := $(if $(LLVM_ROOT_POSIX),$(LLVM_ROOT_POSIX)/bin/llvm-ar,)
WR_AR ?= $(if $(wildcard $(VX_TOOLCHAIN_BIN)/llvm-ar*),$(VX_TOOLCHAIN_BIN)/llvm-ar,$(if $(wildcard $(LLVM_AR_PATH)*),$(LLVM_AR_PATH),llvm-ar))

# ---- Target/CPU ----
VX_TARGET ?= powerpc-wrs-vxworks
VX_CPU ?= e6500
VX_CPU_FLAGS ?= --target=$(VX_TARGET) -mcpu=$(VX_CPU) -mhard-float -mno-altivec -msecure-plt

# ---- Sysroot / includes ----
VX_SYSROOT ?= $(VSB_DIR_POSIX)
ifeq ($(strip $(VX_SYSROOT)),)
$(warning VX_SYSROOT is empty. Set VSB_DIR or WIND_CC_SYSROOT for VxWorks builds.)
endif
VX_INCLUDE_DIRS ?= \
	$(VX_SYSROOT)/usr/h/public \
	$(VX_SYSROOT)/usr/h \
	$(VX_SYSROOT)/share/h/public \
	$(VX_SYSROOT)/krnl/h/public/base \
	$(WIND_BASE_POSIX)/target/h
VX_CPPFLAGS ?= -D_VSB_CONFIG_FILE=\"$(VX_SYSROOT)/h/config/vsbConfig.h\"
VX_CFLAGS ?= $(VX_CPU_FLAGS) --sysroot=$(VX_SYSROOT) $(addprefix -I,$(VX_INCLUDE_DIRS)) $(VX_CPPFLAGS)
VX_LDFLAGS ?= $(VX_CPU_FLAGS) --sysroot=$(VX_SYSROOT)

# ---- Common include ----
INC_COMMON ?= -I$(ROOT_DIR)/common/include

# ---- Toolchain / flags ----
CC_COMMON ?= $(WR_CC)
AR_COMMON ?= $(WR_AR)
CFLAGS_COMMON ?= $(OPTFLAGS) $(VX_CFLAGS)

CC_DKM ?= $(WR_CC)
LD_DKM ?= $(WR_CC)
CFLAGS_DKM ?= $(OPTFLAGS) $(VX_CFLAGS) -D_WRS_KERNEL
LDFLAGS_DKM ?= $(VX_LDFLAGS) -r

CC_RTP ?= $(WR_CC)
LD_RTP ?= $(WR_CC)
CFLAGS_RTP ?= $(OPTFLAGS) $(VX_CFLAGS) -D__RTP__
LDFLAGS_RTP ?= $(VX_LDFLAGS) -rtp -static
