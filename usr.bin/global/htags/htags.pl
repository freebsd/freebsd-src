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
#	htags.pl	5-Apr-97
#
$com = $0;
$com =~ s/.*\///;
$usage = "usage: $com [-a][-v][-w][-t title][-d tagdir][dir]";
#-------------------------------------------------------------------------
# CONFIGURATION
#-------------------------------------------------------------------------
# columns of line number
$ncol = 4;
# font
$comment_begin  = '<I><FONT COLOR=green>';	# /* ... */
$comment_end    = '</FONT></I>';
$sharp_begin    = '<FONT COLOR=darkred>';	# #define, #include or so on
$sharp_end      = '</FONT>';
$brace_begin    = '<FONT COLOR=blue>';		# { ... }
$brace_end      = '</FONT>';
$reserved_begin = '<B>';			# if, while, for or so on
$reserved_end   = '</B>';
# reserved words
$reserved_words = "auto|break|case|char|continue|default|do|double|else|extern|float|for|goto|if|int|long|register|return|short|sizeof|static|struct|switch|typedef|union|unsigned|void|while";
# temporary directory
$tmp = '/tmp';
#-------------------------------------------------------------------------
# DEFINITION
#-------------------------------------------------------------------------
# unit for a path
$SEP    = ' ';	# source file path must not include $SEP charactor
$ESCSEP = &escape($SEP);
$SRCS   = 'S';
$DEFS   = 'D';
$REFS   = 'R';
$FILES  = 'files';
$FUNCS  = 'funcs';
#-------------------------------------------------------------------------
# JAVASCRIPT PARTS
#-------------------------------------------------------------------------
# escaped angle
$langle  = sprintf("unescape('%s')", &escape('<'));
$rangle  = sprintf("unescape('%s')", &escape('>'));
# frame name
$f_mains = 'mains';		# for main view
$f_funcs = 'funcs';		# for function index
$f_files = 'files';		# for file index
$begin_script="<SCRIPT LANGUAGE=javascript>\n<!--\n";
$end_script="<!-- end of script -->\n</SCRIPT>\n";
$defaultview=
	"// if your browser doesn't support javascript, write a BASE tag statically.\n" .
	"if (parent.frames.length)\n" .
	"	document.write($langle+'BASE TARGET=$f_mains'+$rangle)\n";
$rewrite_href_funcs =
	"// IE3.0 seems to be not able to treat following code.\n" .
	"if (parent.frames.length && parent.$f_funcs == self) {\n" .
	"	document.links[0].href = '../funcs.html';\n" .
	"	document.links[document.links.length - 1].href = '../funcs.html';\n" .
	"}\n";
$rewrite_href_files =
	"// IE3.0 seems to be not able to treat following code.\n" .
	"if (parent.frames.length && parent.$f_files == self) {\n" .
	"       document.links[0].href = '../files.html';\n" .
	"       document.links[document.links.length - 1].href = '../files.html';\n" .
	"}\n";
