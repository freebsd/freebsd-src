# $NetBSD: phony-end.mk,v 1.2 2020/10/24 08:34:59 rillig Exp $

all ok also.ok bug phony:
	@echo '${.TARGET .PREFIX .IMPSRC:L:@v@$v="${$v}"@}'

.END:	ok also.ok bug

phony bug:	.PHONY
all: phony
