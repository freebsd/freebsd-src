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
#	global.pl		7-Jul-97
#
sub getcwd {
	local($dir);
        chop($dir = `/bin/pwd`);
        $dir;
}
sub regexp {
	$_[0] =~ /[][.*\^\$+?|(){}\\]/;	# include regular expression ?
}
$com = $0;
$com =~ s/.*\///;
$usage = "usage:\t$com [-a][-r][-x] pattern\n\t$com -c [name]\n\t$com [-a] -f file\n";
$ENV{'PATH'} = '/bin:/usr/bin';
#
# options check
#
while ($ARGV[0] =~ /^-/) {
        $opt = shift;
	if ($opt =~ /a/) { $aflag = 1; }
	if ($opt =~ /c/) { $cflag = 1; }
	if ($opt =~ /f/) { $fflag = 1; }
	if ($opt =~ /r/) { $rflag = 1; }
	if ($opt =~ /x/) { $xflag = 1; }
}
# -f option is valid when it is only one except for -a and -x option
if ($fflag && ($cflag || $rflag)) {
	$fflag = 0;
}
# -c option is valid when it is only one
if ($cflag && ($aflag || $fflag || $rflag || $xflag)) {
	$cflag = 0;
}
if (@ARGV == 0) {
	die($usage) if (! $cflag);
}
if ($cflag && &regexp($ARGV[0])) {
	die "$com: regular expression not allowed with -c option.\n";
}
$ARGV[0] =~ s/^[ \t]+//;			# remove leading blanks
#
# get $dbpath and $root
#
local($dbpath, $root) = &getdbpath();
#
# recognize format version of GTAGS. 'format version record' is saved as a
# META record in GTAGS and GRTAGS. if 'format version record' is not found,
# it's assumed version 1.
	$support_version = 1;		# accept this format version
