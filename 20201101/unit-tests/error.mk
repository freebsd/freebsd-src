# $NetBSD: error.mk,v 1.2 2020/10/24 08:34:59 rillig Exp $

.info just FYI
.warning this could be serious
.error this is fatal

all:

.info.html:
	@echo this should be ignored
