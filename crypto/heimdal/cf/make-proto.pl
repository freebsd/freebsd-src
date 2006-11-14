# Make prototypes from .c files
# $Id: make-proto.pl,v 1.16 2002/09/19 19:29:42 joda Exp $

##use Getopt::Std;
require 'getopts.pl';

$brace = 0;
$line = "";
$debug = 0;
$oproto = 1;
$private_func_re = "^_";

do Getopts('o:p:dqR:P:') || die "foo";

if($opt_d) {
    $debug = 1;
}

if($opt_q) {
    $oproto = 0;
}

if($opt_R) {
    $private_func_re = $opt_R;
}

while(<>) {
    print $brace, " ", $_ if($debug);
    if(/^\#if 0/) {
	$if_0 = 1;
    }
    if($if_0 && /^\#endif/) {
	$if_0 = 0;
    }
    if($if_0) { next }
    if(/^\s*\#/) {
	next;
    }
    if(/^\s*$/) {
	$line = "";
	next;
    }
    if(/\{/){
	if (!/\}/) {
	    $brace++;
	}
	$_ = $line;
	while(s/\*\//\ca/){
	    s/\/\*(.|\n)*\ca//;
	}
	s/^\s*//;
	s/\s*$//;
	s/\s+/ /g;
	if($_ =~ /\)$/){
	    if(!/^static/ && !/^PRIVATE/){
		if(/(.*)(__attribute__\s?\(.*\))/) {
		    $attr = $2;
		    $_ = $1;
		} else {
		    $attr = "";
		}
		# remove outer ()
		s/\s*\(/</;
		s/\)\s?$/>/;
		# remove , within ()
		while(s/\(([^()]*),(.*)\)/($1\$$2)/g){}
		s/\<\s*void\s*\>/<>/;
		# remove parameter names 
		if($opt_P eq "remove") {
		    s/(\s*)([a-zA-Z0-9_]+)([,>])/$3/g;
		    s/\(\*(\s*)([a-zA-Z0-9_]+)\)/(*)/g;
		} elsif($opt_P eq "comment") {
		    s/([a-zA-Z0-9_]+)([,>])/\/\*$1\*\/$2/g;
		    s/\(\*([a-zA-Z0-9_]+)\)/(*\/\*$1\*\/)/g;
		}
		s/\<\>/<void>/;
		# add newlines before parameters
		s/,\s*/,\n\t/g;
		# fix removed ,
		s/\$/,/g;
		# match function name
		/([a-zA-Z0-9_]+)\s*\</;
		$f = $1;
		if($oproto) {
		    $LP = "__P((";
		    $RP = "))";
		} else {
		    $LP = "(";
		    $RP = ")";
		}
		# only add newline if more than one parameter
                if(/,/){ 
		    s/\</ $LP\n\t/;
		}else{
		    s/\</ $LP/;
		}
		s/\>/$RP/;
		# insert newline before function name
		s/(.*)\s([a-zA-Z0-9_]+ \Q$LP\E)/$1\n$2/;
		if($attr ne "") {
		    $_ .= "\n    $attr";
		}
		$_ = $_ . ";";
		$funcs{$f} = $_;
	    }
	}
	$line = "";
    }
    if(/\}/){
	$brace--;
    }
    if(/^\}/){
	$brace = 0;
    }
    if($brace == 0) {
	$line = $line . " " . $_;
    }
}

sub foo {
    local ($arg) = @_;
    $_ = $arg;
    s/.*\/([^\/]*)/$1/;
    s/[^a-zA-Z0-9]/_/g;
    "__" . $_ . "__";
}

if($opt_o) {
    open(OUT, ">$opt_o");
    $block = &foo($opt_o);
} else {
    $block = "__public_h__";
}

if($opt_p) {
    open(PRIV, ">$opt_p");
    $private = &foo($opt_p);
} else {
    $private = "__private_h__";
}

$public_h = "";
$private_h = "";

$public_h_header = "/* This is a generated file */
#ifndef $block
#define $block

";
if ($oproto) {
$public_h_header .= "#ifdef __STDC__
#include <stdarg.h>
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

";
} else {
    $public_h_header .= "#include <stdarg.h>

";
}

$private_h_header = "/* This is a generated file */
#ifndef $private
#define $private

";
if($oproto) {
$private_h_header .= "#ifdef __STDC__
#include <stdarg.h>
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

";
} else {
    $private_h_header .= "#include <stdarg.h>

";
}
foreach(sort keys %funcs){
    if(/^(main)$/) { next }
    if(/$private_func_re/) {
	$private_h .= $funcs{$_} . "\n\n";
	if($funcs{$_} =~ /__attribute__/) {
	    $private_attribute_seen = 1;
	}
    } else {
	$public_h .= $funcs{$_} . "\n\n";
	if($funcs{$_} =~ /__attribute__/) {
	    $public_attribute_seen = 1;
	}
    }
}

if ($public_attribute_seen) {
    $public_h_header .= "#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

";
}

if ($private_attribute_seen) {
    $private_h_header .= "#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

";
}


if ($public_h ne "") {
    $public_h = $public_h_header . $public_h . "#endif /* $block */\n";
}
if ($private_h ne "") {
    $private_h = $private_h_header . $private_h . "#endif /* $private */\n";
}

if($opt_o) {
    print OUT $public_h;
} 
if($opt_p) {
    print PRIV $private_h;
} 

close OUT;
close PRIV;