#
open(GTAGS, "btreeop -K ' __.VERSION' $dbpath/GTAGS |") || die "$com: GTAGS not found.\n";
$rec = <GTAGS>;
close(GTAGS);
if ($rec =~ /^ __\.VERSION[ \t]+([0-9]+)$/) {
	$format_version = $1;
} else {
	$format_version = 1;
}
if ($format_version > $support_version) {
	die "$com: GTAGS seems new format. Please install the latest GLOBAL.\n";
}
#
# complete function name
#
if ($cflag) {
	open(PIPEIN, "btreeop -L $dbpath/GTAGS |") || die "$com: btreeop cannot exec.\n";
	while (<PIPEIN>) {
		print if (@ARGV == 0 || $_ =~ /^$ARGV[0]/o);
	}
	close(PIPEIN);
	exit(0);
}
#
# make path filter.
#
if ($aflag) {
	@com = ("-e 's!\\.!$root!'");		# absolute
} else {
	@com = &relative_filter($root);		# relative
}
#
# print function definitions.
#
if ($fflag) {
	if (! -f $ARGV[0]) { die "$com: file '$ARGV[0]' not exist.\n"; }
	$path = &realpath($ARGV[0]);
	$path =~ s/^$root/./;
	chdir($root) || die "$com: cannot move to directory '$root'.\n";
	system("gctags -Dex '$path' | sort +1n -2 | sed @com");
	exit(0);
}
#
# search in current source tree.
#
$cnt = &search($ARGV[0], $dbpath, @com);
#
# search in library path.
#
if ($cnt == 0 && ! &regexp($ARGV[0]) && ! $rflag && defined($ENV{'GTAGSLIBPATH'})) {
	local($cwd) = &getcwd;
	foreach $lib (split(':', $ENV{'GTAGSLIBPATH'})) {
		next unless (-f "$lib/GTAGS");
		next if ($dbpath eq $lib);
		chdir($lib) || die "$com: cannot chdir to $lib.\n";
		$root = $dbpath = &getcwd;
		if ($aflag) {
			@com = ("-e 's!\\.!$root!'");
		} else {
			$common = &common($root, $cwd);
			$up = $root;
			$up =~ s/$common//;
			$down = $cwd;
			$down =~ s/$common//;
			$down =~ s![^/]+!..!g;
			next if ($down eq '' || $up eq '');
			@com = ("-e 's!\\./!$down/$up/!'");
		}
		$cnt = &search($ARGV[0], $dbpath, @com);
		last if ($cnt > 0);
	}
	chdir($cwd) || die "$com: cannot return current directory.\n";
}
exit(0);
#
# realpath: get absolute path name
#
#	r)	absolute path
#
sub realpath {
	local($path) = @_;
	local($dirname, $basename);
	if ($path =~ m!^(.*)/([^/]*)$!) {
		$dirname = $1;
		$basename = $2;
	} else {
		$dirname = '.';
		$basename = $path;
	}
	local($cwd) = &getcwd;
	chdir($dirname) || die "$com: cannot move to '$dirname'.\n";
	$path = &getcwd . '/' . $basename;
	chdir($cwd) || die "$com: cannot return to '$cwd'.\n";
	$path;
}
#
# getdbpath: get dbpath and root directory
#
#	r)	($dbpath, $root)
#
sub getdbpath {
	local($dbpath, $root);
	local($cwd) = &getcwd;

	if (defined($ENV{'GTAGSROOT'})) {
		$dbpath = $root = $ENV{'GTAGSROOT'};
		if (defined($ENV{'GTAGSDBPATH'})) {
			$dbpath = $ENV{'GTAGSDBPATH'};
		}
		$root =~ /^\// || die "$com: GTAGSROOT must be an absolute path.\n";
		$dbpath =~ /^\// || die "$com: GTAGSDBPATH must be an absolute path.\n";
		chdir($root) || die "$com: directory $root not found.\n";
		$root = &getcwd;
		chdir($cwd);
		chdir($dbpath) || die "$com: directory $dbpath not found.\n";
		$dbpath = &getcwd;
		if ($cwd !~ /^$root/) {
			die "$com: you must be under GTAGSROOT.\n";
		}
	}
	if (!$root) {
		local($gtags) = 'GTAGS';
		while (! -r $gtags && ! -r "obj/$gtags") {
			if (&getcwd =~ m!^/$!) { die "$com: $gtags not found.\n"; }
			chdir('..');
		}
		$dbpath = $root = &getcwd;
		$dbpath = "$dbpath/obj" if (! -r $gtags);
	}
	chdir($cwd) || die "$com: cannot return current directory.\n";
	($dbpath, $root);
}
#
# relative_filter: make relative path filter
#
#	i)	$root	the root directory of source tree
#	r)	@com	sed command list
#
sub relative_filter {
	local($root) = @_;
	local($cwd) = &getcwd;
	local($cur) = $cwd;

	$cur =~ s!$root!!;
	$cur =~ s!^/!!;
	local(@step) = split('/', $cur);
	local($downpath) = '\\.\\./' x @step;
	local(@com);
	push(@com, "-e 's!\\./!$downpath!'");
	foreach $step (@step) {
		push(@com, "-e 's!\\.\\./$step/!!'");
	}
	chdir($cwd) || die "$com: cannot return current directory.\n";
	@com;
}
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
#	i)	@com		sed's command
#	gi)	$xflag		-x option
#	gi)	$rflag		-r option
#	r)			count of output lines
#
sub search {
	local($pattern, $dbpath, @com) = @_;
	local($regex, $gtags, $outfilter, $infilter);
	#
	# make input filter
	#
	$gtags = ($rflag) ? 'GRTAGS' : 'GTAGS';
	if ($regex = &regexp($pattern)) {	# regular expression
		$infilter = "btreeop $dbpath/$gtags |";
	} else {
		$infilter = "btreeop -K '$pattern' $dbpath/$gtags |";
	}
	#
	# make output filter
	#	gtags fields is same to ctags -x format.
	#	0:tag, 1:lineno, 2:filename, 3: pattern.
	#
	if ($xflag) {
		$outfilter = "| sort +0 -1 +2 -3 +1n -2";
	} else {
		$outfilter = "| awk '{print \$3}' | sort | uniq";
	}
	$outfilter .= "| sed @com";
	open(PIPEIN, $infilter) || die "$com: database not found.\n";
	open(PIPEOUT, $outfilter) || die "$com: pipe cannot open.\n";
	local($cnt) = 0;
	while (<PIPEIN>) {
		local($tag) = split;
		if (! $regex || $tag =~ /$pattern/o) {
			$cnt++;
			print PIPEOUT $_;
		}
	}
	close(PIPEIN);
	close(PIPEOUT);
	$cnt;
}