#-------------------------------------------------------------------------
# UTIRITIES
#-------------------------------------------------------------------------
sub getcwd {
        local($dir) = `/bin/pwd`;
        chop($dir);
        $dir;
}
sub date {
	local($date) = `date`;
	chop($date);
	$date;
}
sub error {
	local($msg) = @_;
	&clean();
	die($msg);
}
sub clean {
	&anchor'finish();
	&cache'close();
}
sub escape {
	local($c) = @_;
	'%' . sprintf("%x", ord($c));
}
#-------------------------------------------------------------------------
# PROCESS START
#-------------------------------------------------------------------------
#
# options check
#
$aflag = $vflag = $wflag = $sflag = '';	# $sflag is set internally
while ($ARGV[0] =~ /^-/) {
	$opt = shift;
	if ($opt =~ /[^-avwdt]/) {
		print STDERR "$usage\n";
		exit 1;
	}
	if ($opt =~ /a/) { $aflag = 1; }
	if ($opt =~ /v/) { $vflag = 1; }
	if ($opt =~ /w/) { $wflag = 1; }
	if ($opt =~ /t/) {
		$opt = shift;
		last if ($opt eq '');
		$title = $opt;
	} elsif ($opt =~ /d/) {
		$opt = shift;
		last if ($opt eq '');
		$dbpath = $opt;
	}
}
if (!$title) {
	@cwd = split('/', &getcwd);
	$title = $cwd[$#cwd];
}
if (!$dbpath) {
	$dbpath = '.';
}
unless (-r "$dbpath/GTAGS" && -r "$dbpath/GRTAGS") {
	&error("GTAGS and GRTAGS not found. please type 'gtags[RET]'\n");
}
$html = &getcwd() . '/HTML';
if ($ARGV[0]) {
	$cwd = &getcwd();
	unless (-w $ARGV[0]) {
		 &error("$ARGV[0] is not writable directory.\n");
	}
	chdir($ARGV[0]) || &error("directory $ARGV[0] not found.\n");
	$html = &getcwd() . '/HTML';
	chdir($cwd) || &error("cannot return directory.\n");
}
#
# set sflag if *.[sS] are included.
#
open(CHECK, "btreeop $dbpath/GTAGS |") || &error("btreeop $dbpath/GTAGS failed.\n");
while (<CHECK>) {
	local($tag, $lno, $filename) = split;
	if ($filename =~ /\.[sS]$/) {
		$'sflag = 1;
		last;
	}
}
close(CHECK);
#-------------------------------------------------------------------------
# MAKE FILES
#-------------------------------------------------------------------------
#	HTML/help.html		... help file (2)
#	HTML/funcs.html		... function index (3)
#	HTML/$FUNCS/*		... function index (3)
#	HTML/$REFS/*		... referencies (4)
#	HTML/$DEFS/*		... definitions (4)
#	HTML/files.html		... file index (5)
#	HTML/$FILES/*		... file index (5)
#	HTML/index.html		... index file (6)
#	HTML/mains.html		... main index (7)
#	HTML/$SRCS/		... source files (8)
#-------------------------------------------------------------------------
print STDERR "[", &date, "] ", "Htags started\n" if ($vflag);
#
# (1) make directories
#
print STDERR "[", &date, "] ", "(1) making directories ...\n" if ($vflag);
mkdir($html, 0777) || &error("cannot make directory <$html>.\n") if (! -d $html);
foreach $d ($SRCS, $REFS, $DEFS, $FILES, $FUNCS) {
	mkdir("$html/$d", 0775) || &error("cannot make HTML directory\n") if (! -d "$html/$d");
}
#
# (2) make help file
#
print STDERR "[", &date, "] ", "(2) making help.html ...\n" if ($vflag);
&makehelp("$html/help.html");
#
# (3) make function index (funcs.html and $FUNCS/*)
#     PRODUCE @funcs
#
print STDERR "[", &date, "] ", "(3) making function index ...\n" if ($vflag);
$func_total = &makefuncindex("$html/funcs.html");
print STDERR "Total $func_total functions.\n" if ($vflag);
#
# (4) make function entries ($DEFS/* and $REFS/*)
#     MAKING TAG CACHE
#
print STDERR "[", &date, "] ", "(4) making duplicate entries ...\n" if ($vflag);
sub suddenly { &clean(); exit 1}
$SIG{'INT'} = 'suddenly';
$SIG{'QUIT'} = 'suddenly';
$SIG{'TERM'} = 'suddenly';
&cache'open(100000);
$func_total = &makedupindex($func_total);
print STDERR "Total $func_total functions.\n" if ($vflag);
#
# (5) make file index (files.html and $FILES/*)
#     PRODUCE @files
#
print STDERR "[", &date, "] ", "(5) making file index ...\n" if ($vflag);
$file_total = &makefileindex("$html/files.html");
print STDERR "Total $file_total files.\n" if ($vflag);
#
# [#] make a common part for mains.html and index.html
#     USING @funcs @files
#
print STDERR "[", &date, "] ", "(#) making a common part ...\n" if ($vflag);
$index = &makecommonpart($title);
#
# (6)make index file (index.html)
#
print STDERR "[", &date, "] ", "(6) making index file ...\n" if ($vflag);
&makeindex("$html/index.html", $title, $index);
#
# (7) make main index (mains.html)
#
print STDERR "[", &date, "] ", "(7) making main index ...\n" if ($vflag);
&makemainindex("$html/mains.html", $index);
#
# (#) make anchor database
#
print STDERR "[", &date, "] ", "(#) making temporary database ...\n" if ($vflag);
&anchor'create();
#
# (8) make HTML files ($SRCS/*)
#     USING TAG CACHE
#
print STDERR "[", &date, "] ", "(8) making hypertext from source code ...\n" if ($vflag);
&makehtml($file_total);
&clean();
print STDERR "[", &date, "] ", "Done.\n" if ($vflag);
exit 0;
#-------------------------------------------------------------------------
# SUBROUTINES
#-------------------------------------------------------------------------
#
# makehelp: make help file
#
sub makehelp {
	local($file) = @_;

	open(HELP, ">$file") || &error("cannot make help file.\n");
	print HELP "<HTML>\n<HEAD><TITLE>HELP</TITLE></HEAD>\n<BODY>\n";
	print HELP "<H2>Usage of Links</H2>\n";
	print HELP "<PRE>/* [&lt;][&gt;][^][v] [top][bottom][index][help] */</PRE>\n";
	print HELP "<DL>\n";
	print HELP "<DT>[&lt;]<DD>Previous function.\n";
	print HELP "<DT>[&gt;]<DD>Next function.\n";
	print HELP "<DT>[^]<DD>First function in this file.\n";
	print HELP "<DT>[v]<DD>Last function in this file.\n";
	print HELP "<DT>[top]<DD>Top of this file.\n";
	print HELP "<DT>[bottom]<DD>Bottom of this file.\n";
	print HELP "<DT>[index]<DD>Return to index page (mains.html).\n";
	print HELP "<DT>[help]<DD>You are seeing now.\n";
	print HELP "</DL>\n";
	print HELP "</BODY>\n</HTML>\n";
	close(HELP);
}
#
# makefuncindex: make function index (including alphabetic index)
#
#	i)	file		function index file
#	go)	@funcs
#
sub makefuncindex {
	local($file) = @_;
	local($count) = 0;

	open(FUNCTIONS, ">$file") || &error("cannot make function index <$file>.\n");
	print FUNCTIONS "<HTML>\n<HEAD><TITLE>FUNCTION INDEX</TITLE>\n";
	print FUNCTIONS "$begin_script$defaultview$end_script</HEAD>\n<BODY>\n";
	print FUNCTIONS "<H2>FUNCTION INDEX</H2>\n";
	print FUNCTIONS "<OL>\n" if (!$aflag);
	local($old) = select(FUNCTIONS);
	open(TAGS, "btreeop $dbpath/GTAGS | awk '{print \$1}' | sort | uniq |") || &error("btreeop $dbpath/GTAGS failed.\n");
	local($alpha) = '';
	@funcs = ();	# [A][B][C]...
	while (<TAGS>) {
		$count++;
		chop;
		local($tag) = $_;
		print STDERR " [$count] adding $tag\n" if ($vflag);
		if ($aflag && $alpha ne substr($tag, 0, 1)) {
			if ($alpha) {
				print ALPHA "</OL>\n";
				print ALPHA "<A HREF=../mains.html TARGET=_self>[index]</A>\n";
				print ALPHA "$begin_script$rewrite_href_funcs$end_script";
				print ALPHA "</BODY>\n</HTML>\n";
				close(ALPHA);
			}
			$alpha = substr($tag, 0, 1);
			push(@funcs, "<A HREF=$FUNCS/$alpha.html TARGET=_self>[$alpha]</A>\n");
			open(ALPHA, ">$html/$FUNCS/$alpha.html") || &error("cannot make alphabetical function index.\n");
			print ALPHA "<HTML>\n<HEAD><TITLE>$alpha</TITLE>\n";
			print ALPHA "$begin_script$defaultview$end_script";
			print ALPHA "</HEAD>\n<BODY>\n<H2>[$alpha]</H2>\n";
			print ALPHA "<A HREF=../mains.html TARGET=_self>[index]</A>\n";
			print ALPHA "<OL>\n";
			select(ALPHA);
		}
		open(LIST, "btreeop -K $tag $dbpath/GTAGS |") || &error("btreeop -K $tag $dbpath/GTAGS failed.\n");;
		local($line1, $line2);
		if ($line1 = <LIST>) {
			$line2 = <LIST>;
		}
		close(LIST);
		if ($line2) {
			print "<LI><A HREF=", ($aflag) ? "../" : "", "D/$tag.html>$tag</A>\n";
		} else {
			local($nouse, $lno, $filename) = split(/[ \t]+/, $line1);
			$nouse = '';	# to make perl quiet
			$filename =~ s/^\.\///;
			$filename =~ s/\//$ESCSEP/g;
			print "<LI><A HREF=", ($aflag) ? "../" : "", "$SRCS/$filename.html#$lno>$tag</A>\n";
		}
		close(LIST);
	}
	close(TAGS);
	select($old);
	if ($aflag) {
		print ALPHA "</OL>\n";
		print ALPHA "<A HREF=../mains.html TARGET=_self>[index]</A>\n";
		print ALPHA "$begin_script$rewrite_href_funcs$end_script";
		print ALPHA "</BODY>\n</HTML>\n";
		close(ALPHA);

		print FUNCTIONS @funcs;
	}
	print FUNCTIONS "</OL>\n" if (!$aflag);
	print FUNCTIONS "</BODY>\n</HTML>\n";
	close(FUNCTIONS);
	$count;
}
#
# makedupindex: make duplicate entries index ($DEFS/* and $REFS/*)
#
#	i)	$total	functions total
#	r)	$count
#
sub makedupindex {
	local($total) = @_;
	local($count) = 0;

	open(TAGS, "btreeop $dbpath/GTAGS | awk '{print \$1}' | sort | uniq |") || &error("btreeop $dbpath/GTAGS failed.\n");
	while (<TAGS>) {
		$count++;
		chop;
		local($tag) = $_;
		print STDERR " [$count/$total] adding $tag\n" if ($vflag);
		foreach $db ('GTAGS', 'GRTAGS') {
			open(LIST, "btreeop -K $tag $dbpath/$db | sort +0b -1 +2b -3 +1n -2|") || &error("btreeop -K $tag $dbpath/$db failed.\n");;
			local($line1, $line2);
			if ($line1 = <LIST>) {
				$line2 = <LIST>;
			}
			&cache'put($db, $tag, ($line2) ? '' : $line1) if ($line1);
			if ($line2) {		# two or more entries exist
				local($type) = ($db eq 'GTAGS') ? $'DEFS : $'REFS;
				open(FILE, ">$html/$type/$tag.html") || &error("cannot make file <$html/$type/$tag.html>.\n");
				print FILE "<HTML>\n<HEAD><TITLE>$tag</TITLE></HEAD>\n<BODY>\n";
				print FILE "<PRE>\n";
				for (;;) {
					if ($line1) {
						$_ = $line1;
						$line1 = '';
					} elsif ($line2) {
						$_ = $line2;
						$line2 = '';
					} elsif (!($_ = <LIST>)) {
						last;
					}
					s/\.\///;
					s/&/&amp;/g;
					s/</&lt;/g;
					s/>/&gt;/g;
					local($nouse, $lno, $filename) = split;
					$nouse = '';	# to make perl quiet
					$filename =~ s/\//$ESCSEP/g;
					s/^$tag/<A HREF=..\/$SRCS\/$filename.html#$lno>$tag<\/A>/;
					print FILE;
				}
				print FILE "</PRE>\n</BODY>\n</HTML>\n";
				close(FILE);
			}
			close(LIST);
		}
	}
	close(TAGS);
	$count;
}
#
# makefileindex: make file index
#
#	i)	file name
#	go)	@files
#
sub makefileindex {
	local($file) = @_;
	local($count) = 0;

	open(FILES, ">$file") || &error("cannot make file <$file>.\n");
	print FILES "<HTML>\n<HEAD><TITLE>FILES</TITLE>\n";
	print FILES "$begin_script$defaultview$end_script";
	print FILES "</HEAD>\n<BODY>\n<H2>FILE INDEX</H2>\n";
	print FILES "<OL>\n";
	local($old) = select(FILES);
	open(FIND, "find . -name '*.[chysS]' -print | sort |") || &error("cannot exec find.\n");
	local($lastdir) = '';
	@files = ();
	while (<FIND>) {
		$count++;
		chop;
		s/^\.\///;
		next if /(y\.tab\.c|y\.tab\.h)$/;
		next if (!$'sflag && /\.[sS]$/);
		local($filename) = $_;
		print STDERR " [$count] adding $filename\n" if ($vflag);
		local($dir);
		if (index($filename, '/') >= 0) {
			@split = split('/');
			$dir = $split[0];
		} else {
			$dir = '';
		}
		#if ($dir && $dir ne $lastdir) {
		if ($dir ne $lastdir) {
			if ($lastdir) {
				print DIR "</OL>\n";
				print DIR "<A HREF=../mains.html TARGET=_self>[index]</A>\n";
				print DIR "$begin_script$rewrite_href_files$end_script";
				print DIR "</BODY>\n</HTML>\n";
				close(DIR);
			}
			if ($dir) {
				push(@files, "<LI><A HREF=$FILES/$dir.html TARGET=_self>$dir/</A>\n");
				open(DIR, ">$html/$FILES/$dir.html") || &error("cannot make directory index.\n");
				print DIR "<HTML>\n<HEAD><TITLE>$dir/</TITLE>\n";
				print DIR "$begin_script$defaultview$end_script";
				print DIR "</HEAD>\n<BODY>\n<H2>$dir/</H2>\n";
				print DIR "<A HREF=../mains.html TARGET=_self>[index]</A>\n";
				print DIR "<OL>\n";
			}
			$lastdir = $dir;
		}
		local($path) = $filename;
		$path =~ s/\//$ESCSEP/g;
		if ($dir eq '') {
			push(@files, "<LI><A HREF=", ($dir) ? "../" : "", "$SRCS/$path.html>$filename</A>\n");
		} else {
			print DIR "<LI><A HREF=../$SRCS/$path.html>$filename</A>\n";
		}
	}
	close(FIND);
	select($old);
	if ($lastdir) {
		print DIR "</OL>\n";
		print DIR "<A HREF=../mains.html TARGET=_self>[index]</A>\n";
		print DIR "$begin_script$rewrite_href_files$end_script";
		print DIR "</BODY>\n</HTML>\n";
		close(DIR);
	}
	print FILES @files;
	print FILES "</OL>\n";
	print FILES "</BODY>\n</HTML>\n";
	close(FILES);

	$count;
}
#
# makecommonpart: make a common part for mains.html and index.html
#
#	gi)	@files
#	gi)	@funcs
#
sub makecommonpart {
	local($title) = @_;
	local($index) = '';

	$index .= "<H1><FONT COLOR=#cc0000>$title</FONT></H1>\n";
	$index .= "<P ALIGN=right>";
	$index .= "Last updated " . &date . "<BR>\n";
	$index .= "This hypertext was generated by <A HREF=http://wafu.netgate.net/tama/unix/indexe.html#global TARGET=_top>GLOBAL</A>.<BR>\n";
	$index .= "$begin_script";
	$index .= "if (parent.frames.length && parent.$f_mains == self)\n";
	$index .= "	document.write($langle+'A HREF=mains.html TARGET=_top'+$rangle+'[No frame version is here.]'+$langle+'/A'+$rangle)\n";
	$index .= "$end_script";
	$index .= "</P>\n<HR>\n";
	$index .= "<H2>MAINS</H2>\n";
	$index .= "<PRE>\n";
	open(PIPE, "btreeop -K main $dbpath/GTAGS | sort +0b -1 +2b -3 +1n -2 |") || &error("btreeop -K main $dbpath/GTAGS failed.\n");
	while (<PIPE>) {
		local($nouse, $lno, $filename) = split;
		$nouse = '';	# to make perl quiet
		$filename =~ s/^\.\///;
		$filename =~ s/\//$ESCSEP/g;
		s/(main)/<A HREF=$SRCS\/$filename.html#$lno>$1<\/A>/;
		$index .= $_;
	}
	close(PIPE);
	$index .= "</PRE>\n<HR>\n<H2>FUNCTIONS</H2>\n";
	if ($aflag) {
		foreach $f (@funcs) {
			$index .= $f;
		}
	} else {
		$index .= "<PRE><A HREF=funcs.html>function index</A></PRE>\n";
	}
	$index .= "<HR>\n<H2>FILES</H2>\n";
	$index .= "<OL>\n";
	foreach $f (@files) {
		$index .= $f;
	}
	$index .= "</OL>\n<HR>\n";
	$index;
}
#
# makeindex: make index file
#
#	i)	$file	file name
#	i)	$title	title of index file
#	i)	$index	common part
#
sub makeindex {
	local($file, $title, $index) = @_;

	open(FRAME, ">$file") || &error("cannot open file <$file>.\n");
	print FRAME "<HTML>\n<HEAD><TITLE>$title</TITLE></HEAD>\n";
	print FRAME "<FRAMESET COLS='200,*'>\n";
	print FRAME "<NOFRAME>\n$index</NOFRAME>\n";
	print FRAME "<FRAMESET ROWS='50%,50%'>\n";
	print FRAME "<FRAME NAME=$f_funcs SRC=funcs.html>\n";
	print FRAME "<FRAME NAME=$f_files SRC=files.html>\n";
	print FRAME "</FRAMESET>\n";
	print FRAME "<FRAME NAME=$f_mains SRC=mains.html>\n";
	print FRAME "</FRAMESET>\n";
	print FRAME "</HTML>\n";
	close(FRAME);
}
#
# makemainindex: make main index
#
#	i)	$file	file name
#	i)	$index	common part
#
sub makemainindex {
	local($file, $index) = @_;

	open(INDEX, ">$file") || &error("cannot create file <$file>.\n");
	print INDEX "<HTML>\n<HEAD><TITLE>MAINS</TITLE></HEAD>\n";
	print INDEX "<BODY>\n$index</BODY>\n</HTML>\n";
	close(INDEX);
}
#
# makehtml: make html files
#
sub makehtml {
	local($total) = @_;
	local($count) = 0;

	open(FIND, "find . -name '*.[chysS]' -print|") || &error("cannot exec find.\n");
	while (<FIND>) {
		$count++;
		chop;
		s/^\.\///;
		next if /y\.tab\.c|y\.tab\.h/;
		next if (!$'sflag && /\.[sS]$/);
		local($path) = $_;
		$path =~ s/\//$SEP/g;
		print STDERR " [$count/$total] converting $_\n" if ($vflag);
		&convert'src2html($_, "$html/$SRCS/$path.html");
	}
	close(FIND);
}
#=========================================================================
# CONVERT PACKAGE
#=========================================================================
package convert;
#
# src2html: convert source code into HTML
#
#	i)	$file	source file	- Read from
#	i)	$html	HTML file	- Write to
#
sub src2html {
	local($file, $html) = @_;
	local($ncol) = $'ncol;

	open(HTML, ">$html") || &error("cannot create file <$html>.\n");
	local($old) = select(HTML);
	#
	# load tags belonging to this file.
	#
	$file =~ s/^\.\///;
	&anchor'load($file);
	open(C, $file) || &error("cannot open file <$file>.\n");
	#
	# print the header
	#
	print "<HTML>\n<HEAD><TITLE>$file</TITLE></HEAD>\n";
	print "<BODY><A NAME=TOP><H2>$file</H2>\n";
	print &link_format(&anchor'getlinks(0));
	print "\n<HR>\n";
	print "<H2>FUNCTIONS</H2>\n";
	print "This source file includes following functions.\n";
	print "<OL>\n";
	local($lno, $tag, $type);
	for (($lno, $tag, $type) = &anchor'first(); $lno; ($lno, $tag, $type) = &anchor'next()) {
		print "<LI><A HREF=#$lno>$tag</A>\n" if ($type eq 'D');
	}
	print "</OL>\n";
	print "<HR>\n";
	#
	# print source code
	#
	print "<PRE>\n";
	$INCOMMENT = 0;			# initial status is out of comment
	local($LNO, $TAG, $TYPE) = &anchor'first();
	while (<C>) {
		s/\r$//;
		s/&/&amp;/g;		# '<', '>' and '&' are used for HTML tag
		s/</&lt;/g;
		s/>/&gt;/g;
		&protect_line();	# protect quoted char, strings and comments
		# painting source code
		s/({|})/$'brace_begin$1$'brace_end/g;
		$sharp = s/^(#\w+)// ? $1 : '';			# protect macro
		s/\b($'reserved_words)\b/$'reserved_begin$1$'reserved_end/go if ($sharp ne '#include');
		s/^/$'sharp_begin$sharp$'sharp_end/ if ($sharp);	# recover macro

		local($define_line) = 0;
		local(@links) = ();
		local($count) = 0;
		local($first);

		for ($first = 1; int($LNO) == $.; ($LNO, $TAG, $TYPE) = &anchor'next()) {
			if ($first) {
				$first = 0;
				print "<A NAME=$LNO>"
			}
			$define_line = $LNO if ($TYPE eq 'D');
			$db = ($TYPE eq 'D') ? 'GRTAGS' : 'GTAGS';
			local($line) = &cache'get($db, $TAG);
			if (defined($line)) {
				local($href, $dir);
				if ($line) {
					local($nouse, $lno, $filename) = split(/[ \t]+/, $line);
					$nouse = '';	# to make perl quiet
					$filename =~ s/^\.\///;
					$filename =~ s/\//$'ESCSEP/g;
					$href = "<A HREF=../$'SRCS/$filename.html#$lno>$TAG</A>";
				} else {
					$dir = ($TYPE eq 'D') ? $'REFS : $'DEFS;
					$href = "<A HREF=../$dir/$TAG.html>$TAG</A>";
				}
				# set tag marks and push hyperlink into @links
				if (s/\b$TAG\b/\005$count\005/) {
					$count++;
					push(@links, $href);
				} else {
					print STDERR "Error: $file $LNO $TAG($TYPE) tag must exist.\n" if ($'wflag);
				}
			} else {
				print STDERR "Warning: $file $LNO $TAG($TYPE) found but not refered.\n" if ($'wflag);
			}
		}
		# implant links
		local($s);
		for ($count = 0; @links; $count++) {
			$s = shift @links;
			unless (s/\005$count\005/$s/) {
				print STDERR "Error: $file $LNO $TAG($TYPE) tag must exist.\n" if ($'wflag);
			}
		}
		&unprotect_line();
		# print a line
		printf "%${ncol}d ", $.;
		print;
		# print hyperlinks
		if ($define_line && $file !~ /\.h$/) {
			print ' ' x ($ncol + 1);
			print &link_format(&anchor'getlinks($define_line));
			print "\n";
		}
	}
	print "</PRE>\n";
	print "<HR>\n";
	print "<A NAME=BOTTOM>\n";
	print &link_format(&anchor'getlinks(-1));
	print "\n";
	print "</BODY>\n</HTML>\n";
	close(C);
	close(HTML);
	select($old);

}
#
# protect_line: protect quoted strings
#
#	io)	$_	source line
#
#	\001	quoted(\) char
#	\002	quoted('') char
#	\003	quoted string
#	\004	comment
#	\032	temporary mark
#
sub protect_line {
	@quoted_char1 = ();
	while (/(\\.)/) {
		push(@quoted_char1, $1);
		s/\\./\001/;
	}
	@quoted_char2 = ();
	while (/('[^']')/) {
		push(@quoted_char2, $1);
		s/'[^']'/\002/;
	}
	@quoted_strings = ();
	while (/("[^"]*")/) {
		push(@quoted_strings, $1);
		s/"[^"]*"/\003/;
	}
	@comments = ();
	s/^/\032/ if ($INCOMMENT);
	while (1) {
		if ($INCOMMENT == 0) {
			if (/\/\*/) {			# start comment
				s/\/\*/\032\/\*/;
				$INCOMMENT = 1;
			} else {
				last;
			}
		} else {
			# Thanks to Jeffrey Friedl for his code.
			if (m!\032(/\*)?[^*]*\*+([^/*][^*]*\*+)*/!) {
				$INCOMMENT = 0;
				# minimum matching
				s!\032((/\*)?[^*]*\*+([^/*][^*]*\*+)*/)!\004!;
				push(@comments, $1);
			} else {
				s/\032(.*)$/\004/;	# mark comment
				push(@comments, $1);
			}
			last if ($INCOMMENT);
		}
	}
}
#
# unprotect_line: recover quoted strings
#
#	i)	$_	source line
#
sub unprotect_line {
	local($s);

	while (@comments) {
		$s = shift @comments;
		s/\004/$'comment_begin$s$'comment_end/;
	}
	while (@quoted_strings) {
		$s = shift @quoted_strings;
		s/\003/$s/;
	}
	while (@quoted_char2) {
		$s = shift @quoted_char2;
		s/\002/$s/;
	}
	while (@quoted_char1) {
		$s = shift @quoted_char1;
		s/\001/$s/;
	}
}
#
# link_format: format hyperlinks.
#
#	i)	(previous, next, first, last, top, bottom)
#
sub link_format {
	local(@tag) = @_;
	local(@label) = ('&lt;', '&gt;', '^', 'v', 'top', 'bottom');

	local($line) = "$'comment_begin/* ";
	while ($label = shift @label) {
		local($tag) = shift @tag;
		$line .=  "<A HREF=#$tag>" if ($tag);
		$line .=  "[$label]";
		$line .=  "</A>"		if ($tag);
	}
	$line .=  "<A HREF=../mains.html>[index]</A>";
	$line .=  "<A HREF=../help.html>[help]</A>";
	$line .=  " */$'comment_end";

	$line;
}

#=========================================================================
# ANCHOR PACKAGE
#=========================================================================
package anchor;
#
# create: create anchors temporary database
#
sub create {
	$ANCH = "$'tmp/ANCH$$";
	open(ANCH, "| btreeop -C $ANCH") || &error("btreeop -C $ANCH failed.\n");
	foreach $db ('GTAGS', 'GRTAGS') {
		local($type) = ($db eq 'GTAGS') ? 'D' : 'R';
		open(PIPE, "btreeop $'dbpath/$db |") || &error("btreeop $'dbpath/$db failed.\n");
		while (<PIPE>) {
			local($tag, $lno, $filename) = split;
			print ANCH "$filename $lno $tag $type\n";
		}
		close(PIPE);
	}
	close(ANCH);
}
#
# finish: remove anchors database
#
sub finish {
	unlink("$ANCH") if (defined($ANCH));
}
#
# load: load anchors in a file from database
#
#	i)	$file	source file
#
sub load {
	local($file) = @_;

	$file = './' . $file if ($file !~ /^\.\//);

	@ANCHORS = ();
	open(ANCH, "btreeop -K $file $ANCH|") || &error("btreeop -K $file $ANCH failed.\n");
$n = 0;
	while (<ANCH>) {
		local($filename, $lno, $tag, $type) = split;
		local($line);
		# START for DTFLAG
		# don't refer to macros which is defined in other C source.
		if ($type eq 'R' && ($line = &cache'get('GTAGS', $tag))) {
			local($nouse1, $nouse2, $f, $def) = split(/[ \t]+/, $line);
			if ($f !~ /\.h$/ && $f !~ $filename && $def =~ /^#/) {
				print STDERR "Information: skip <$filename $lno $tag> because this is a macro which is defined in other C source.\n" if ($'wflag);
				next;
			}
		}
		# END for DTFLAG
		push(@ANCHORS, "$lno,$tag,$type");
	}
	close(ANCH);
	local(@keys);
	foreach (@ANCHORS) {
		push(@keys, (split(/,/))[0]);
	}
	sub compare { $keys[$a] <=> $keys[$b]; }
	@ANCHORS = @ANCHORS[sort compare 0 .. $#keys];
	local($c);
	for ($c = 0; $c < @ANCHORS; $c++) {
		local($lno, $tag, $type) = split(/,/, $ANCHORS[$c]);
		if ($type eq 'D') {
			$FIRST = $lno;
			last;
		}
	}
	for ($c = $#ANCHORS; $c >= 0; $c--) {
		local($lno, $tag, $type) = split(/,/, $ANCHORS[$c]);
		if ($type eq 'D') {
			$LAST = $lno;
			last;
		}
	}
}
#
# first: get first anchor
#
sub first {
	$CURRENT = 0;
	local($lno, $tag, $type) = split(/,/, $ANCHORS[$CURRENT]);
	$CURRENTDEF = $CURRENT if ($type eq 'D');

	($lno, $tag, $type);
}
#
# next: get next anchor
#
sub next {
	if (++$CURRENT > $#ANCHORS) {
		return ('', '', '');
	}
	local($lno, $tag, $type) = split(/,/, $ANCHORS[$CURRENT]);
	$CURRENTDEF = $CURRENT if ($type eq 'D');

	($lno, $tag, $type);
}
#
# getlinks: get links
#
#	i)	linenumber	>= 1: line number
#				0: header, -1: tailer
#	gi)	@ANCHORS tag table in current file
#	r)		(previous, next, first, last, top, bottom)
#
sub getlinks {
	local($linenumber) = @_;
	local($prev, $next, $first, $last, $top, $bottom);

	$prev = $next = $first = $last = $top = $bottom = 0;
	if ($linenumber >= 1) {
		local($c, $p, $n);
		if ($CURRENTDEF == 0) {
			for ($c = 0; $c <= $#ANCHORS; $c++) {
				local($lno, $tag, $type) = split(/,/, $ANCHORS[$c]);
				if ($lno == $linenumber && $type eq 'D') {
					last;
				}
			}
			$CURRENTDEF = $c;
		} else {
			for ($c = $CURRENTDEF; $c >= 0; $c--) {
				local($lno, $tag, $type) = split(/,/, $ANCHORS[$c]);
				if ($lno == $linenumber && $type eq 'D') {
					last;
				}
			}
		}
		$p = $n = $c;
		while (--$p >= 0) {
			local($lno, $tag, $type) = split(/,/, $ANCHORS[$p]);
			if ($type eq 'D') {
				$prev = $lno;
				last;
			}
		}
		while (++$n <= $#ANCHORS) {
			local($lno, $tag, $type) = split(/,/, $ANCHORS[$n]);
			if ($type eq 'D') {
				$next = $lno;
				last;
			}
		}
	}
	$first = $FIRST if ($linenumber != $FIRST);
	$last  = $LAST if ($linenumber != $LAST);
	$top = 'TOP' if ($linenumber != 0);
	$bottom = 'BOTTOM' if ($linenumber != -1);
	if ($FIRST == $LAST) {
		$last  = '' if ($linenumber == 0);
		$first = '' if ($linenumber == -1);
	}

	($prev, $next, $first, $last, $top, $bottom);
}

#=========================================================================
# CACHE PACKAGE
#=========================================================================
package cache;
#
# open: open tag cache
#
#	i)	size	cache size
#			   -1: all cache
#			    0: no cache
#			other: sized cache
#
sub open {
	($cachesize) = @_;

	if ($cachesize == -1) {
		return;
	}
	undef %CACH if defined(%CACH);
	$cachecount = 0;
}
#
# put: put tag into cache
#
#	i)	$db	database name
#	i)	$tag	tag name
#	i)	$line	tag line
#
sub put {
	local($db, $tag, $line) = @_;
	local($label) = ($db eq 'GTAGS') ? 'D' : 'R';

	$cachecount++;
	if ($cachesize >= 0 && $cachecount > $cachesize) {
		$CACH  = "$'tmp/CACH$$";
		dbmopen(%CACH, $CACH, 0600) || &error("make cache database.\n");
		$cachesize = -1;
	}
	$CACH{$label.$tag} = $line;
}
#
# get: get tag from cache
#
#	i)	$db	database name
#	i)	$tag	tag name
#	r)		tag line
#
sub get {
	local($db, $tag) = @_;
	local($label) = ($db eq 'GTAGS') ? 'D' : 'R';

	defined($CACH{$label.$tag}) ? $CACH{$label.$tag} : undef;
}
#
# close: close cache
#
sub close {
	#dbmclose(%CACH);
	unlink("$CACH.db") if (defined($CACH));
}
