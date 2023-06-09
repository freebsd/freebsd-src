# Makefile fragment - requires GNU make
#
# Copyright (c) 2022, Arm Limited.
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

# These targets are defined if we prescribe pl in SUBS.
# It requires PLSUBS to be set.

$(foreach sub,$(PLSUBS),$(eval include $(srcdir)/pl/$(sub)/Dir.mk))

pl-files := $($(PLSUBS:%=pl/%-files))

all-pl: $(PLSUBS:%=all-pl/%)

check-pl: $(PLSUBS:%=check-pl/%)

install-pl: $(PLSUBS:%=install-pl/%)

clean-pl: $(PLSUBS:%=clean-pl/%)

.PHONY: all-pl check-pl install-pl clean-pl
