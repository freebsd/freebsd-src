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
#	htags.pl				20-Jan-98
#
$com = $0;
$com =~ s/.*\///;
$usage = "usage: $com [-a][-f][-l][-n][-v][-w][-t title][-d tagdir][dir]\n";
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
if (defined($ENV{'TMPDIR'}) && -d $ENV{'TMPDIR'}) {
	$tmp = $ENV{'TMPDIR'};
}
#-------------------------------------------------------------------------
# DEFINITION
#-------------------------------------------------------------------------
# unit for a path
$SEP    = ' ';	# source file path must not include $SEP charactor
$ESCSEP = &escape($SEP);
$SRCS   = 'S';
$DEFS   = 'D';
$REFS   = 'R';
$INCS   = 'I';
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
	"if (parent.frames.length && parent.$f_funcs == self) {\n" .
	"	document.links[0].href = '../funcs.html';\n" .
	"	document.links[document.links.length - 1].href = '../funcs.html';\n" .
	"}\n";
$rewrite_href_files =
	"if (parent.frames.length && parent.$f_files == self) {\n" .
	"       document.links[0].href = '../files.html';\n" .
	"       document.links[document.links.length - 1].href = '../files.html';\n" .
	"}\n";
#-------------------------------------------------------------------------
# UTIRITIES
#-------------------------------------------------------------------------
$findcom = "find . \\( -type f -o -type l \\) -name '*.[chysS]' -print";
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
	&clean();
	printf STDERR "$com: $_[0]\n";
	exit 1;
}
sub clean {
	&anchor'finish();
	&cache'close();
}
sub escape {
	local($c) = @_;
	'%' . sprintf("%x", ord($c));
}
sub usable {
	local($com) = @_;
	foreach (split(/:/, $ENV{'PATH'})) {
		return 1 if (-x "$_/$com");
	}
	return 0;
}
sub copy {
	local($from, $to) = @_;
	local($ret) = system("cp $from $to");
	$ret = $ret / 256;
	$ret = ($ret == 0) ? 1 : 0;
	$ret;
}
#-------------------------------------------------------------------------
# PROCESS START
#-------------------------------------------------------------------------
#
# options check.
#
$aflag = $fflag = $lflag = $nflag = $vflag = $wflag = '';
while ($ARGV[0] =~ /^-/) {
	$opt = shift;
	if ($opt =~ /[^-aflnvwtd]/) {
		print STDERR $usage;
		exit 1;
	}
	if ($opt =~ /a/) { $aflag = 'a'; }
	if ($opt =~ /f/) { $fflag = 'f'; }
	if ($opt =~ /l/) { $lflag = 'l'; }
	if ($opt =~ /n/) { $nflag = 'n'; }
	if ($opt =~ /v/) { $vflag = 'v'; }
	if ($opt =~ /w/) { $wflag = 'w'; }
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
$dbpath = &getcwd() if (!$dbpath);
unless (-r "$dbpath/GTAGS" && -r "$dbpath/GRTAGS") {
	&error("GTAGS and GRTAGS not found. please type 'gtags[RET]'");
}
#
# recognize format version
# if version record is not found, it's assumed version 1.
#
	$support_version = 1;		# I can understand this format version
#
open(GTAGS, "btreeop -K ' __.VERSION' $dbpath/GTAGS |") || &error("GTAGS not found.");
$rec = <GTAGS>;
close(GTAGS);
if ($rec =~ /^ __\.VERSION[ \t]+([0-9]+)$/) {
	$format_version = $1;
} else {
	$format_version = 1;
}
if ($format_version != $support_version) {
	&error("GTAGS format version unmatched. Please remake it.");
}
#
# check directories
#
$html = &getcwd() . '/HTML';
if ($ARGV[0]) {
	$cwd = &getcwd();
	unless (-w $ARGV[0]) {
		 &error("'$ARGV[0]' is not writable directory.");
	}
	chdir($ARGV[0]) || &error("directory '$ARGV[0]' not found.");
	$html = &getcwd() . '/HTML';
	chdir($cwd) || &error("cannot return to original directory.");
}
#
# check if GTAGS, GRTAGS is the latest.
#
$gtags_ctime = (stat("$dbpath/GTAGS"))[10];
open(FIND, "$findcom |") || &error("cannot exec find.");
while (<FIND>) {
	chop;
	next if /(y\.tab\.c|y\.tab\.h)$/;
	next if /(\/SCCS\/|\/RCS\/)/;
	if ($gtags_ctime < (stat($_))[10]) {
		&error("GTAGS is not the latest one. Please remake it.");
	}
}
close(FIND);
#-------------------------------------------------------------------------
# MAKE FILES
#-------------------------------------------------------------------------
#	HTML/cgi-bin/global.cgi	... CGI program (1)
#	HTML/help.html		... help file (2)
#	HTML/$REFS/*		... referencies (3)
#	HTML/$DEFS/*		... definitions (3)
#	HTML/funcs.html		... function index (4)
#	HTML/funcs/*		... function index (4)
#	HTML/files.html		... file index (5)
#	HTML/files/*		... file index (5)
#	HTML/index.html		... index file (6)
#	HTML/mains.html		... main index (7)
#	HTML/$SRCS/		... source files (8)
#	HTML/$INCS/		... include file index (9)
#-------------------------------------------------------------------------
print STDERR "[", &date, "] ", "Htags started\n" if ($vflag);
#
# (0) make directories
#
print STDERR "[", &date, "] ", "(0) making directories ...\n" if ($vflag);
mkdir($html, 0777) || &error("cannot make directory '$html'.") if (! -d $html);
foreach $d ($SRCS, $INCS, $DEFS, $REFS, files, funcs) {
	mkdir("$html/$d", 0775) || &error("cannot make HTML directory") if (! -d "$html/$d");
}
if ($fflag) {
	mkdir("$html/cgi-bin", 0775) || &error("cannot make cgi-bin directory") if (! -d "$html/cgi-bin");
}
#
# (1) make CGI program
#
if ($fflag) {
	print STDERR "[", &date, "] ", "(1) making CGI program ...\n" if ($vflag);
	&makeprogram("$html/cgi-bin/global.cgi") || &error("cannot make CGI program.");
	chmod(0755, "$html/cgi-bin/global.cgi") || &error("cannot chmod CGI program.");
	unlink("$html/cgi-bin/GTAGS", "$html/cgi-bin/GRTAGS");
	link("$dbpath/GTAGS", "$html/cgi-bin/GTAGS") || &copy("$dbpath/GTAGS", "$html/cgi-bin/GTAGS") || &error("cannot copy GTAGS.");
	link("$dbpath/GRTAGS", "$html/cgi-bin/GRTAGS") || &copy("$dbpath/GRTAGS", "$html/cgi-bin/GRTAGS") || &error("cannot copy GRTAGS.");
}
#
# (2) make help file
#
print STDERR "[", &date, "] ", "(2) making help.html ...\n" if ($vflag);
&makehelp("$html/help.html");
#
# (3) make function entries ($DEFS/* and $REFS/*)
#     MAKING TAG CACHE
#
print STDERR "[", &date, "] ", "(3) making duplicate entries ...\n" if ($vflag);
sub suddenly { &clean(); exit 1}
$SIG{'INT'} = 'suddenly';
$SIG{'QUIT'} = 'suddenly';
$SIG{'TERM'} = 'suddenly';
&cache'open(100000);
$func_total = &makedupindex();
print STDERR "Total $func_total functions.\n" if ($vflag);
#
# (4) make function index (funcs.html and funcs/*)
#     PRODUCE @funcs
#
print STDERR "[", &date, "] ", "(4) making function index ...\n" if ($vflag);
$func_total = &makefuncindex("$html/funcs.html", $func_total);
print STDERR "Total $func_total functions.\n" if ($vflag);
#
# (5) make file index (files.html and files/*)
#     PRODUCE @files %includes
#
print STDERR "[", &date, "] ", "(5) making file index ...\n" if ($vflag);
$file_total = &makefileindex("$html/files.html", "$html/$INCS");
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
#     USING TAG CACHE, %includes and anchor database.
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
# makeprogram: make CGI program
#
sub makeprogram {
	local($file) = @_;

	open(PROGRAM, ">$file") || &error("cannot make CGI program.");
	$program = <<'END_OF_SCRIPT';
#!/usr/bin/perl
#------------------------------------------------------------------
# SORRY TO HAVE SURPRISED YOU!
# IF YOU SEE THIS UNREASONABLE FILE WHILE BROUSING, FORGET PLEASE.
# IF YOU ARE A ADMINISTRATOR OF THIS SITE, PLEASE SETUP HTTP SERVER
# SO THAT THIS SCRIPT CAN BE EXECUTED AS A CGI COMMAND. THANK YOU.
#------------------------------------------------------------------
$SRCS   = 'S';
$SEP    = ' ';	# source file path must not include $SEP charactor
$ESCSEP = &escape($SEP);
sub escape {
	local($c) = @_;
	'%' . sprintf("%x", ord($c));
}
print "Content-type: text/html\n\n";
print "<HTML>\n";
@pairs = split (/&/, $ENV{'QUERY_STRING'});
foreach $p (@pairs) {
	($name, $value) = split(/=/, $p);
	$value =~ tr/+/ /;
	$value =~ s/%([\dA-Fa-f][\dA-Fa-f])/pack("C", hex($1))/eg;
	$form{$name} = $value;
}
if ($form{'pattern'} eq '') {
	print "<H3>Pattern not specified. <A HREF=../mains.html>[return]</A></H3>\n";
	print "</HTML>\n";
	exit 0;
}
$pattern = $form{'pattern'};
$flag = ($form{'type'} eq 'definition') ? '' : 'r';
$words = ($form{'type'} eq 'definition') ? 'definitions' : 'referencies';
print "<H1><FONT COLOR=#cc0000>\"$pattern\"</FONT></H1>\n";
print "Following $words are matched to above pattern.<HR>\n";
$pattern =~ s/'//g;			# to shut security hole
unless (open(PIPE, "/usr/bin/global -x$flag '$pattern' |")) {
	print "<H3>Cannot execute global. <A HREF=../mains.html>[return]</A></H3>\n";
	print "</HTML>\n";
	exit 0;
}
$cnt = 0;
print "<PRE>\n";
while (<PIPE>) {
	$cnt++;
	local($tag, $lno, $filename) = split;
	$filename =~ s/^\.\///;
	$filename =~ s/\//$ESCSEP/g;
	s/($tag)/<A HREF=..\/$SRCS\/$filename.html#$lno>$1<\/A>/;
	print;
}
print "</PRE>\n";
if ($cnt == 0) {
	print "<H3>Pattern not found. <A HREF=../mains.html>[return]</A></H3>\n";
}
print "</HTML>\n";
exit 0;
#------------------------------------------------------------------
# SORRY TO HAVE SURPRISED YOU!
# IF YOU SEE THIS UNREASONABLE FILE WHILE BROUSING, FORGET PLEASE.
# IF YOU ARE A ADMINISTRATOR OF THIS SITE, PLEASE SETUP HTTP SERVER
# SO THAT THIS SCRIPT CAN BE EXECUTED AS A CGI COMMAND. THANK YOU.
#------------------------------------------------------------------
END_OF_SCRIPT

	print PROGRAM $program;
	close(PROGRAM);
}
#
# makehelp: make help file
#
sub makehelp {
	local($file) = @_;

	open(HELP, ">$file") || &error("cannot make help file.");
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
# makedupindex: make duplicate entries index ($DEFS/* and $REFS/*)
#
#	go)	tag cache
#	r)	$count
#
sub makeline {
	$_[0] =~ s/\.\///;
	$_[0] =~ s/&/&amp;/g;
	$_[0] =~ s/</&lt;/g;
	$_[0] =~ s/>/&gt;/g;
	local($tag, $lno, $filename) = split(/[ \t\n]+/, $_[0]);;
	$filename =~ s/\//$ESCSEP/g;
	$_[0] =~ s/^$tag/<A HREF=..\/$SRCS\/$filename.html#$lno>$tag<\/A>/;
}
sub makedupindex {
	local($count) = 0;

	foreach $db ('GRTAGS', 'GTAGS') {
		local($kind) = $db eq 'GTAGS' ? "definitions" : "references";
		local($prev) = '';
		local($first_line);
		local($writing) = 0;

		$count = 0;
		open(LIST, "btreeop $dbpath/$db | sort +0 -1 +2 -3 +1n -2|") || &error("btreeop $dbpath/$db | sort +0 -1 +2 -3 +1n -2 failed.");
		while (<LIST>) {
			chop;
			local($tag, $lno, $filename) = split;
			if ($prev ne $tag) {
				$count++;
				print STDERR " [$count] adding $tag $kind.\n" if ($vflag);
				if ($writing) {
					print FILE "</PRE>\n</BODY>\n</HTML>\n";
					close(FILE);
					$writing = 0;
				}
				# single entry
				if ($first_line) {
					&cache'put($db, $prev, $first_line);
				}
				$first_line = $_;
				$prev = $tag;
			} else {
				# duplicate entry
				if ($first_line) {
					&cache'put($db, $tag, '');
					local($type) = ($db eq 'GTAGS') ? $DEFS : $REFS;
					open(FILE, ">$html/$type/$tag.html") || &error("cannot make file '$html/$type/$tag.html'.");
					$writing = 1;
					print FILE "<HTML>\n<HEAD><TITLE>$tag</TITLE></HEAD>\n<BODY>\n";
					print FILE "<PRE>\n";
					&makeline($first_line);
					print FILE $first_line, "\n";
					$first_line = '';
				}
				&makeline($_);
				print FILE $_, "\n";
			}
		}
		close(LIST);
		if ($writing) {
			print FILE "</PRE>\n</BODY>\n</HTML>\n";
			close(FILE);
		}
		if ($first_line) {
			&cache'put($db, $prev, $first_line);
		}
	}
	$count;
}
#
# makefuncindex: make function index (including alphabetic index)
#
#	i)	file		function index file
#	i)	total		functions total
#	gi)	tag cache
#	go)	@funcs
#
sub makefuncindex {
	local($file, $total) = @_;
	local($count) = 0;

	open(FUNCTIONS, ">$file") || &error("cannot make function index '$file'.");
	print FUNCTIONS "<HTML>\n<HEAD><TITLE>FUNCTION INDEX</TITLE>\n";
	print FUNCTIONS "$begin_script$defaultview$end_script</HEAD>\n<BODY>\n";
	print FUNCTIONS "<H2>FUNCTION INDEX</H2>\n";
	print FUNCTIONS "<OL>\n" if (!$aflag);
	local($old) = select(FUNCTIONS);
	open(TAGS, "btreeop -L $dbpath/GTAGS |") || &error("btreeop -L $dbpath/GTAGS failed.");
	local($alpha) = '';
	@funcs = ();	# [A][B][C]...
	while (<TAGS>) {
		$count++;
		chop;
		local($tag) = $_;
		print STDERR " [$count/$total] adding $tag\n" if ($vflag);
		if ($aflag && $alpha ne substr($tag, 0, 1)) {
			if ($alpha) {
				print ALPHA "</OL>\n";
				print ALPHA "<A HREF=../mains.html TARGET=_self>[index]</A>\n";
				print ALPHA "$begin_script$rewrite_href_funcs$end_script";
				print ALPHA "</BODY>\n</HTML>\n";
				close(ALPHA);
			}
			$alpha = substr($tag, 0, 1);
			push(@funcs, "<A HREF=funcs/$alpha.html TARGET=_self>[$alpha]</A>\n");
			open(ALPHA, ">$html/funcs/$alpha.html") || &error("cannot make alphabetical function index.");
			print ALPHA "<HTML>\n<HEAD><TITLE>$alpha</TITLE>\n";
			print ALPHA "$begin_script$defaultview$end_script";
			print ALPHA "</HEAD>\n<BODY>\n<H2>[$alpha]</H2>\n";
			print ALPHA "<A HREF=../mains.html TARGET=_self>[index]</A>\n";
			print ALPHA "<OL>\n";
			select(ALPHA);
		}
		local($line) = &cache'get('GTAGS', $tag);
		if (!$line) {
			print "<LI><A HREF=", ($aflag) ? "../" : "", "$DEFS/$tag.html>$tag</A>\n";
		} else {
			local($tag, $lno, $filename) = split(/[ \t]+/, $line);
			$filename =~ s/^\.\///;
			$filename =~ s/\//$ESCSEP/g;
			print "<LI><A HREF=", ($aflag) ? "../" : "", "$SRCS/$filename.html#$lno>$tag</A>\n";
		}
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
# makefileindex: make file index
#
#	i)	file name
#	i)	$INC directory
#	go)	@files
#	go)	%includes
#
sub makefileindex {
	local($file, $incdir) = @_;
	local($count) = 0;

	open(FILES, ">$file") || &error("cannot make file '$file'.");
	print FILES "<HTML>\n<HEAD><TITLE>FILES</TITLE>\n";
	print FILES "$begin_script$defaultview$end_script";
	print FILES "</HEAD>\n<BODY>\n<H2>FILE INDEX</H2>\n";
	print FILES "<OL>\n";
	local($old) = select(FILES);
	open(FIND, "$findcom | sort |") || &error("cannot exec find.");
	local($lastdir) = '';
	@files = ();
	while (<FIND>) {
		next if /(y\.tab\.c|y\.tab\.h)$/;
		next if /(\/SCCS\/|\/RCS\/)/;

		$count++;
		chop;
		s/^\.\///;
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
				push(@files, "<LI><A HREF=files/$dir.html TARGET=_self>$dir/</A>\n");
				open(DIR, ">$html/files/$dir.html") || &error("cannot make directory index.");
				print DIR "<HTML>\n<HEAD><TITLE>$dir/</TITLE>\n";
				print DIR "$begin_script$defaultview$end_script";
				print DIR "</HEAD>\n<BODY>\n<H2>$dir/</H2>\n";
				print DIR "<A HREF=../mains.html TARGET=_self>[index]</A>\n";
				print DIR "<OL>\n";
			}
			$lastdir = $dir;
		}
		# collect include files.
		if ($filename =~ /.*\.h$/) {
			local($last) = $filename;
			$last =~ s!.*/!!;
			if (! defined $includes{$last}) {
				$includes{$last} = $filename;
			} else {
				# duplicate entries
				$includes{$last} = "$includes{$last}\n$filename";
			}
		}
		local($path) = $filename;
		$path =~ s/\//$ESCSEP/g;
		if ($dir eq '') {
			push(@files, "<LI><A HREF=$SRCS/$path.html>$filename</A>\n");
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

	foreach $last (keys %includes) {
		local(@incs) = split(/\n/, $includes{$last});
		if (@incs > 1) {
			open(INCLUDE, ">$incdir/$last.html") || &error("cannot open file '$incdir/$last.html'.");
			print INCLUDE "<HTML>\n<HEAD><TITLE>$last</TITLE></HEAD>\n<BODY>\n<PRE>\n";
			foreach $filename (@incs) {
				local($path) = $filename;
				$path =~ s/\//$ESCSEP/g;
				print INCLUDE "<A HREF=../$SRCS/$path.html>$filename</A>\n";
			}
			print INCLUDE "</PRE>\n</BODY>\n</HTML>\n";
			close(INCLUDE);
			# '' means that information already written to file.
			$includes{$last} = '';
		}
	}
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
	if ($fflag) {
		$index .= "<H2>FUNCTION SEARCH</H2>\n";
		$index .= "Please input function name and select [Search]. Perl's regular expression is allowed.<P>\n"; 
		$index .= "<FORM METHOD=GET ACTION=cgi-bin/global.cgi>\n";
		$index .= "<INPUT NAME=pattern>\n";
		$index .= "<INPUT TYPE=submit VALUE=Search>\n";
		$index .= "<INPUT TYPE=reset VALUE=Reset><BR>\n";
		$index .= "<INPUT TYPE=radio NAME=type VALUE=definition CHECKED>Definition\n";
		$index .= "<INPUT TYPE=radio NAME=type VALUE=reference>Reference\n";
		$index .= "</FORM>\n<HR>\n";
	}
	$index .= "<H2>MAINS</H2>\n";
	$index .= "<PRE>\n";
	open(PIPE, "btreeop -K main $dbpath/GTAGS | sort +0 -1 +2 -3 +1n -2|") || &error("btreeop -K main $dbpath/GTAGS failed.");
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

	open(FRAME, ">$file") || &error("cannot open file '$file'.");
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

	open(INDEX, ">$file") || &error("cannot create file '$file'.");
	print INDEX "<HTML>\n<HEAD><TITLE>MAINS</TITLE></HEAD>\n";
	print INDEX "<BODY>\n$index</BODY>\n</HTML>\n";
	close(INDEX);
}
#
# makehtml: make html files
#
#	i)	total	number of files.
#
sub makehtml {
	local($total) = @_;
	local($count) = 0;

	open(FIND, "$findcom |") || &error("cannot exec find.");
	while (<FIND>) {
		next if /y\.tab\.c|y\.tab\.h/;
		next if /(\/SCCS\/|\/RCS\/)/;

		$count++;
		chop;
		local($path) = $_;
		$path =~ s/^\.\///;
		print STDERR " [$count/$total] converting $path\n" if ($vflag);
		$path =~ s/\//$SEP/g;
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
#	gi)	%includes
#			pairs of include file and the path
#
sub src2html {
	local($file, $html) = @_;
	local($ncol) = $'ncol;
	local($expand) = &'usable('expand') ? 'expand' : 'cat';
	local(%ctab) = ('&', '&amp;', '<', '&lt;', '>', '&gt;');

	open(HTML, ">$html") || &'error("cannot create file '$html'.");
	local($old) = select(HTML);
	#
	# load tags belonging to this file.
	#
	&anchor'load($file);
	open(C, "$expand '$file' |") || &'error("cannot open file '$file'.");
	#
	# print the header
	#
	$file =~ s/^\.\///;
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
		local($converted);
		s/\r$//;
		# make link for include file
		if (!$INCOMMENT && /^#include/) {
			local($last, $sep) = m![</"]([^</"]+)([">])!;
			local($link);
			if (defined $'includes{$last}) {
				if ($'includes{$last}) {
					$link = $'includes{$last};
					$link =~ s/\//$'ESCSEP/g;
				} else {
					$link = "../$'INCS/$last";
				}
				if ($sep eq '"') {
					s!"(.*$last)"!"<A HREF=$link.html>$1</A>"!;
				} else {
					s!<(.*$last)>!&lt;<A HREF=$link.html>$1</A>&gt;!;
				}
				$converted = 1;
			}
		}
		# translate '<', '>' and '&' into entity name
		if (!$converted) { s/([&<>])/$ctab{$1}/ge; }
		&protect_line();	# protect quoted char, strings and comments
		# painting source code
		s/({|})/$'brace_begin$1$'brace_end/g;
		local($sharp) = s/^(#\w+)// ? $1 : '';
		s/\b($'reserved_words)\b/$'reserved_begin$1$'reserved_end/go if ($sharp ne '#include');
		s/^/$'sharp_begin$sharp$'sharp_end/ if ($sharp);	# recover macro

		local($define_line) = 0;
		local(@links) = ();
		local($count) = 0;
		local($lno_printed) = 0;

		if ($'lflag) {
			print "<A NAME=$.>";
			$lno_printed = 1;
		}
		for (; int($LNO) == $.; ($LNO, $TAG, $TYPE) = &anchor'next()) {
			if (!$lno_printed) {
				print "<A NAME=$.>";
				$lno_printed = 1;
			}
			$define_line = $LNO if ($TYPE eq 'D');
			$db = ($TYPE eq 'D') ? 'GRTAGS' : 'GTAGS';
			local($line) = &cache'get($db, $TAG);
			if (defined($line)) {
				local($href);
				if ($line) {
					local($nouse, $lno, $filename) = split(/[ \t]+/, $line);
					$nouse = '';	# to make perl quiet
					$filename =~ s/^\.\///;
					$filename =~ s/\//$'ESCSEP/g;
					$href = "<A HREF=../$'SRCS/$filename.html#$lno>$TAG</A>";
				} else {
					local($dir) = ($TYPE eq 'D') ? $'REFS : $'DEFS;
					$href = "<A HREF=../$dir/$TAG.html>$TAG</A>";
				}
				# set tag marks and save hyperlink into @links
				if (s/\b$TAG\b/\005$count\005/ || s/\b_$TAG\b/_\005$count\005/) {
					$count++;
					push(@links, $href);
				} else {
					print STDERR "Error: $file $LNO $TAG($TYPE) tag must exist.\n" if ($'wflag);
				}
			} else {
				print STDERR "Warning: $file $LNO $TAG($TYPE) found but not referred.\n" if ($'wflag);
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
		printf "%${ncol}d ", $. if ($'nflag);
		print;
		# print hyperlinks
		if ($define_line && $file !~ /\.h$/) {
			print ' ' x ($ncol + 1) if ($'nflag);
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
	while (s/(\\.)/\001/) {
		push(@quoted_char1, $1);
	}
	@quoted_char2 = ();
	while (s/('[^']')/\002/) {
		push(@quoted_char2, $1);
	}
	@quoted_strings = ();
	while (s/("[^"]*")/\003/) {
		push(@quoted_strings, $1);
	}
	@comments = ();
	s/^/\032/ if ($INCOMMENT);
	while (1) {
		if ($INCOMMENT == 0) {
			if (s/\/\*/\032\/\*/) {		# start comment
				$INCOMMENT = 1;
			} else {
				last;
			}
		} else {
			# Thanks to Jeffrey Friedl for his code.
			if (s!\032((/\*)?[^*]*\*+([^/*][^*]*\*+)*/)!\004!) {
				push(@comments, $1);
				$INCOMMENT = 0;
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
#	go)	%PATHLIST
#
sub create {
	$ANCH = "$'tmp/ANCH$$";
	open(ANCH, ">$ANCH") || &'error("cannot create file '$ANCH'.");
	close(ANCH);
	chmod ($ANCH, 0600);
	open(ANCH, "| btreeop -C $ANCH") || &'error("btreeop -C $ANCH failed.");
	local($fcount) = 1;
	local($fnumber);
	foreach $db ('GTAGS', 'GRTAGS') {
		local($type) = ($db eq 'GTAGS') ? 'D' : 'R';
		open(PIPE, "btreeop $'dbpath/$db |") || &'error("btreeop $'dbpath/$db failed.");
		while (<PIPE>) {
			local($tag, $lno, $filename) = split;
			$fnumber = $PATHLIST{$filename};
			if (!$fnumber) {
				$PATHLIST{$filename} = $fnumber = $fcount++;
			}
			print ANCH "$fnumber $lno $tag $type\n";
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
#	gi)	%PATHLIST
#	go)	FIRST	first definition
#	go)	LAST	last definition
#
sub load {
	local($file) = @_;
	local($fnumber);

	@ANCHORS = ();
	$FIRST = $LAST = 0;

	$file = './' . $file if ($file !~ /^\.\//);
	if (!($fnumber = $PATHLIST{$file})) {
		return;
	}
	open(ANCH, "btreeop -K $fnumber $ANCH |") || &'error("btreeop -K $file $ANCH failed.");
$n = 0;
	while (<ANCH>) {
		local($fnumber, $lno, $tag, $type) = split;
		local($line);
		# don't refer to macros which is defined in other C source.
		if ($type eq 'R' && ($line = &cache'get('GTAGS', $tag))) {
			local($nouse1, $nouse2, $f, $def) = split(/[ \t]+/, $line);
			if ($f !~ /\.h$/ && $f !~ $file && $def =~ /^#/) {
				print STDERR "Information: $file $lno $tag($type) skipped, because this is a macro which is defined in other C source.\n" if ($'wflag);
				next;
			}
		}
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
	$first = $FIRST if ($FIRST > 0 && $linenumber != $FIRST);
	$last  = $LAST if ($LAST > 0 && $linenumber != $LAST);
	$top = 'TOP' if ($linenumber != 0);
	$bottom = 'BOTTOM' if ($linenumber != -1);
	if ($FIRST > 0 && $FIRST == $LAST) {
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
		dbmopen(%CACH, $CACH, 0600) || &'error("make cache database.");
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
