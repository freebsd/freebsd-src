#!/usr/bin/perl
#
# Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
#	This product includes software developed by Shigio Yamaguchi.
# 4. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	gtags.pl	5-Jul-97
#
$com = $0;
$com =~ s/.*\///;
$usage = "usage: $com [-e][-s][dbpath]";
$ENV{'PATH'} = '/bin:/usr/bin';
#
# ctags flag
#
$eflag = $sflag = '';
while ($ARGV[0] =~ /^-/) {
	$opt = shift;
	if ($opt =~ /[^-es]/) { die "$usage\n"; }
	if ($opt =~ /e/) { $eflag = 'e'; }
	if ($opt =~ /s/) { $sflag = 's'; }
}
$dbpath = '.';
$dbpath = $ARGV[0] if ($ARGV[0]);
if (-f "$dbpath/GTAGS" && -f "$dbpath/GRTAGS") {
	if (! -w "$dbpath/GTAGS") {
		die "$com: cannot write to GTAGS.\n";
	} elsif (! -w "$dbpath/GRTAGS") {
		die "$com: cannot write to GRTAGS.\n";
	}
} elsif (! -w "$dbpath") {
	die "$com: cannot write to the directory '$dbpath'.\n"
}
#
# make global database
#
foreach $db ('GTAGS', 'GRTAGS') {
	# currently only *.c *.h *.y are supported.
	# *.s *.S is valid only when -s option specified.
	open(FIND, "find . -type f -name '*.[chysS]' -print |") || die "$com: cannot exec find.\n";
	open(DB, "|btreeop -C $dbpath/$db") || die "$com: cannot create db file '$dbpath/$db'.\n";
	while (<FIND>) {
		chop;
		next if /(y\.tab\.c|y\.tab\.h)$/;
		next if /(\/SCCS\/|\/RCS\/)/;
		next if (/\.[sS]$/ && (!$sflag || $db eq 'GRTAGS'));

		$flag = ($db eq 'GRTAGS') ? "${eflag}Dxr" : "${eflag}Dx";
		$ENV{'GTAGDBPATH'} = $dbpath;
		open(TAGS, "gctags -$flag $_ |") || die "$com: cannot read '$_'.\n";
		while (<TAGS>) {
			print DB;
		}
		close(TAGS);
	}
	close(DB);
	close(FIND);
}
exit 0;
