#! /usr/bin/bash -norc
# hints/posix-bc.sh
#
# BS2000 (Posix Subsystem) hints by Thomas Dorner <Thomas.Dorner@start.de>
#
#  thanks to the authors of the os390.sh
#

# To get ANSI C, we need to use c89, and ld does not exist
# You can override this with Configure -Dcc=gcc -Dld=ld.
case "$cc" in
'') cc='c89' ;;
esac
case "$ld" in
'') ld='c89' ;;
esac

# C-Flags:
# -DPOSIX_BC
# -DUSE_PURE_BISON
# -D_XOPEN_SOURCE_EXTENDED alters system headers.
# Prepend your favorites with Configure -Dccflags=your_favorites
case "$ccflags" in
'') ccflags='-K enum_long,llm_case_lower,llm_keep,no_integer_overflow -DPOSIX_BC -DUSE_PURE_BISON -D_XOPEN_SOURCE_EXTENDED' ;;
*) ccflags='$ccflags -Kenum_long,llm_case_lower,llm_keep,no_integer_overflow -DPOSIX_BC -DUSE_PURE_BISON -D_XOPEN_SOURCE_EXTENDED' ;;
esac

# ccdlflags have yet to be determined.
#case "$ccdlflags" in
#'') ccdlflags='-c' ;;
#esac

# cccdlflags have yet to be determined.
#case "$cccdlflags" in
#'') cccdlflags='' ;;
#esac

# ldflags have yet to be determined.
#case "$ldflags" in
#'') ldflags='' ;;
#esac

# lddlflags have yet to be determined.
#case "$lddlflags" in
#'') lddlflags='' ;;
#esac

# Flags on a RISC-Host (SUNRISE):
if [ -n "`bs2cmd SHOW-SYSTEM-INFO | egrep 'HSI-ATT.*TYPE.*SR'`" ]; then
    echo
    echo "Congratulations, you are running a machine with Sunrise CPUs."
    echo "Let's hope you have the matching RISC compiler as well."
    ccflags="-K risc_4000 $ccflags"
    ldflags='-K risc_4000'
fi

# Turning on optimization breaks perl (CORE-DUMP):
# You can override this with Configure -Doptimize='-O' or somesuch.
case "$optimize" in
'') optimize='none' ;;
esac

# we don''t use dynamic memorys (yet):
case "$so" in
'') so='none' ;;
esac

case "$usemymalloc" in
'') usemymalloc='n' ;;
esac

# On BS2000/Posix, libc.a does not really hold anything at all,
# so running nm on it is pretty useless.
# You can override this with Configure -Dusenm.
case "$usenm" in
'') usenm='false' ;;
esac

# Dynamic loading doesn't work on OS/390 quite yet.
# You can override this with
#  Configure -Dusedl -Ddlext=.so -Ddlsrc=dl_dllload.xs.
case "$usedl" in
'') usedl='n' ;;
esac
case "$dlext" in
'') dlext='none' ;;
esac
#case "$dlsrc" in
#'') dlsrc='none' ;;
#esac
#case "$ldlibpthname" in
#'') ldlibpthname=LIBPATH ;;
#esac

