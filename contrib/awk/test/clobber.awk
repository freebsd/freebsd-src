BEGIN {
	print "000800" > "seq"
	close("seq")
	ARGV[1] = "seq"
	ARGC = 2
}

{ printf "%06d", $1 + 1 >"seq";
  printf "%06d", $1 + 1 }
# Date: Mon, 20 Jan 1997 15:14:06 -0600 (CST)
# From: Dave Bodenstab <emory!synet.net!imdave>
# To: bug-gnu-utils@prep.ai.mit.edu
# Subject: GNU awk 3.0.2 core dump
# Cc: arnold@gnu.ai.mit.edu
# 
# The following program produces a core file on my FreeBSD system:
# 
# bash$ echo 000800 >/tmp/seq
# bash$ gawk '{ printf "%06d", $1 + 1 >"/tmp/seq";
# 	      printf "%06d", $1 + 1 }' /tmp/seq                  
# 
# This fragment comes from mgetty+sendfax.
# 
# Here is the trace:
# 
# Script started on Mon Jan 20 15:09:04 1997
# bash$ gawk --version
# GNU Awk 3.0.2
# Copyright (C) 1989, 1991-1996 Free Software Foundation.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# bash$ gdb gawk
# GDB is free software and you are welcome to distribute copies of it
#  under certain conditions; type "show copying" to see the conditions.
# There is absolutely no warranty for GDB; type "show warranty" for details.
# GDB 4.13 (i386-unknown-freebsd), 
# Copyright 1994 Free Software Foundation, Inc...
# (gdb) shell echo 000800 >/tmp/seq
# (gdb) r '{ printf "%06d", $1 + 1 >"/tmp/seq"; printf "%06d", $1 + 1 }(gdb) r '{ printf "%06d", $1 + 1 >"/tmp/seq"; printf "%06d", $1 + 1 }' /tmp/seq
# Starting program: /scratch/archive/src/cmd/gnuawk-3.0.2/gawk '{ printf "%06d", $1 + 1 >"/tmp/seq"; printf "%06d", $1 + 1 }' /tmp/seq
# 
# Program received signal SIGBUS, Bus error.
# 0xd86f in def_parse_field (up_to=1, buf=0x37704, len=6, fs=0x3b240, rp=0x0, 
#     set=0xce6c <set_field>, n=0x0) at field.c:391
# 391		sav = *end;
# (gdb) bt
# #0  0xd86f in def_parse_field (up_to=1, buf=0x37704, len=6, fs=0x3b240, 
#     rp=0x0, set=0xce6c <set_field>, n=0x0) at field.c:391
# #1  0xddb1 in get_field (requested=1, assign=0x0) at field.c:669
# #2  0xc25d in r_get_lhs (ptr=0x3b9b4, assign=0x0) at eval.c:1339
# #3  0x9ab0 in r_tree_eval (tree=0x3b9b4, iscond=0) at eval.c:604
# #4  0xa5f1 in r_tree_eval (tree=0x3b9fc, iscond=0) at eval.c:745
# #5  0x4661 in format_tree (fmt_string=0x3e040 "%06d", n0=0, carg=0x3ba20)
#     at builtin.c:620
# #6  0x5beb in do_sprintf (tree=0x3b96c) at builtin.c:809
# #7  0x5cd5 in do_printf (tree=0x3ba8c) at builtin.c:844
# #8  0x9271 in interpret (tree=0x3ba8c) at eval.c:465
# #9  0x8ca3 in interpret (tree=0x3bbd0) at eval.c:308
# #10 0x8c34 in interpret (tree=0x3bc18) at eval.c:292
# #11 0xf069 in do_input () at io.c:312
# #12 0x12ba9 in main (argc=3, argv=0xefbfd538) at main.c:393
# (gdb) l
# 386			*buf += len;
# 387			return nf;
# 388		}
# 389	
# 390		/* before doing anything save the char at *end */
# 391		sav = *end;
# 392		/* because it will be destroyed now: */
# 393	
# 394		*end = ' ';	/* sentinel character */
# 395		for (; nf < up_to; scan++) {
# (gdb) print end
# $1 = 0x804d006 <Error reading address 0x804d006: No such file or directory>
# (gdb) print buf
# $2 = (char **) 0x37704
# (gdb) print *buf
# $3 = 0x804d000 <Error reading address 0x804d000: No such file or directory>
# (gdb) q
# The program is running.  Quit anyway (and kill it)? (y or n) y
# bash$ exit
# 
# Script done on Mon Jan 20 15:11:07 1997
# 
# Dave Bodenstab
# imdave@synet.net
