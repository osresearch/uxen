
BUILDDIR_default = obj
SRCDIR ?= .
TOPDIR = $(abspath $(SRCDIR)/../..)
include $(TOPDIR)/Config.mk

ifeq (,$(MAKENOW))

SRCROOT = $(TOPDIR)/common/xen-tools/libxc

VPATH = $(SRCROOT)

include $(SRCROOT)/Makefile.libxc

endif # MAKENOW
