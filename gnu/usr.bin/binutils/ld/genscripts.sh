#!/bin/sh
# genscripts.sh - generate the ld-emulation-target specific files
#
# Usage: genscripts.sh srcdir libdir host target target_alias \
# default_emulation native_lib_dirs this_emulation
#
# Sample usage:
# genscripts.sh /djm/ld-devo/devo/ld /usr/local/lib sparc-sun-sunos4.1.3 \
# sparc-sun-sunos4.1.3 sparc-sun-sunos4.1.3 sun4 "" sun3 sparc-sun-sunos4.1.3
# produces sun3.x sun3.xbn sun3.xn sun3.xr sun3.xu em_sun3.c
#
# $FreeBSD: src/gnu/usr.bin/binutils/ld/genscripts.sh,v 1.1.2.1 2000/07/06 22:15:52 obrien Exp $
#
# This is a cut-down version of the GNU script. Instead of jumping through
# hoops for all possible combinations of paths, just use the libdir
# argument in place of LIB_PATH.
#
# The host, target and target_alias arguments are not used in this version.
#

srcdir=$1
libdir=$2
host=$3
target=$4
target_alias=$5
EMULATION_LIBPATH=$6
NATIVE_LIB_DIRS=$7
EMULATION_NAME=$8

# Include the emulation-specific parameters:
. ${srcdir}/emulparams/${EMULATION_NAME}.sh

if test -d ldscripts; then
  true
else
  mkdir ldscripts
fi

# Set the library search path, for libraries named by -lfoo.
# If LIB_PATH is defined (e.g., by Makefile) and non-empty, it is used.
# Otherwise, the default is set here.
#
# The format is the usual list of colon-separated directories.
# To force a logically empty LIB_PATH, do LIBPATH=":".

LIB_SEARCH_DIRS=`echo ${libdir} | tr ':' ' ' | sed -e 's/\([^ ][^ ]*\)/SEARCH_DIR(\1);/g'`

# Generate 5 or 6 script files from a master script template in
# ${srcdir}/scripttempl/${SCRIPT_NAME}.sh.  Which one of the 5 or 6
# script files is actually used depends on command line options given
# to ld.  (SCRIPT_NAME was set in the emulparams_file.)
#
# A .x script file is the default script.
# A .xr script is for linking without relocation (-r flag).
# A .xu script is like .xr, but *do* create constructors (-Ur flag).
# A .xn script is for linking with -n flag (mix text and data on same page).
# A .xbn script is for linking with -N flag (mix text and data on same page).
# A .xs script is for generating a shared library with the --shared
#   flag; it is only generated if $GENERATE_SHLIB_SCRIPT is set by the
#   emulation parameters.

SEGMENT_SIZE=${SEGMENT_SIZE-${TARGET_PAGE_SIZE}}

# Determine DATA_ALIGNMENT for the 5 variants, using
# values specified in the emulparams/<emulation>.sh file or default.

DATA_ALIGNMENT_="${DATA_ALIGNMENT_-${DATA_ALIGNMENT-ALIGN(${SEGMENT_SIZE})}}"
DATA_ALIGNMENT_n="${DATA_ALIGNMENT_n-${DATA_ALIGNMENT_}}"
DATA_ALIGNMENT_N="${DATA_ALIGNMENT_N-${DATA_ALIGNMENT-.}}"
DATA_ALIGNMENT_r="${DATA_ALIGNMENT_r-${DATA_ALIGNMENT-}}"
DATA_ALIGNMENT_u="${DATA_ALIGNMENT_u-${DATA_ALIGNMENT_r}}"

LD_FLAG=r
DATA_ALIGNMENT=${DATA_ALIGNMENT_r}
DEFAULT_DATA_ALIGNMENT="ALIGN(${SEGMENT_SIZE})"
(. ${srcdir}/scripttempl/${SCRIPT_NAME}.sc) | sed -e '/^ *$/d' > \
  ldscripts/${EMULATION_NAME}.xr

LD_FLAG=u
DATA_ALIGNMENT=${DATA_ALIGNMENT_u}
CONSTRUCTING=" "
(. ${srcdir}/scripttempl/${SCRIPT_NAME}.sc) | sed -e '/^ *$/d' > \
  ldscripts/${EMULATION_NAME}.xu

LD_FLAG=
DATA_ALIGNMENT=${DATA_ALIGNMENT_}
RELOCATING=" "
(. ${srcdir}/scripttempl/${SCRIPT_NAME}.sc) | sed -e '/^ *$/d' > \
  ldscripts/${EMULATION_NAME}.x

LD_FLAG=n
DATA_ALIGNMENT=${DATA_ALIGNMENT_n}
TEXT_START_ADDR=${NONPAGED_TEXT_START_ADDR-${TEXT_START_ADDR}}
(. ${srcdir}/scripttempl/${SCRIPT_NAME}.sc) | sed -e '/^ *$/d' > \
  ldscripts/${EMULATION_NAME}.xn

LD_FLAG=N
DATA_ALIGNMENT=${DATA_ALIGNMENT_N}
(. ${srcdir}/scripttempl/${SCRIPT_NAME}.sc) | sed -e '/^ *$/d' > \
  ldscripts/${EMULATION_NAME}.xbn

if test -n "$GENERATE_SHLIB_SCRIPT"; then
  LD_FLAG=shared
  DATA_ALIGNMENT=${DATA_ALIGNMENT_s-${DATA_ALIGNMENT_}}
  CREATE_SHLIB=" "
  # Note that TEXT_START_ADDR is set to NONPAGED_TEXT_START_ADDR.
  (. ${srcdir}/scripttempl/${SCRIPT_NAME}.sc) | sed -e '/^ *$/d' > \
    ldscripts/${EMULATION_NAME}.xs
fi

for i in $EMULATION_LIBPATH ; do
  test "$i" = "$EMULATION_NAME" && COMPILE_IN=true
done
# Generate e${EMULATION_NAME}.c.
. ${srcdir}/emultempl/${TEMPLATE_NAME-generic}.em
