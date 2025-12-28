#!/bin/sh
set -e

check="lib/libc/gen/uexterr_format.c"
target="lib/libc/gen/exterr_cat_filenames.h"

if [ \! -f "${check}" ] ; then
    echo "Script must be run from the top of the full source tree"
    exit 1
fi

echo "/*" >"${target}"
printf " * Automatically %sgenerated, use\\n" \@ >>"${target}"
echo " * tools/build/make_libc_exterr_cat_filenames.sh" >>"${target}"
echo " */" >>"${target}"

(find sys -type f -name '*.c' | \
    xargs grep -E '^#define[[:space:]]+EXTERR_CATEGORY[[:space:]]+EXTERR_CAT_' | \
    sed -E 's/[[:space:]]+/:/g' | \
    awk -F ':' '{filename = $1; sub(/^sys\//, "", filename);
        printf("\t[%s] = \"%s\",\n", $4, filename)}') \
    >>"${target}"
