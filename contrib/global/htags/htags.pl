#!/usr/bin/perl
#
# Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
#	htags.pl				10-Nov-98
#
$com = $0;
$com =~ s/.*\///;
$usage = "usage: $com [-a][-c][-f][-h][-l][-n][-v][-w][-t title][-d tagdir][dir]\n";
#-------------------------------------------------------------------------
# COMMAND EXISTENCE CHECK
#-------------------------------------------------------------------------
foreach $c ('sort', 'gtags', 'global', 'btreeop') {
	if (!&'usable($c)) {
		&'error("'$c' command is required but not found.");
	}
}
#-------------------------------------------------------------------------
# CONFIGURATION
#-------------------------------------------------------------------------
# temporary directory
$'tmp = '/tmp';
if (defined($ENV{'TMPDIR'}) && -d $ENV{'TMPDIR'}) {
	$tmp = $ENV{'TMPDIR'};
}
$'ncol = 4;					# columns of line number
$'tabs = 8;					# tab skip
$'gzipped_suffix = 'ghtml';			# suffix of gzipped html file
#
# font
#
$'title_begin	= '<FONT COLOR=#cc0000>';
$'title_end	= '</FONT>';
$'comment_begin  = '<I><FONT COLOR=green>';	# /* ... */
$'comment_end    = '</FONT></I>';
$'sharp_begin    = '<FONT COLOR=darkred>';	# #define, #include or so on
$'sharp_end      = '</FONT>';
$'brace_begin    = '<FONT COLOR=blue>';		# { ... }
$'brace_end      = '</FONT>';
$'reserved_begin = '<B>';			# if, while, for or so on
$'reserved_end   = '</B>';
#
# color
#
$'body_bgcolor	= '';
$'body_text	= '';
$'body_link	= '';
$'body_vlink	= '';
$'body_alink	= '';
#
# Reserved words for C and Java are hard coded.
# (configuration parameter 'reserved_words' was deleted.)
#
$'c_reserved_words = "auto,break,case,char,continue,default,do,double,else," .
			"extern,float,for,goto,if,int,long,register,return," .
			"short,sizeof,static,struct,switch,typedef,union," .
			"unsigned,void,while";
$'java_reserved_words  = "abstract,boolean,break,byte,case,catch,char,class," .
			"const,continue,default,do,double,else,extends,false," .
			"final,finally,float,for,goto,if,implements,import," .
			"instanceof,int,interface,long,native,new,null," .
			"package,private,protected,public,return,short," .
			"static,super,switch,synchronized,this,throw,throws," .
			"union,transient,true,try,void,volatile,while";
