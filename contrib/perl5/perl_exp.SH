#!/bin/sh
#
# Written: Nov 1994 Wayne Scott <wscott@ichips.intel.com>
#
# Updated: 1997-8 Jarkko Hietaniemi <jhi@iki.fi>
#
# Create the export list for perl.
# Needed by AIX to do dynamic linking.
#
# This simple program relies on 'global.sym' and few other *.sym files
# and the *var*.h files being up to date with all of the global
# symbols that a dynamic link library might want to access.
#
# Most symbols have a Perl_ or PL_prefix because that's what embed.h
# sticks in front of them.
#
# AIX requires the list of external symbols (variables or functions)
# that are made available for another executable object file the import.
# The list is called the export file and it is a simple text file.
# The first line must be
#!
# That is, hash-bang, pound-shout, however you want to call it.
# The remainder of the file are the names of the symbols, one per line.
# The file is then given to the system loader (cc/xlc command line)
# as -bE:export.file.

case $CONFIG in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac

echo "Extracting perl.exp"

rm -f perl.exp
echo "#!" > perl.exp

# No compat3 since 5.004_50.
# No interp.sym since 5.005_03.
# perlio.sym will added later if needed.

syms="global.sym thread.sym"

sed -n '/^[A-Za-z]/ s/^/Perl_/p' $syms                 >> perl.exp

sed -n 's/^PERLVAR.*(G\([^[,]*\).*/PL_\1/p' perlvars.h >> perl.exp
sed -n 's/^PERLVAR.*(I\([^[,]*\).*/PL_\1/p' intrpvar.h >> perl.exp
sed -n 's/^PERLVAR.*(T\([^[,]*\).*/PL_\1/p' thrdvar.h  >> perl.exp

# 
# If we use the PerlIO abstraction layer, add its symbols.
#

if [ $useperlio = "define" ]
then
	grep '^[A-Za-z]' perlio.sym >> perl.exp
fi

#
# Extra globals not included above (including a few that might
# not actually be defined, but there's no harm in that).
#

cat >> perl.exp <<END
perl_init_i18nl10n
perl_init_i18nl14n
perl_new_collate
perl_new_ctype
perl_new_numeric
perl_set_numeric_local
perl_set_numeric_standard
perl_alloc
perl_construct
perl_destruct
perl_free
perl_parse
perl_run
perl_get_sv
perl_get_av
perl_get_hv
perl_get_cv
perl_call_argv
perl_call_pv
perl_call_method
perl_call_sv
perl_eval_pv
perl_eval_sv
perl_require_pv
cast_i32
cast_iv
cast_uv
END

case "$ccflags" in
*-DHIDEMYMALLOC*)
  cat >>perl.exp <<END
Mymalloc
Mycalloc
Myremalloc
Myfree
END
  ;;
esac

case "$ccflags" in
*-DEMBEDMYMALLOC*)
  cat >>perl.exp <<END
Perl_malloc
Perl_calloc
Perl_realloc
Perl_free
END
  ;;
esac

# The shebang line nicely sorts as the first one.
sort -o perl.exp -u perl.exp

# eof
