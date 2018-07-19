
BUILDDIR_default = obj
SRCDIR ?= .
TOPDIR = $(abspath $(SRCDIR)/../..)
include $(TOPDIR)/Config.mk

ifeq (,$(MAKENOW))

SRCROOT = $(abspath $(TOPDIR)/common/yajl)

VPATH = $(SRCROOT)

include $(SRCROOT)/Makefile.yajl

dist: all

endif # MAKENOW
