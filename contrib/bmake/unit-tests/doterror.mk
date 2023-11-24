# $NetBSD: doterror.mk,v 1.3 2023/06/01 20:56:35 rillig Exp $


.BEGIN:
	@echo At first, I am

.END:
	@echo not reached

.ERROR:
	@echo "$@: Looks like '${.ERROR_TARGET}' is upset."

all:	happy sad

happy:
	@echo $@

sad:
	@echo and now: $@; exit 1
