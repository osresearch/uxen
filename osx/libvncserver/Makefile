
BUILDDIR_default = obj
SRCDIR ?= .
TOPDIR = $(abspath $(SRCDIR)/../..)
include $(TOPDIR)/Config.mk

ifeq (,$(MAKENOW))

SRCROOT = $(TOPDIR)/common/libvncserver

VPATH = $(SRCROOT)

include $(SRCROOT)/Makefile.libvncserver

endif # MAKENOW
