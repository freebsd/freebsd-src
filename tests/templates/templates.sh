#!/bin/bash

rm -f *.aml *.asl *.dsl *.log

files=`ls`

if [ "$1" == 1 ]; then
	ASL_COMPILER="../../generate/unix/bin/iasl"
else
	ASL_COMPILER="iasl"
fi

echo "Using $ASL_COMPILER"

#
# Create and compile the terse (normal) templates just
# to ensure that they will emit and compile
#
$ASL_COMPILER -T ALL > /dev/null 2>&1
$ASL_COMPILER *.asl > /dev/null 2>&1

rm -f *.aml *.asl *.dsl *.log

# Create the templates (use verbose mode)

$ASL_COMPILER -vt -T ALL > /dev/null 2>&1

# Compile the templates

$ASL_COMPILER *.asl > /dev/null 2>&1

# Disassemble the compiled templates

$ASL_COMPILER -d *.aml > /dev/null 2>&1

> diff.log

#
# Compare templates to compiled/disassembled templates
#
for f in $files ; do
    if [ "$f" != "$0" ] && [ "$f" != "Makefile" ]; then
        sig=`echo $f | awk -F. '{print $1}'`

        # Ignore differences in the comment/header field

        diff -pu -I" \*" $sig.asl $sig.dsl >> diff.log
    fi
done

