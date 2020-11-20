# $NetBSD: error.mk,v 1.3 2020/11/03 17:38:45 rillig Exp $
#
# Demonstrate that the .error directive exits immediately, without
# continuing parsing until the end of the file.

.info just FYI
.warning this could be serious
.error this is fatal
.info this is not reached because of the .error above

all:
	: this is not reached because of the .error
