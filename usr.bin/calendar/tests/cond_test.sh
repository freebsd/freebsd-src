#!/bin/sh

SRCDIR="$(dirname "${0}")"; export SRCDIR

m4 "${SRCDIR}/../regress.m4" "${SRCDIR}/cond.sh" | sh
