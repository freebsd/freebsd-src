# hints/umips.sh
# 
# Mips R3030 / Bruker AspectSation  running RISC/os (UMIPS) 4.52
# compiling with gcc 2.7.2
#
# Created Sat Aug 17 00:17:15 MET DST 1996
# by Guenter Schmidt  <gsc@bruker.de> 
#
# uname -a output looks like this:
# 	xxx xxx 4_52 umips mips

# Speculative notes on getting cc to work added by
# Andy Dougherty	<doughera@lafcol.lafayette.edu>
# Tue Aug 20 21:51:49 EDT 1996
    
# Recommend the GNU C Compiler
case "$cc" in 
'')	echo 'gcc 2.7.2 (or later) is recommended.  Use Configure -Dcc=gcc' >&4
	# The test with the native compiler not succeed:
	# `sh  cflags libperl.a miniperlmain.o`  miniperlmain.c
	#  CCCMD =  cc -c -I/usr/local/include -I/usr/include/bsd -DLANGUAGE_C -O   
	# ccom: Error: ./mg.h, line 12: redeclaration of formal parameter, sv
	# 	  int           (*svt_set)       (SV *sv, MAGIC* mg);
	#       ------------------------------------------^
	# ccom: Error: ./mg.h, line 12: redeclaration of formal parameter, mg
	# This is probably a result of incomplete prototype support.
	prototype=undef
	;;
esac

#  POSIX support in RiscOS is not useable
useposix='false'

# Will give WHOA message, but the prototype are defined in the GCC inc dirs
case "$cc" in
*gcc*) d_shmatprototype='define' ;;
esac

glibpth="$glibpth /usr/lib/cmplrs/cc"
