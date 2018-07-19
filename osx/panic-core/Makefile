# 
# Copyright (c) 2013, Christian Limpach <Christian.Limpach@gmail.com>
# 
# Permission to use, copy, modify, and/or distribute this software
# for any purpose with or without fee is hereby granted, provided
# that the above copyright notice and this permission notice appear
# in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# 

BUILDDIR_default = obj
SRCDIR ?= .
TOPDIR = $(abspath $(SRCDIR)/../..)
include $(TOPDIR)/Config.mk

ifeq (,$(MAKENOW))

SRCROOT = $(SRCDIR)

VPATH = $(SRCROOT)

$(HOST_WINDOWS)CC := $(TOOLSDIR)/bin/x86_64-w64-mingw32-gcc

$(HOST_WINDOWS)CPPFLAGS += -I$(TOPDIR)/windows/include

PROGRAMS = panic-core$(HOST_EXE_SUFFIX) macho-edit$(HOST_EXE_SUFFIX)
SCRIPTS = gdb-panic

all: $(PROGRAMS) $(SCRIPTS)
	@ :

_install_banner:
	$(_W)echo Installing from $(abspath $(BUILDDIR)) to $(DISTDIR)

$(patsubst %,install_%,$(PROGRAMS)): install_%: % _install_banner
	$(_W)echo Installing -- $(<)
	$(_V)$(call install_exe,$(<),$(DISTDIR))

_install_banner_scripts:
	$(_W)echo Installing from $(abspath $(SRCDIR)) to $(DISTDIR)

$(patsubst %,install_%,$(SCRIPTS)): install_%: % _install_banner_scripts
	$(_W)echo Installing -- $(<F)
	$(_V)$(call install_data,$(<),$(DISTDIR))

dist: $(patsubst %,install_%,$(PROGRAMS) $(SCRIPTS))

panic_core_SRCS = panic-core.c
panic_core_OBJS  = $(patsubst %.c,%.o,$(panic_core_SRCS))
panic_core_OBJS := $(subst /,_,$(panic_core_OBJS))

MULTILIBDIR := $(TOOLSDIR)/host-all/lib/`$(CC) -print-multi-os-directory`
$(panic_core_OBJS) : CPPFLAGS += -iquote$(TOOLSDIR)/host-all/include
panic_core_LDLIBS = -L$(TOOLSDIR)/host-all/lib -L$(MULTILIBDIR) \
        -lbfd -liberty

panic-core$(HOST_EXE_SUFFIX): $(panic_core_OBJS)
	$(_W)echo Linking - $@
	$(_V)$(call link,$@,$^ $(panic_core_LDLIBS))

macho_edit_SRCS = macho-edit.c
macho_edit_OBJS  = $(patsubst %.c,%.o,$(macho_edit_SRCS))
macho_edit_OBJS := $(subst /,_,$(macho_edit_OBJS))

macho-edit$(HOST_EXE_SUFFIX): $(macho_edit_OBJS)
	$(_W)echo Linking - $@
	$(_V)$(call link,$@,$^ $(macho_edit_LDLIBS))

%.o: %.c
	$(_W)echo Compiling - $@
	$(_V)$(COMPILE.c) $< -o $@

OBJS = $(panic_core_OBJS)
OBJS += $(macho_edit_OBJS)
$(OBJS): .deps/.exists

-include .deps/*.d

endif # MAKENOW
