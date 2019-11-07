#
# $Id$
#
# Copyright 2019, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

XO=$1
shift

XOP="${XO} --warn"

# This is testing --wrap, --open, --close, --top-wrap, etc, so
# the output is not a single valid document

set -- 'The capital of {:state} is {:city}\n' 'North Carolina' Raleigh

${XOP} --top-wrap --open a/b/c "$@"
${XOP} --top-wrap --close a/b/c --not-first "$@"

${XOP} --top-wrap --wrap a/b/c "$@"

${XOP} --top-wrap --open a/b/c "$@"
${XOP} --depth 4 --not-first --wrap d/e/f "$@"
${XOP} --top-wrap --close a/b/c --not-first "$@"

${XOP} --wrap a/b/c "$@"

${XOP} --top-wrap --wrap a/b/c "$@"

${XOP} --top-wrap "test\n"

${XOP} --open answer
${XOP} "Answer:"
${XOP} --continuation "$@"
${XOP} --close answer

${XOP} --top-wrap --open top/data
${XOP} --depth 2 'First {:tag} ' value1
${XOP} --depth 2 --continuation 'and then {:tag}\n' value2
${XOP} --top-wrap --close top/data


${XOP} --help

${XOP} --open-list machine
NF=
for name in red green blue; do
    ${XOP} --depth 1 $NF --open-instance machine
    ${XOP} --depth 2 "Machine {k:name} has {:memory}\n" $name 55
    ${XOP} --depth 1 --close-instance machine
    NF=--not-first
done
${XOP} $NF --close-list machine
