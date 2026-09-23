# use gcc and vasm
CC=m68k-amigaos-gcc -c
LD=m68k-amigaos-gcc
AS=vasmm68k_mot

# GCC LTO requires the 16.2.1 toolchain package (GCC 16.2.0b with the
# HUNK/binutils plugin fixes). Older linkers can warn that the plugin is
# needed yet return success with an unusable device image.
# LTO=0 remains available for size and regression comparisons.
LTO ?= 1
ifeq ($(LTO),1)
LTO_SUFFIX = _lto
LTO_FLAGS = -flto -fuse-linker-plugin
LTO_LINK_FLAGS = $(LTO_FLAGS) -Wl,--fatal-warnings
else ifneq ($(LTO),0)
$(error LTO must be 0 or 1)
endif

# NDK includes/libs
NDK_DIR ?= $(AMIGA_DIR)/ndk_3.9
NDK_INC = $(NDK_DIR)/include/include_h
NDK_LIB = $(NDK_DIR)/include/linker_libs
NDK_INC_ASM = $(NDK_DIR)/include/include_i

# netinclude
NET_INC ?= $(AMIGA_DIR)/roadshow/netinclude
DEV_INC ?= $(AMIGA_DIR)/roadshow/include

# GCC's 68000 -fbaserel form emits out-of-range text relocations for this
# device; absolute relocations work for the 000 build. Keep the tested 020+
# base-relative form until both have been exercised on their target CPUs.
ifeq ($(CPUSUFFIX),000)
BASEREL =
else
BASEREL = -fbaserel -DBASEREL
endif

CFLAGS = -Wall -Werror -noixemul -mcrt=clib2
CFLAGS += -mcpu=68$(CPUSUFFIX) $(BASEREL) -Os
CFLAGS += $(if $(VBCC_INC),-I$(VBCC_INC)) -I$(NDK_INC) -I$(NET_INC) -I$(DEV_INC)
CFLAGS += -I$(DEVICE_NAME) -I.

CFLAGS += -DDEVICE_NAME='"$(DEVICE_NAME).device"'
CFLAGS += -DDEVICE_VERSION=$(DEVICE_VERSION)
CFLAGS += -DDEVICE_REVISION=$(DEVICE_REVISION)
CFLAGS += -DDEVICE_ID='"$(DEVICE_ID)"'
CFLAGS += $(LTO_FLAGS)
CFLAGS += $(EXTRA_CFLAGS)

OBJ_NAME = -o
CFLAGS_RELEASE = $(CFLAGS)

LDFLAGS = -mcpu=68$(CPUSUFFIX) $(BASEREL) -L$(NDK_LIB)
LDFLAGS += -Os $(LTO_LINK_FLAGS)
LIBS_debug = -ldebug
LIBS = -lamiga -lc
LDFLAGS_RELEASE = $(LDFLAGS) $(LIBS) -o
LDFLAGS_DEV = -nostartfiles
LDFLAGS_APP =
LDFLAGS_HAS_MAP = 0

# HUNK LTO cannot link GCC's temporary DWARF/debug hunk. The existing
# non-LTO -g build also warns that its debug hunk was not written. Keep
# debug logging, but omit unavailable symbol data in the LTO variant.
ifeq ($(LTO),1)
CFLAGS_DEBUG = $(CFLAGS)
LDFLAGS_DEBUG = $(LDFLAGS) $(LIBS_debug) $(LIBS) -o
else
CFLAGS_DEBUG = $(CFLAGS) -g
LDFLAGS_DEBUG = $(LDFLAGS) $(LIBS_debug) -g $(LIBS) -o
endif

ASFLAGS = -Fhunk -quiet -phxass -m68$(CPUSUFFIX) -DGCC_BUILD -I$(NDK_INC_ASM)
