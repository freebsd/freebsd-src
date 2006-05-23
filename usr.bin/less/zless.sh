#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1.2.1 2005/06/20 07:12:48 des Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"
