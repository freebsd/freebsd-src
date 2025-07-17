# $NetBSD: suff-self.mk,v 1.1 2020/11/16 15:12:16 rillig Exp $
#
# See what happens if someone defines a self-referencing suffix
# transformation rule.

.SUFFIXES: .suff

.suff.suff:
	: Making ${.TARGET} out of ${.IMPSRC}.

all: suff-self.suff
