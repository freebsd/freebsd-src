#!/bin/sh

run() {
	kind=$1
	echo Testing ${kind}
	buf_ring_test --cons-type=${kind} --prod-count=2 --buf-size=4
}

OBJDIR=$(make -V.OBJDIR)
export PATH=${OBJDIR}:${PATH}

run mc
run sc
run peek
run peek-clear
run mc-mt
