
TOPDIR = .
include $(TOPDIR)/Config.mk

SUBDIRS  =
SUBDIRS += xen
SUBDIRS += common/xen-tools/firmware
SUBDIRS += common/include
SUBDIRS += $(TARGET_HOST)

TARGETS = all dist clean

.PHONY: $(TARGETS)
$(TARGETS): %: subdirs-%

dist: uxen-interface.txt
	$(_W)echo Installing from $(abspath $(<D)) to $(DISTDIR)
	$(_W)echo Installing -- $(<F)
	$(_V)$(call install_data,$<,$(DISTDIR))

.PHONY: tests
tests:: subdirs-tests

.PHONY: tools
tools:
	@$(MAKE) -C tools all

subdirs-all subdirs-dist: tools-check

TOOLS_UPDATE_TARGET = tools
$(HOST_WINDOWS)TOOLS_UPDATE_TARGET := tools-update

IGNORE_TOOLS_UPDATE = false

.PHONY: tools-check
tools-check:
	$(_W)echo Checking if tools are up-to-date...
	$(_V)git write-tree --prefix tools/ | \
	  cmp -s $(TOOLSDIR)/.tools_revision - || \
	  ( echo ERROR: tools need update -- make $(TOOLS_UPDATE_TARGET) >&2; \
	    $(IGNORE_TOOLS_UPDATE) )

tools-update: TOOLS_VERSION := $(shell git write-tree --prefix tools/)
tools-update:
	$(_W)echo "Updating tools in $(TOOLSDIR) from $(UXEN_DISTFS)"
	$(_W)echo "         version $(TOOLS_VERSION)"
	$(_V)rsync --delete -avHP --modify-window=3 \
	  $(UXEN_DISTFS)/uxen-tools-$(HOST)-$(TOOLS_VERSION)/ \
	  $(TOOLSDIR)/
