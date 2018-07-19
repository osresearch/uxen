
BUILDDIR_default = obj
SRCDIR ?= .
TOPDIR = $(abspath $(SRCDIR)/../..)
include $(TOPDIR)/Config.mk

ifeq (,$(MAKENOW))

SRCROOT = $(TOPDIR)/common/filecrypt

VPATH = $(SRCROOT)

include $(SRCROOT)/Makefile.filecrypt

endif # MAKENOW
