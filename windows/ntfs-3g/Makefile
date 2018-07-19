
BUILDDIR_default = obj
SRCDIR ?= .
TOPDIR = $(abspath $(SRCDIR)/../..)
include $(TOPDIR)/Config.mk

ifeq (,$(MAKENOW))

SRCROOT = $(abspath $(TOPDIR)/windows/ntfs-3g)

VPATH = $(SRCROOT)

include $(SRCROOT)/Makefile.ntfs-3g

dist: all

endif # MAKENOW