$'c_reserved_words    =~ s/,/|/g;
$'java_reserved_words =~ s/,/|/g;
#
# read values from global.conf
#
chop($config = `gtags --config`);
if ($config) {
	if ($var1 = &'getconf('ncol')) {
		if ($var1 < 1 || $var1 > 10) {
			print STDERR "Warning: parameter 'ncol' ignored becase the value is too large or too small.\n";
		} else {
			$ncol = $var1;
		}
	}
	if ($var1 = &'getconf('tabs')) {
		if ($var1 < 1 || $var1 > 32) {
			print STDERR "Warning: parameter 'tabs' ignored becase the value is too large or too small.\n";
		} else {
			$tabs = $var1;
		}
	}
	if ($var1 = &'getconf('gzipped_suffix')) {
		$gzipped_suffix = $var1;
	}
	if (($var1 = &'getconf('title_begin')) && ($var2 = &'getconf('title_end'))) {
		$title_begin  = $var1;
		$title_end    = $var2;
	}
	if (($var1 = &'getconf('comment_begin')) && ($var2 = &'getconf('comment_end'))) {
		$comment_begin  = $var1;
		$comment_end    = $var2;
	}
	if (($var1 = &'getconf('sharp_begin')) && ($var2 = &'getconf('sharp_end'))) {
		$sharp_begin  = $var1;
		$sharp_end    = $var2;
	}
	if (($var1 = &'getconf('brace_begin')) && ($var2 = &'getconf('brace_end'))) {
		$brace_begin  = $var1;
		$brace_end    = $var2;
	}
	if (($var1 = &'getconf('reserved_begin')) && ($var2 = &'getconf('reserved_end'))) {
		$reserved_begin  = $var1;
		$reserved_end    = $var2;
	}
	$body_bgcolor	= $var1 if ($var1 = &'getconf('bgcolor'));
	$body_text	= $var1 if ($var1 = &'getconf('text'));
	$body_link	= $var1 if ($var1 = &'getconf('link'));
	$body_vlink	= $var1 if ($var1 = &'getconf('vlink'));
	$body_alink	= $var1 if ($var1 = &'getconf('alink'));
}
# HTML tag
$'begin_html  = "<HTML>\n";
$'end_html    = "</HTML>\n";
$'begin_body  = '<BODY';
$'begin_body .= " BGCOLOR=$body_bgcolor" if ($body_bgcolor);
$'begin_body .= " TEXT=$body_text" if ($body_text);
$'begin_body .= " LINK=$body_link" if ($body_link);
$'begin_body .= " LINK=$body_vlink" if ($body_vlink);
$'begin_body .= " LINK=$body_alink" if ($body_alink);
$'begin_body .= '>';
$'begin_body .= "\n";
$'end_body    = "</BODY>\n";
#-------------------------------------------------------------------------
# DEFINITION
#-------------------------------------------------------------------------
# unit for a path
$'SEP    = ' ';	# source file path must not include $SEP charactor
$'ESCSEP = &'escape($SEP);
$'SRCS   = 'S';
$'DEFS   = 'D';
$'REFS   = 'R';
$'INCS   = 'I';
#-------------------------------------------------------------------------
# JAVASCRIPT PARTS
#-------------------------------------------------------------------------
# escaped angle
$'langle  = sprintf("unescape('%s')", &'escape('<'));
$'rangle  = sprintf("unescape('%s')", &'escape('>'));
$'begin_script="<SCRIPT LANGUAGE=javascript>\n<!--\n";
$'end_script="<!-- end of script -->\n</SCRIPT>\n";
$'default_view=
	"// if your browser doesn't support javascript, write a BASE tag statically.\n" .
	"if (parent.frames.length)\n" .
	"	document.write($langle+'BASE TARGET=mains'+$rangle)\n";
$'rewrite_href_funcs =
	"if (parent.frames.length && parent.funcs == self) {\n" .
	"	document.links[0].href = '../funcs.html';\n" .
	"	document.links[document.links.length - 1].href = '../funcs.html';\n" .
	"}\n";
$'rewrite_href_files =
	"if (parent.frames.length && parent.files == self) {\n" .
	"	document.links[0].href = '../files.html';\n" .
	"	document.links[document.links.length - 1].href = '../files.html';\n" .
	"}\n";
sub set_header {
	local($display, $title, $script) = @_;
	local($head) = "<HEAD><TITLE>$title</TITLE>";
	if ($script || ($'hflag && $display)) {
		$head .= "\n";
		$head .= $'begin_script;
		$head .= $script if ($script);
		if ($'hflag && $display) {
			$title = '[' . $title . ']' if ($title);
			$head .= "if (parent.frames.length && parent.mains == self) {\n";
			$head .= "	parent.title.document.open();\n";
			$head .= "	parent.title.document.write('<H3>$title</H3>');\n";
			$head .= "	parent.title.document.close();\n";
			$head .= "}\n";
		}
		$head .= $'end_script;
	}
	$head .= "</HEAD>\n";
	$head;
}
#-------------------------------------------------------------------------
# UTIRITIES
#-------------------------------------------------------------------------
sub getcwd {
        local($dir) = `/bin/pwd`;
        chop($dir);
        $dir;
}
sub realpath {
	local($dir) = @_;
	local($cwd) = &getcwd;
	chdir($dir) || &'error("cannot change directory '$dir'.");
        local($new) = &getcwd;
	chdir($cwd) || &'error("cannot recover current directory '$cwd'.");
	$new;
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
	local($command) = @_;
	foreach (split(/:/, $ENV{'PATH'})) {
		return 1 if (-x "$_/$command");
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
sub getconf {
	local($name) = @_;
	local($val);
	chop($val = `gtags --config $name`);
	if ($? != 0) { $val = ''; }
	$val;
}
sub path2file {
	local($path) = @_;
	$path =~ s/^\.\///;
	$path =~ s!/!$'SEP!g;
	$path . '.' . $'HTML;
}
sub path2url {
	local($path) = @_;
	$path =~ s/^\.\///;
	$path =~ s!/!$'ESCSEP!g;
	$path . '.' . $'HTML;
}
#-------------------------------------------------------------------------
# PROCESS START
#-------------------------------------------------------------------------
#
# options check.
#
$'aflag = $'cflag = $'fflag = $'hflag = $'lflag = $'nflag = $'vflag = $'wflag = '';
while ($ARGV[0] =~ /^-/) {
	$opt = shift;
	if ($opt =~ /[^-acfhlnvwtd]/) {
		print STDERR $usage;
		exit 1;
	}
	if ($opt =~ /a/) { $'aflag = 'a'; }
	if ($opt =~ /c/) { $'cflag = 'c'; }
	if ($opt =~ /f/) { $'fflag = 'f'; }
	if ($opt =~ /h/) { $'hflag = 'h'; }
	if ($opt =~ /l/) { $'lflag = 'l'; }
	if ($opt =~ /n/) { $'nflag = 'n'; }
	if ($opt =~ /v/) { $'vflag = 'v'; }
	if ($opt =~ /w/) { $'wflag = 'w'; }
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
if ($'cflag && !&'usable('gzip')) {
	print STDERR "Warning: 'gzip' command not found. -c option ignored.\n";
	$'cflag = '';
}
if (!$title) {
	@cwd = split('/', &'getcwd);
	$title = $cwd[$#cwd];
}
$dbpath = '.' if (!$dbpath);
unless (-r "$dbpath/GTAGS" && -r "$dbpath/GRTAGS") {
	&'error("GTAGS and GRTAGS not found. Please make them.");
}
$dbpath = &'realpath($dbpath);
#
# for global(1)
#
$ENV{'GTAGSROOT'} = &'getcwd();
$ENV{'GTAGSDBPATH'} = $dbpath;
delete $ENV{'GTAGSLIBPATH'};
#
# check directories
#
$dist = &'getcwd() . '/HTML';
if ($ARGV[0]) {
	$cwd = &'getcwd();
	unless (-w $ARGV[0]) {
		 &'error("'$ARGV[0]' is not writable directory.");
	}
	chdir($ARGV[0]) || &'error("directory '$ARGV[0]' not found.");
	$dist = &'getcwd() . '/HTML';
	chdir($cwd) || &'error("cannot return to original directory.");
}
#
# find filter
#
$'findcom = "gtags --find";
#
# check if GTAGS, GRTAGS is the latest.
#
$gtags_ctime = (stat("$dbpath/GTAGS"))[10];
open(FIND, "$'findcom |") || &'error("cannot fork.");
while (<FIND>) {
	chop;
	if ($gtags_ctime < (stat($_))[10]) {
		&'error("GTAGS is not the latest one. Please remake it.");
	}
}
close(FIND);
if ($?) { &'error("cannot traverse directory."); }
#-------------------------------------------------------------------------
# MAKE FILES
#-------------------------------------------------------------------------
#	HTML/cgi-bin/global.cgi	... CGI program (1)
#	HTML/cgi-bin/ghtml.cgi	... unzip script (1)
#	HTML/.htaccess.skel	... skelton of .htaccess (1)
#	HTML/help.html		... help file (2)
#	HTML/$REFS/*		... referencies (3)
#	HTML/$DEFS/*		... definitions (3)
#	HTML/funcs.html		... function index (4)
#	HTML/funcs/*		... function index (4)
#	HTML/files.html		... file index (5)
#	HTML/files/*		... file index (5)
#	HTML/index.html		... index file (6)
#	HTML/mains.html		... main index (7)
#	HTML/null.html		... main null html (7)
#	HTML/$SRCS/		... source files (8)
#	HTML/$INCS/		... include file index (9)
#-------------------------------------------------------------------------
$'HTML = ($'cflag) ? $gzipped_suffix : 'html';
print STDERR "[", &'date, "] ", "Htags started\n" if ($'vflag);
#
# (0) make directories
#
print STDERR "[", &'date, "] ", "(0) making directories ...\n" if ($'vflag);
mkdir($dist, 0777) || &'error("cannot make directory '$dist'.") if (! -d $dist);
foreach $d ($SRCS, $INCS, $DEFS, $REFS, files, funcs) {
	mkdir("$dist/$d", 0775) || &'error("cannot make HTML directory") if (! -d "$dist/$d");
}
if ($'fflag || $'cflag) {
	mkdir("$dist/cgi-bin", 0775) || &'error("cannot make cgi-bin directory") if (! -d "$dist/cgi-bin");
}
#
# (1) make CGI program
#
if ($'fflag) {
	print STDERR "[", &'date, "] ", "(1) making CGI program ...\n" if ($'vflag);
	&makeprogram("$dist/cgi-bin/global.cgi") || &'error("cannot make CGI program.");
	chmod(0755, "$dist/cgi-bin/global.cgi") || &'error("cannot chmod CGI program.");
	unlink("$dist/cgi-bin/GTAGS", "$dist/cgi-bin/GRTAGS", "$dist/cgi-bin/GPATH");
	link("$dbpath/GTAGS", "$dist/cgi-bin/GTAGS") || &'copy("$dbpath/GTAGS", "$dist/cgi-bin/GTAGS") || &'error("cannot copy GTAGS.");
	link("$dbpath/GRTAGS", "$dist/cgi-bin/GRTAGS") || &'copy("$dbpath/GRTAGS", "$dist/cgi-bin/GRTAGS") || &'error("cannot copy GRTAGS.");
	link("$dbpath/GPATH", "$dist/cgi-bin/GPATH") || &'copy("$dbpath/GPATH", "$dist/cgi-bin/GPATH") || &'error("cannot copy GPATH.");
}
if ($'cflag) {
	&makehtaccess("$dist/.htaccess.skel") || &'error("cannot make .htaccess skelton.");
	&makeghtml("$dist/cgi-bin/ghtml.cgi") || &'error("cannot make unzip script.");
	chmod(0755, "$dist/cgi-bin/ghtml.cgi") || &'error("cannot chmod unzip script.");
}
#
# (2) make help file
#
print STDERR "[", &'date, "] ", "(2) making help.html ...\n" if ($'vflag);
&makehelp("$dist/help.html");
#
# (3) make function entries ($DEFS/* and $REFS/*)
#     MAKING TAG CACHE
#
print STDERR "[", &'date, "] ", "(3) making duplicate entries ...\n" if ($'vflag);
sub suddenly { &'clean(); exit 1}
$SIG{'INT'} = 'suddenly';
$SIG{'QUIT'} = 'suddenly';
$SIG{'TERM'} = 'suddenly';
&cache'open(100000);
$func_total = &makedupindex($dist);
print STDERR "Total $func_total functions.\n" if ($'vflag);
#
# (4) make function index (funcs.html and funcs/*)
#     PRODUCE @funcs
#
print STDERR "[", &'date, "] ", "(4) making function index ...\n" if ($'vflag);
$func_total = &makefuncindex($dist, "$dist/funcs.html", $func_total);
print STDERR "Total $func_total functions.\n" if ($'vflag);
#
# (5) make file index (files.html and files/*)
#     PRODUCE @files %includes
#
print STDERR "[", &'date, "] ", "(5) making file index ...\n" if ($'vflag);
$file_total = &makefileindex($dist, "$dist/files.html", "$dist/$INCS");
print STDERR "Total $file_total files.\n" if ($'vflag);
#
# [#] make a common part for mains.html and index.html
#     USING @funcs @files
#
print STDERR "[", &'date, "] ", "(#) making a common part ...\n" if ($'vflag);
$index = &makecommonpart($title);
#
# (6)make index file (index.html)
#
print STDERR "[", &'date, "] ", "(6) making index file ...\n" if ($'vflag);
&makeindex("$dist/index.html", $title, $index);
#
# (7) make main index (mains.html)
#
print STDERR "[", &'date, "] ", "(7) making main index ...\n" if ($'vflag);
&makemainindex("$dist/mains.html", $index);
&makenullhtml("$dist/null.html") if ($'hflag);
#
# (#) make anchor database
#
print STDERR "[", &'date, "] ", "(#) making temporary database ...\n" if ($'vflag);
&anchor'create();
#
# (8) make HTML files ($SRCS/*)
#     USING TAG CACHE, %includes and anchor database.
#
print STDERR "[", &'date, "] ", "(8) making hypertext from source code ...\n" if ($'vflag);
&makehtml($dist, $file_total);
&'clean();
print STDERR "[", &'date, "] ", "Done.\n" if ($'vflag);
if ($'cflag && $'vflag) {
	print STDERR "\n";
	print STDERR "[Information]\n";
	print STDERR "\n";
	print STDERR " You need to setup http server so that '*.ghtml' are treated\n";
	print STDERR " as gzipped files. Please see 'HTML/.htaccess.skel'.\n";
	print STDERR " Good luck!\n";
	print STDERR "\n";
}
exit 0;
#-------------------------------------------------------------------------
# SUBROUTINES
#-------------------------------------------------------------------------
#
# makeprogram: make CGI program
#
sub makeprogram {
	local($file) = @_;

	open(PROGRAM, ">$file") || &'error("cannot make CGI program.");
	$program = <<'END_OF_SCRIPT';
#!/usr/bin/perl
#------------------------------------------------------------------
# SORRY TO HAVE SURPRISED YOU!
# IF YOU SEE THIS UNREASONABLE FILE WHILE BROUSING, FORGET PLEASE.
# IF YOU ARE A ADMINISTRATOR OF THIS SITE, PLEASE SETUP HTTP SERVER
# SO THAT THIS SCRIPT CAN BE EXECUTED AS A CGI COMMAND. THANK YOU.
#------------------------------------------------------------------
$SRCS   = 'S';
$HTML	= '@HTML@';
$SEP    = ' ';	# source file path must not include $SEP charactor
$ESCSEP = &escape($SEP);
sub escape {
	local($c) = @_;
	'%' . sprintf("%x", ord($c));
}
print "Content-type: text/html\n\n";
print "<HTML><BODY>\n";
@pairs = split (/&/, $ENV{'QUERY_STRING'});
foreach $p (@pairs) {
	($name, $value) = split(/=/, $p);
	$value =~ tr/+/ /;
	$value =~ s/%([\dA-Fa-f][\dA-Fa-f])/pack("C", hex($1))/eg;
	$form{$name} = $value;
}
if ($form{'pattern'} eq '') {
	print "<H3>Pattern not specified. <A HREF=../mains.html>[return]</A></H3>\n";
	print "</BODY></HTML>\n";
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
	print "</BODY></HTML>\n";
	exit 0;
}
$cnt = 0;
print "<PRE>\n";
while (<PIPE>) {
	$cnt++;
	local($tag, $lno, $filename) = split;
	$filename =~ s/^\.\///;
	$filename =~ s/\//$ESCSEP/g;
	s/($tag)/<A HREF=..\/$SRCS\/$filename.$HTML#$lno>$1<\/A>/;
	print;
}
close(PIPE);
print "</PRE>\n";
if ($cnt == 0) {
	print "<H3>Pattern not found. <A HREF=../mains.html>[return]</A></H3>\n";
}
print "</BODY></HTML>\n";
exit 0;
#------------------------------------------------------------------
# SORRY TO HAVE SURPRISED YOU!
# IF YOU SEE THIS UNREASONABLE FILE WHILE BROUSING, FORGET PLEASE.
# IF YOU ARE A ADMINISTRATOR OF THIS SITE, PLEASE SETUP HTTP SERVER
# SO THAT THIS SCRIPT CAN BE EXECUTED AS A CGI COMMAND. THANK YOU.
#------------------------------------------------------------------
END_OF_SCRIPT

	$program =~ s/\@HTML\@/$'HTML/g;
	print PROGRAM $program;
	close(PROGRAM);
}
#
# makeghtml: make unzip script
#
sub makeghtml {
	local($file) = @_;
	open(PROGRAM, ">$file") || &'error("cannot make unzip script.");
	$program = <<'END_OF_SCRIPT';
#!/bin/sh
echo "content-type: text/html"
echo
gzip -S @HTML@ -d -c "$PATH_TRANSLATED"
END_OF_SCRIPT

	$program =~ s/\@HTML\@/$'HTML/g;
	print PROGRAM $program;
	close(PROGRAM);
}
#
# makehtaccess: make .htaccess skelton file.
#
sub makehtaccess {
	local($file) = @_;
	open(SKELTON, ">$file") || &'error("cannot make .htaccess skelton file.");
	$skelton = <<'END_OF_SCRIPT';
#
# Skelton file for .htaccess -- This file was generated by htags(1).
#
# Htags have made gzipped hypertext because you specified -c option.
# You need to setup http server so that these hypertext can be treated
# as gzipped files.
# There are many way to do it, but one of the method is to put .htaccess
# file in 'HTML' directory.
#
# Please rewrite XXX to the true value in your web site and rename this
# file to '.htaccess' and http server read this.
#
AddHandler htags-gzipped-html ghtml
Action htags-gzipped-html /XXX/cgi-bin/ghtml.cgi
END_OF_SCRIPT
	print SKELTON $skelton;
	close(SKELTON);
}
#
# makehelp: make help file
#
sub makehelp {
	local($file) = @_;

	open(HELP, ">$file") || &'error("cannot make help file.");
	print HELP $'begin_html;
	print HELP &'set_header(0, 'HELP');
	print HELP $'begin_body;
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
	print HELP $'end_body;
	print HELP $'end_html;
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
	$filename = &'path2url($filename);
	$_[0] =~ s/^$tag/<A HREF=..\/$'SRCS\/$filename#$lno>$tag<\/A>/;
}
sub makedupindex {
	local($dist) = @_;
	local($count) = 0;

	foreach $db ('GRTAGS', 'GTAGS') {
		local($kind) = $db eq 'GTAGS' ? "definitions" : "references";
		local($option) = $db eq 'GTAGS' ? '' : 'r';
		local($prev) = '';
		local($first_line);
		local($writing) = 0;

		$count = 0;
		local($command) = "global -nx$option '.*' | sort +0 -1 +2 -3 +1n -2";
		open(LIST, "$command |") || &'error("cannot fork.");
		while (<LIST>) {
			chop;
			local($tag, $lno, $filename) = split;
			if ($prev ne $tag) {
				$count++;
				print STDERR " [$count] adding $tag $kind.\n" if ($'vflag);
				if ($writing) {
					print FILE "</PRE>\n";
					print FILE $'end_body;
					print FILE $'end_html;
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
					local($type) = ($db eq 'GTAGS') ? $'DEFS : $'REFS;
					if ($'cflag) {
						open(FILE, "| gzip -c >$dist/$type/$tag.$'HTML") || &'error("cannot make file '$dist/$type/$tag.$'HTML'.");
					} else {
						open(FILE, ">$dist/$type/$tag.$'HTML") || &'error("cannot make file '$dist/$type/$tag.$'HTML'.");
					}
					$writing = 1;
					print FILE $'begin_html;
					print FILE &'set_header(0, $tag);
					print FILE $'begin_body;
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
		if ($?) { &'error("'$command' failed."); }
		if ($writing) {
			print FILE "</PRE>\n";
			print FILE $'end_body;
			print FILE $'end_html;
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
#	i)	dist		distribution directory
#	i)	file		function index file
#	i)	total		functions total
#	gi)	tag cache
#	go)	@funcs
#
sub makefuncindex {
	local($dist, $file, $total) = @_;
	local($count) = 0;
	local($indexlink) = "../mains.$'HTML";

	open(FUNCTIONS, ">$file") || &'error("cannot make function index '$file'.");
	print FUNCTIONS $'begin_html;
	print FUNCTIONS &'set_header(0, 'FUNCTION INDEX', $'default_view);
	print FUNCTIONS $'begin_body;
	print FUNCTIONS "<H2>FUNCTION INDEX</H2>\n";
	print FUNCTIONS "<OL>\n" if (!$'aflag);
	local($old) = select(FUNCTIONS);
	local($command) = "global -c";
	open(TAGS, "$command |") || &'error("cannot fork.");
	local($alpha, $alpha_f);
	@funcs = ();	# [A][B][C]...
	while (<TAGS>) {
		$count++;
		chop;
		local($tag) = $_;
		print STDERR " [$count/$total] adding $tag\n" if ($'vflag);
		if ($'aflag && ($alpha eq '' || $tag !~ /^$alpha/)) {
			if ($alpha) {
				print ALPHA "</OL>\n";
				print ALPHA "<A HREF=$indexlink TARGET=_self>[index]</A>\n";
				print ALPHA "$'begin_script$'rewrite_href_funcs$'end_script";
				print ALPHA $'end_body;
				print ALPHA $'end_html;
				close(ALPHA);
			}
			# for multi-byte code
			local($c0, $c1);
			$c0 = substr($tag, 0, 1);
			if (ord($c0) > 127) {
				$c1 = substr($tag, 1, 1);
				$alpha   = $c0 . $c1;
				$alpha_f = "" . ord($c0) . ord($c1);
			} else {
				$alpha = $alpha_f = $c0;
			}
			push(@funcs, "<A HREF=funcs/$alpha_f.$'HTML TARGET=_self>[$alpha]</A>\n");
			if ($'cflag) {
				open(ALPHA, "| gzip -c >$dist/funcs/$alpha_f.$'HTML") || &'error("cannot make alphabetical function index.");
			} else {
				open(ALPHA, ">$dist/funcs/$alpha_f.$'HTML") || &'error("cannot make alphabetical function index.");
			}
			print ALPHA $'begin_html;
			print ALPHA &'set_header(0, $alpha, $'default_view);
			print ALPHA $'begin_body;
			print ALPHA "<H2>[$alpha]</H2>\n";
			print ALPHA "<A HREF=$indexlink TARGET=_self>[index]</A>\n";
			print ALPHA "<OL>\n";
			select(ALPHA);
		}
		local($line) = &cache'get('GTAGS', $tag);
		if (!$line) {
			print "<LI><A HREF=", ($'aflag) ? "../" : "", "$'DEFS/$tag.$'HTML>$tag</A>\n";
		} else {
			local($tag, $lno, $filename) = split(/[ \t]+/, $line);
			$filename = &'path2url($filename);
			print "<LI><A HREF=", ($'aflag) ? "../" : "", "$'SRCS/$filename#$lno>$tag</A>\n";
		}
	}
	close(TAGS);
	if ($?) { &'error("'$command' failed."); }
	select($old);
	if ($'aflag) {
		print ALPHA "</OL>\n";
		print ALPHA "<A HREF=$indexlink TARGET=_self>[index]</A>\n";
		print ALPHA "$'begin_script$'rewrite_href_funcs$'end_script";
		print ALPHA $'end_body;
		print ALPHA $'end_html;
		close(ALPHA);

		print FUNCTIONS @funcs;
	}
	print FUNCTIONS "</OL>\n" if (!$'aflag);
	print FUNCTIONS $'end_body;
	print FUNCTIONS $'end_html;
	close(FUNCTIONS);
	$count;
}
#
# makefileindex: make file index
#
#	i)	dist		distribution directory
#	i)	file		file name
#	i)	$incdir		$INC directory
#	go)	@files
#	go)	%includes
#
sub makefileindex {
	local($dist, $file, $incdir) = @_;
	local($count) = 0;
	local($indexlink) = "../mains.$'HTML";
	local(@dirstack, @fdstack);
	local($command) = "gtags --find | sort";
	open(FIND, "$command |") || &'error("cannot fork.");
	open(FILES, ">$file") || &'error("cannot make file '$file'.");
	print FILES $'begin_html;
	print FILES &'set_header(0, 'FILE INDEX', $'default_view);
	print FILES $'begin_body;
	print FILES "<H2>FILE INDEX</H2>\n";
	print FILES "<OL>\n";

	local($org) = select(FILES);
	local(@push, @pop, $file);

	while (<FIND>) {
		$count++;
		chop;
		s/^\.\///;
		print STDERR " [$count] adding $_\n" if ($'vflag);
		@push = split('/');
		$file = pop(@push);
		@pop  = @dirstack;
		while ($push[0] && $pop[0] && $push[0] eq $pop[0]) {
			shift @push;
			shift @pop;
		}
		if (@push || @pop) {
			while (@pop) {
				pop(@dirstack);
				local($parent) = (@dirstack) ? &path2url(join('/', @dirstack)) : $indexlink;
				print "</OL>\n";
				print "<A HREF=$parent TARGET=_self>[..]</A>\n";
				print "$'begin_script$'rewrite_href_files$'end_script" if (@dirstack == 0);
				print $'end_body;
				print $'end_html;
				$path = pop(@fdstack);
				close($path);
				select($fdstack[$#fdstack]) if (@fdstack);
				pop(@pop);
			}
			while (@push) {
				local($parent) = (@dirstack) ? &path2url(join('/', @dirstack)) : $indexlink;
				push(@dirstack, shift @push);
				$path = join('/', @dirstack);
				$cur = "$dist/files/" . &path2file($path);
				local($li) = "<LI><A HREF=" . (@dirstack == 1 ? 'files/' : '') . &path2url($path) . " TARGET=_self>$path/</A>\n";
				if (@dirstack == 1) {
					push(@files, $li);
				} else {
					print $li;
				}
				if ($'cflag) {
					open($cur, "| gzip -c >'$cur'") || &'error("cannot make directory index.");
				} else {
					open($cur, ">$cur") || &'error("cannot make directory index.");
				}
				select($cur);
				push(@fdstack, $cur);
				print $'begin_html;
				print &'set_header(0, "$path", $'default_view);
				print $'begin_body;
				print "<H2>$path/</H2>\n";
				print "<A HREF=$parent  TARGET=_self>[..]</A>\n";
				print "<OL>\n";
			}
		}
		# collect include files.
		if (/.*\.h$/) {
			if (! defined $includes{$file}) {
				$includes{$file} = $_;
			} else {
				# duplicate entries
				$includes{$file} = "$includes{$file}\n$_";
			}
		}
		local($url) = &path2url($_);
		local($li) = "<LI><A HREF=" . (@dirstack == 0 ? '' : '../') . "S/$url>$_</A>\n";
		if (@dirstack == 0) {
			push(@files, $li);
		} else {
			print $li;
		}
	}
	close(FIND);
	while (@dirstack) {
		pop(@dirstack);
		local($parent) = (@dirstack) ? &path2url(join('/', @dirstack)) : $indexlink;
		print "</OL>\n";
		print "<A HREF=$parent TARGET=_self>[..]</A>\n";
		print "$'begin_script$'rewrite_href_files$'end_script" if (@dirstack == 0);
		print $'end_body;
		print $'end_html;
		$path = pop(@fdstack);
		close($path);
		select($fdstack[$#fdstack]) if (@fdstack);
	}
	print FILES @files;
	print FILES "</OL>\n";
	print FILES $'end_body;
	print FILES $'end_html;
        close(FILES);

	select($org);
	foreach $last (keys %includes) {
		local(@incs) = split(/\n/, $includes{$last});
		if (@incs > 1) {
			if ($'cflag) {
				open(INCLUDE, "| gzip -c >$incdir/$last.$'HTML") || &'error("cannot open file '$incdir/$last.$'HTML'.");
			} else {
				open(INCLUDE, ">$incdir/$last.$'HTML") || &'error("cannot open file '$incdir/$last.$'HTML'.");
			}
			print INCLUDE $'begin_html;
			print INCLUDE &'set_header(0, $last);
			print INCLUDE $'begin_body;
			print INCLUDE "<PRE>\n";
			foreach $filename (@incs) {
				local($path) = &'path2url($filename);
				print INCLUDE "<A HREF=../$'SRCS/$path>$filename</A>\n";
			}
			print INCLUDE "</PRE>\n";
			print INCLUDE $'end_body;
			print INCLUDE $'end_html;
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

	$index .= "<H1>$'title_begin$'title$'title_end</H1>\n";
	$index .= "<P ALIGN=right>";
	$index .= "Last updated " . &'date . "<BR>\n";
	$index .= "This hypertext was generated by <A HREF=http://wafu.netgate.net/tama/unix/global.html TARGET=_top>GLOBAL</A>.<BR>\n";
	$index .= $'begin_script;
	$index .= "if (parent.frames.length && parent.mains == self)\n";
	$index .= "	document.write($'langle+'A HREF=mains.html TARGET=_top'+$'rangle+'[No frame version is here.]'+$'langle+'/A'+$'rangle)\n";
	$index .= $'end_script;
	$index .= "</P>\n<HR>\n";
	if ($'fflag) {
		$index .= "<H2>FUNCTION SEARCH</H2>\n";
		$index .= "Please input function name and select [Search]. POSIX's regular expression is allowed.<P>\n"; 
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
	local($command) = "global -nx main | sort +0 -1 +2 -3 +1n -2";
	open(PIPE, "$command |") || &'error("cannot fork.");
	while (<PIPE>) {
		local($nouse, $lno, $filename) = split;
		$nouse = '';	# to make perl quiet
		$filename = &'path2url($filename);
		s/(main)/<A HREF=$'SRCS\/$filename#$lno>$1<\/A>/;
		$index .= $_;
	}
	close(PIPE);
	if ($?) { &'error("'$command' failed."); }
	$index .= "</PRE>\n<HR>\n<H2>FUNCTIONS</H2>\n";
	if ($'aflag) {
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

	open(FRAME, ">$file") || &'error("cannot open file '$file'.");
	print FRAME $'begin_html;
	print FRAME "<HEAD><TITLE>$title</TITLE></HEAD>\n";
	print FRAME "<FRAMESET COLS='200,*'>\n";
	print FRAME "<FRAMESET ROWS='50%,50%'>\n";
	print FRAME "<FRAME NAME=funcs SRC=funcs.html>\n";
	print FRAME "<FRAME NAME=files SRC=files.html>\n";
	print FRAME "</FRAMESET>\n";
	if ($'hflag) {
		print FRAME "<FRAMESET ROWS='50,*'>\n";
		print FRAME "<FRAME NAME=title SRC=null.html BORDER=0 SCROLLING=no>\n";
		print FRAME "<FRAME NAME=mains SRC=mains.html BORDER=0>\n";
		print FRAME "</FRAMESET>\n";
	} else {
		print FRAME "<FRAME NAME=mains SRC=mains.html>\n";
	}
	print FRAME "<NOFRAMES>\n$index</NOFRAMES>\n";
	print FRAME "</FRAMESET>\n";
	print FRAME $'end_html;
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

	open(INDEX, ">$file") || &'error("cannot create file '$file'.");
	print INDEX $'begin_html;
	print INDEX &'set_header(1, $title);
	print INDEX $'begin_body;
	print INDEX $index;
	print INDEX $'end_body;
	print INDEX $'end_html;
	close(INDEX);
}
#
# makenullhtml: make null html
#
#	i)	$file	file name
#
sub makenullhtml {
	local($file) = @_;

	open(NULL, ">$file") || &'error("cannot create file '$file'.");
	print NULL "<HTML><BODY></BODY></HTML>\n";
	close(NULL);
}
#
# makehtml: make html files
#
#	i)	total	number of files.
#
sub makehtml {
	local($dist, $total) = @_;
	local($count) = 0;

	open(FIND, "$'findcom |") || &'error("cannot fork.");
	while (<FIND>) {
		chop;
		$count++;
		local($path) = $_;
		$path =~ s/^\.\///;
		print STDERR " [$count/$total] converting $_\n" if ($'vflag);
		$path = &'path2file($path);
		&convert'src2html($_, "$dist/$'SRCS/$path");
	}
	close(FIND);
	if ($?) { &'error("cannot traverse directory."); }
}
#=========================================================================
# CONVERT PACKAGE
#=========================================================================
package convert;
#
# src2html: convert source code into HTML
#
#	i)	$file	source file	- Read from
#	i)	$hfile	HTML file	- Write to
#	gi)	%includes
#			pairs of include file and the path
#
sub src2html {
	local($file, $hfile) = @_;
	local($ncol) = $'ncol;
	local($tabs) = $'tabs;
	local(%ctab) = ('&', '&amp;', '<', '&lt;', '>', '&gt;');
	local($expand) = &'usable('expand') ? 'expand' : 'gtags --expand';
	local($isjava) = ($file =~ /\.java$/) ? 1 : 0;
	local($reserved_words) = ($isjava) ? $'java_reserved_words : $'c_reserved_words;

	if ($'cflag) {
		open(HTML, "| gzip -c >'$hfile'") || &'error("cannot create file '$hfile'.");
	} else {
		open(HTML, ">$hfile") || &'error("cannot create file '$hfile'.");
	}
	local($old) = select(HTML);
	#
	# load tags belonging to this file.
	#
	&anchor'load($file);
	open(SRC, "$expand -$tabs '$file' |") || &'error("cannot fork.");
	#
	# print the header
	#
	$file =~ s/^\.\///;
	print $'begin_html;
	print &'set_header(1, $file);
	print $'begin_body;
	print "<A NAME=TOP><H2>$file</H2>\n";
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
	while (<SRC>) {
		local($converted);
		s/\r$//;
		# make link for include file
		if (!$INCOMMENT && /^#include/) {
			local($last, $sep) = m![</"]([^</"]+)([">])!;
			if (defined $'includes{$last}) {
				local($link, $suffix);
				if ($'includes{$last}) {
					$link = &'path2url($'includes{$last});
				} else {
					$link = "../$'INCS/$last.$'HTML";
				}
				if ($sep eq '"') {
					s!"(.*$last)"!"<A HREF=$link>$1</A>"!;
				} else {
					s!<(.*$last)>!&lt;<A HREF=$link>$1</A>&gt;!;
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
		s/\b($reserved_words)\b/$'reserved_begin$1$'reserved_end/go if ($sharp ne '#include');
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
					$filename = &'path2url($filename);
					$href = "<A HREF=../$'SRCS/$filename#$lno>$TAG</A>";
				} else {
					local($dir) = ($TYPE eq 'D') ? $'REFS : $'DEFS;
					$href = "<A HREF=../$dir/$TAG.$'HTML>$TAG</A>";
				}
				# set tag marks and save hyperlink into @links
				if (ord($TAG) > 127) {	# for multi-byte code
					if (s/([\x00-\x7f])$TAG([ \t]*\()/$1\005$count\005$2/ || s/([\x00-\x7f])$TAG([\x00-\x7f])/$1\005$count\005$2/) {
						$count++;
						push(@links, $href);
					} else {
						print STDERR "Error: $file $LNO $TAG($TYPE) tag must exist.\n" if ($'wflag);
					}
				} else {
					if (s/\b$TAG([ \t]*\()/\005$count\005$1/ || s/\b$TAG\b/\005$count\005/ || s/\b_$TAG\b/_\005$count\005/)
					{
						$count++;
						push(@links, $href);
					} else {
						print STDERR "Error: $file $LNO $TAG($TYPE) tag must exist.\n" if ($'wflag);
					}
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
		if ($define_line) {
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
	print $'end_body;
	print $'end_html;
	close(SRC);
	if ($?) { &'error("cannot open file '$file'."); }
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
#	\005	line comment
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
	$line_comment = '';
	if (s!(//.*)$!\005!) {
		$line_comment = $1;
		# ^     //   /*   $
		#       (1)  (2)	... (1) invalidate (2).
		$INCOMMENT = 0;
	}
}
#
# unprotect_line: recover quoted strings
#
#	i)	$_	source line
#
sub unprotect_line {
	local($s);

	if ($line_comment) {
		s/\005/$'comment_begin$line_comment$'comment_end/;
	}
	while (@comments) {
		$s = shift @comments;
		# nested tag can be occured but no problem.
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
	local($command) = "btreeop -C $ANCH";
	open(ANCH, "| $command") || &'error("cannot fork.");
	local($fcount) = 1;
	local($fnumber);
	foreach $db ('GTAGS', 'GRTAGS') {
		local($type) = ($db eq 'GTAGS') ? 'D' : 'R';
		local($option) = ($db eq 'GTAGS') ? '' : 'r';
		local($command) = "global -nx$option '.*'";
		open(PIPE, "$command |") || &'error("cannot fork.");
		while (<PIPE>) {
			local($tag, $lno, $filename) = split;
			$fnumber = $PATHLIST{$filename};
			if (!$fnumber) {
				$PATHLIST{$filename} = $fnumber = $fcount++;
			}
			print ANCH "$fnumber $lno $tag $type\n";
		}
		close(PIPE);
		if ($?) { &'error("'$command' failed."); }
	}
	close(ANCH);
	if ($?) { &'error("'$command' failed."); }
}
#
# finish: remove anchors database
#
sub finish {
	unlink("$ANCH") if (defined($ANCH));
}
#
# load: load anchors belonging to specified file.
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
	local($command) = "btreeop -K $fnumber $ANCH";
	open(ANCH, "$command |") || &'error("cannot fork.");
	while (<ANCH>) {
		local($fnumber, $lno, $tag, $type) = split;
		local($line);
		# don't refer to macros which is defined in other C source.
		if ($type eq 'R' && ($line = &cache'get('GTAGS', $tag))) {
			local($nouse1, $nouse2, $f, $def) = split(/[ \t]+/, $line);
			if ($f !~ /\.h$/ && $f ne $file && $def =~ /^#/) {
				print STDERR "Information: $file $lno $tag($type) skipped, because this is a macro which is defined in other C source.\n" if ($'wflag);
				next;
			}
		}
		push(@ANCHORS, "$lno,$tag,$type");
	}
	close(ANCH);
	if ($?) {&'error("'$command' failed."); }
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
