#!/usr/bin/perl
#
# Copyright (c) 1992, 1993
#        The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#        This product includes software developed by the University of
#        California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

# Process the command line
#
while ( $arg = shift @ARGV ) {
   if ( $arg =~ m/^-.*/ ) {
      push @args, $arg;
   } elsif ( $arg =~ m/\.m$/ ) {
      push @filenames, $arg;
   }
}

foreach $src ( @filenames ) {
   open(SRC, $src);
   $test = <SRC>;
   if ( $test =~ m/.*KOBJ.*/ ) {
	  push @obj, $src;
   } else {
	  push @dev, $src;
   }
}

$path = $0;
@z = split(/\//, $path);
$#z -= 1;
$path = join('/', @z);

if ( $#dev != -1 ) {
   push @x, "perl5";
   push @x, $path . "/makedevops.pl";
   foreach $i ( @args ) {
	  push @x, $i;
   }
   foreach $i ( @dev ) {
	 push @x, $i;
   }
   system(@x);
}

if ( $#obj != -1 ) {
   push @y, "perl5";
   push @y, $path . "/makeobjops.pl";
   foreach $i ( @args ) {
      push @y, $i;
   }
   foreach $i ( @obj ) {
	  push @y, $i;
   }
   system(@y);
}


