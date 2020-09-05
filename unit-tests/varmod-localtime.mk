# $NetBSD: varmod-localtime.mk,v 1.3 2020/08/23 15:13:21 rillig Exp $
#
# Tests for the :localtime variable modifier, which returns the given time,
# formatted as a local timestamp.

all:
	@echo ${%Y:L:localtim=1593536400}	# modifier name too short
	@echo ${%Y:L:localtime=1593536400}	# 2020-07-01T00:00:00Z
	@echo ${%Y:L:localtimer=1593536400}	# modifier name too long
