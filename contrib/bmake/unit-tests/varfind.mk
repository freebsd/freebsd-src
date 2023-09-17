# $NetBSD: varfind.mk,v 1.1 2020/07/25 21:19:29 rillig Exp $
#
# Demonstrates variable name aliases in VarFind.

all: VarFind-aliases.to

.SUFFIXES: .from .to

VarFind-aliases.from:
	@: do nothing

VarFind-aliases.to: explicit-dependency

explicit-dependency:
	@: do nothing

.from.to:
	@echo $@: long ${.ALLSRC:Q}
	@echo $@: abbr ${>:Q}
	@echo $@: long ${.ARCHIVE:Q}
	@echo $@: abbr ${!:Q}
	@echo $@: long ${.IMPSRC:Q}
	@echo $@: abbr ${<:Q}
	@echo $@: long ${.MEMBER:Q}
	@echo $@: abbr ${%:Q}
	@echo $@: long ${.OODATE:Q}
	@echo $@: abbr ${?:Q}
	@echo $@: long ${.PREFIX:Q}
	@echo $@: abbr ${*:Q}
	@echo $@: long ${.TARGET:Q}
	@echo $@: abbr ${@:Q}
