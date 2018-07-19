
BUILDDIR_default = obj
SRCDIR ?= .
TOPDIR = $(abspath $(SRCDIR)/../..)
include $(TOPDIR)/Config.mk

ifeq (,$(MAKENOW))

SRCROOT = $(TOPDIR)/common/libvhd

VPATH = $(SRCROOT)

include $(SRCROOT)/Makefile

endif # MAKENOW
