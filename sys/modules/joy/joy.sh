#!/bin/sh
# $Id$

modload -q -u -A `sysctl -n kern.bootfile` /lkm/joy_mod.o
