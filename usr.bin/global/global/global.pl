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
#	global.pl		21-Apr-97
#
sub getcwd {
	local($dir);
        chop($dir = `/bin/pwd`);
        $dir;
}
$com = $0;
$com =~ s/.*\///;
$usage = "usage:\t$com [-a][-r][-x] pattern\n\t$com -c [name]\n";
#
# options check
#
while ($ARGV[0] =~ /^-/) {
        $opt = shift;
	if ($opt =~ /a/) { $aflag = 1; }
	if ($opt =~ /c/) { $cflag = 1; }
	if ($opt =~ /r/) { $rflag = 1; }
	if ($opt =~ /x/) { $xflag = 1; }
}
if (@ARGV == 0) {
	die($usage) if (! $cflag);
}
$ARGV[0] =~ s/^[ \t]+//;			# remove leading blanks
if ($ARGV[0] =~ /[][.*\^\$+?|(){}\\]/) {	# include regular expression ?
	$regex = 1;
}
if ($cflag) {
	die($usage) if ($aflag || $rflag || $xflag);
	die("$com: regular expression not allowed with -c option.\n") if ($regex);
}
$current = &getcwd;
#
# get $dbpath and $root
#
if (defined($ENV{'GTAGSROOT'})) {
	$root = $ENV{'GTAGSROOT'};
	if (defined($ENV{'GTAGSDBPATH'})) {
		$dbpath = $ENV{'GTAGSDBPATH'};
	} else {
		$dbpath = $root;
	}
	unless ($current =~ /$root/) {
		die("$com: illegal GTAGSROOT.\n");
	}
	chdir($dbpath) || die("$com: directory $dbpath not found.\n");
	$dbpath = &getcwd;
	chdir($current);
	chdir($root) || die("$com: directory $root not found.\n");
	$root = &getcwd;
}
chdir($current) || die("$com: cannot return current directory.\n");
$gtagsname = ($rflag) ? 'GRTAGS' : 'GTAGS';
#
# make a sed command to make paths into relative
#
if (! defined($root)) {
	chdir($current);
	while (! -r $gtagsname && ! -r "obj/$gtagsname") {
		if (&getcwd =~ m!^/$!) { die "$com: $gtagsname not found.\n"; }
		chdir('..');
	}
	$dbpath = $root = &getcwd;
	$dbpath = "$dbpath/obj" if (! -r $gtagsname);
}
$cur = $current;
$cur =~ s!$root!!;
$cur =~ s!^/!!;
@step = split('/', $cur);
$downpath = '\\.\\./' x @step;
push(@com, "-e 's!\\./!$downpath!'");
foreach $step (@step) {
	push(@com, "-e 's!\\.\\./$step/!!'");
}
#
# recognize format version of GTAGS. 'format version record' is saved as a
# META record in GTAGS and GRTAGS. if 'format version record' is not found,
# it's assumed version 1.
	$support_version = 1;		# accept this format version
#
open(GTAGS, "btreeop -K ' __.VERSION' $dbpath/$gtagsname |") || die("$com: GTAGS not found.\n");
$rec = <GTAGS>;
close(GTAGS);
if ($rec =~ /^ __\.VERSION[ \t]+([0-9]+)$/) {
	$format_version = $1;
} else {
	$format_version = 1;
}
if ($format_version > $support_version) {
	die("$com: GTAGS seems new format. Please install the latest GLOBAL.\n");
}
#
# complete function name
#
if ($cflag) {
	open(PIPEIN, "btreeop $dbpath/GTAGS | awk '{print \$1}' | sort | uniq |") || die("$com: btreeop cannot exec.\n");
	while (<PIPEIN>) {
		print if (@ARGV == 0 || $_ =~ /^$ARGV[0]/o);
	}
	close(PIPEIN);
	exit(0);
}
#
# search in current source tree.
#
$cnt = &search($ARGV[0], $dbpath, $gtagsname, @com);
#
# search in library path.
#
if ($cnt == 0 && ! $regex && ! $rflag && defined($ENV{'GTAGSLIBPATH'})) {
	foreach $lib (split(':', $ENV{'GTAGSLIBPATH'})) {
		next unless (-f "$lib/GTAGS");
		next if ($dbpath eq $lib);
		chdir($lib) || die("$com: cannot chdir to $lib.\n");
		$dbpath = &getcwd;
		$common = &common($dbpath, $current);
		$up = $dbpath;
		$up =~ s/$common//;
		$down = $current;
		$down =~ s/$common//;
		$down =~ s![^/]+!..!g;
		next if ($down eq '' || $up eq '');
		$cnt = &search($ARGV[0], $dbpath, 'GTAGS', ("-e 's!\\./!$down/$up/!'"));
		last if ($cnt > 0);
	}
}
exit(0);
#
# common: extract a common part of two paths.
#
#	i)	$p1, $p2	paths
#	r)			common part
#
sub common {
	local($p1, $p2) = @_;
	local(@p1, @p2, @common, $common);

	@p1 = split('/', $p1);
	@p2 = split('/', $p2);
	while (@p1 && @p2 && $p1[0] eq $p2[0]) {
		push(@common, shift @p1);
		shift @p2;
	}
	$common = join('/', @common);
	$common .= '/';
	$common;
}
#
# search: search specified function 
#
#	i)	$pattern	search pattern
#	i)	$dbpath		where GTAGS exist
#	i)	$gtagsname	gtags name (GTAGS or GRTAGS)
#	i)	@com		sed's command
#	gi)	$xflag		-x option
#	gi)	$rflag		-r option
#	gi)	$regex		regular expression
#	r)			count of output lines
#
sub search {
	local($pattern, $dbpath, $gtagsname, @com) = @_;
	local($cnt);
	#
	# make input filter
	#
	if ($regex) {					# regular expression
		$infilter = "btreeop $dbpath/$gtagsname |";
	} else {
		$infilter = "btreeop -K '$pattern' $dbpath/$gtagsname |";
	}
	#
	# make output filter
	#	gtags fields is same to ctags -x format.
	#	0:tag, 1:lineno, 2:filename, 3: pattern.
	#
	if ($xflag) {
		$outfilter = "| sort +0b -1 +2b -3 +1n -2";
	} else {
		$outfilter = "| awk '{print \$3}' | sort | uniq";
	}
	#
	# if absolute path needed
	#
	if ($aflag) {
		@com = ("-e 's!\\.!$dbpath!'");
	}
	$outfilter .= "| sed @com";
	open(PIPEIN, $infilter) || die("$com: database not found.\n");
	open(PIPEOUT, $outfilter) || die("$com: pipe cannot open.\n");
	$cnt = 0;
	while (<PIPEIN>) {
		($tag) = split;
		if (! $regex || $tag =~ /$pattern/o) {
			$cnt++;
			print PIPEOUT $_;
		}
	}
	close(PIPEIN);
	close(PIPEOUT);
	$cnt;
}
