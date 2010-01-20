# Make prototypes from .c files
# $Id: make-proto.pl 14183 2004-09-03 08:50:57Z lha $

##use Getopt::Std;
require 'getopts.pl';

$brace = 0;
$line = "";
$debug = 0;
$oproto = 1;
$private_func_re = "^_";

do Getopts('x:m:o:p:dqE:R:P:') || die "foo";

if($opt_d) {
    $debug = 1;
}

if($opt_q) {
    $oproto = 0;
}

if($opt_R) {
    $private_func_re = $opt_R;
}
%flags = (
	  'multiline-proto' => 1,
	  'header' => 1,
	  'function-blocking' => 0,
	  'gnuc-attribute' => 1,
	  'cxx' => 1
	  );
if($opt_m) {
    foreach $i (split(/,/, $opt_m)) {
	if($i eq "roken") {
	    $flags{"multiline-proto"} = 0;
	    $flags{"header"} = 0;
	    $flags{"function-blocking"} = 0;
	    $flags{"gnuc-attribute"} = 0;
	    $flags{"cxx"} = 0;
	} else {
	    if(substr($i, 0, 3) eq "no-") {
		$flags{substr($i, 3)} = 0;
	    } else {
		$flags{$i} = 1;
	    }
	}
    }
}

if($opt_x) {
    open(EXP, $opt_x);
    while(<EXP>) {
	chomp;
	s/\#.*//g;
	s/\s+/ /g;
	if(/^([a-zA-Z0-9_]+)\s?(.*)$/) {
	    $exported{$1} = $2;
	} else {
	    print $_, "\n";
	}
    }
    close EXP;
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
		    s/\s+\*/*/g;
		    s/\(\*(\s*)([a-zA-Z0-9_]+)\)/(*)/g;
		} elsif($opt_P eq "comment") {
		    s/([a-zA-Z0-9_]+)([,>])/\/\*$1\*\/$2/g;
		    s/\(\*([a-zA-Z0-9_]+)\)/(*\/\*$1\*\/)/g;
		}
		s/\<\>/<void>/;
		# add newlines before parameters
		if($flags{"multiline-proto"}) {
		    s/,\s*/,\n\t/g;
		} else {
		    s/,\s*/, /g;
		}
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
                if($flags{"multiline-proto"} && /,/){ 
		    s/\</ $LP\n\t/;
		}else{
		    s/\</ $LP/;
		}
		s/\>/$RP/;
		# insert newline before function name
		if($flags{"multiline-proto"}) {
		    s/(.*)\s([a-zA-Z0-9_]+ \Q$LP\E)/$1\n$2/;
		}
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

$public_h_header .= "/* This is a generated file */
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
$public_h_trailer = "";

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
$private_h_trailer = "";

foreach(sort keys %funcs){
    if(/^(main)$/) { next }
    if(!defined($exported{$_}) && /$private_func_re/) {
	$private_h .= $funcs{$_} . "\n\n";
	if($funcs{$_} =~ /__attribute__/) {
	    $private_attribute_seen = 1;
	}
    } else {
	if($flags{"function-blocking"}) {
	    $fupper = uc $_;
	    if($exported{$_} =~ /proto/) {
		$public_h .= "#if !defined(HAVE_$fupper) || defined(NEED_${fupper}_PROTO)\n";
	    } else {
		$public_h .= "#ifndef HAVE_$fupper\n";
	    }
	}
	$public_h .= $funcs{$_} . "\n";
	if($funcs{$_} =~ /__attribute__/) {
	    $public_attribute_seen = 1;
	}
	if($flags{"function-blocking"}) {
	    $public_h .= "#endif\n";
	}
	$public_h .= "\n";
    }
}

if($flags{"gnuc-attribute"}) {
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
}
if($flags{"cxx"}) {
    $public_h_header .= "#ifdef __cplusplus
extern \"C\" {
#endif

";
    $public_h_trailer .= "#ifdef __cplusplus
}
#endif

";

}
if ($opt_E) {
    $public_h_header .= "#ifndef $opt_E
#if defined(_WIN32)
#define $opt_E _stdcall
#else
#define $opt_E
#endif
#endif

";
    
    $private_h_header .= "#ifndef $opt_E
#if defined(_WIN32)
#define $opt_E _stdcall
#else
#define $opt_E
#endif
#endif

";
}
    
if ($public_h ne "" && $flags{"header"}) {
    $public_h = $public_h_header . $public_h . 
	$public_h_trailer . "#endif /* $block */\n";
}
if ($private_h ne "" && $flags{"header"}) {
    $private_h = $private_h_header . $private_h .
	$private_h_trailer . "#endif /* $private */\n";
}

if($opt_o) {
    print OUT $public_h;
} 
if($opt_p) {
    print PRIV $private_h;
} 

close OUT;
close PRIV;
