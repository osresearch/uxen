
TOPDIR = ../..
include $(TOPDIR)/Config.mk

SUBDIRS  =
SUBDIRS += uxendisp
SUBDIRS += uxenps2
SUBDIRS += uxenplatform
SUBDIRS += uxenvmlib
SUBDIRS += uxenv4vguest
SUBDIRS += uxennet
SUBDIRS += uxenstor

TARGETS = all dist

.PHONY: $(TARGETS)

$(TARGETS): % : subdirs-%

.PHONY: clean
clean::
	$(_W)echo Cleaning - $(BUILDDIR)
	$(_V)rm -rf $(BUILDDIR)

.PHONY: tests
tests:: subdirs-tests
