#!/bin/sh
# $FreeBSD$

SRCDIR="$(dirname "${0}")"; export SRCDIR

m4 "${SRCDIR}/../regress.m4" "${SRCDIR}/comment.sh" | sh
