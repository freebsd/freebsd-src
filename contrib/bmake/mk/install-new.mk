# $Id: install-new.mk,v 1.9 2025/11/19 17:44:15 sjg Exp $
#
#	@(#) Copyright (c) 2009, Simon J. Gerraty
#
#	SPDX-License-Identifier: BSD-2-Clause
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

.if !defined(InstallNew)

# How do we want CmpCpMv to do the final operation?
# the backup (if any) will use the opposite.
CPMV_OP ?= mv
# clear this if not supported
CPMV_f ?= -f

# copy/move if src and target are different making a backup if desired
CmpCpMv= CmpCpMv() { \
	src=$$1 target=$$2 _bak=$$3; \
	if ! test -s $$target || ! cmp -s $$target $$src; then \
		trap "" 1 2 3 15; \
		case "/${CPMV_OP}" in */cp) bop=mv;; */mv) bop=cp;; esac; \
		if test -s $$target; then \
			if test "x$$_bak" != x; then \
				rm -f $$target$$_bak; \
				$$bop ${CPMV_f} $$target $$target$$_bak; \
			else \
				rm -f $$target; \
			fi; \
		fi; \
		${CPMV_OP} ${CPMV_f} $$src $$target; \
	fi; }

# If the .new file is different, we want it.
# Note: this function will work as is for *.new$RANDOM"
InstallNew= ${CmpCpMv}; InstallNew() { \
	_t=-e; _bak=; \
	while :; do \
		case "$$1" in \
		-?) _t=$$1; shift;; \
		--bak) _bak=$$2; shift 2;; \
		*) break;; \
		esac; \
	done; \
	for new in "$$@"; do \
		if test $$_t $$new; then \
			if ${isPOSIX_SHELL:Ufalse}; then \
				target=$${new%.new}; \
			else \
				target=`expr $$new : '\(.*\).new'`; \
			fi; \
			CmpCpMv $$new $$target $$_bak; \
		fi; \
		rm -f $$new; \
	done; :; }

.endif
