#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);
use Cwd;

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
$origdir = cwd;
chdir dirname($0);
$file = basename($0, '.PL');
$file .= '.com' if $^O eq 'VMS';

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{startperl}
    eval 'exec $Config{perlpath} -S \$0 \${1+"\$@"}'
	if \$running_under_some_shell;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';
#
# pod2latex, version 1.1
# by Taro Kawagish (kawagish@imslab.co.jp),  Jan 11, 1995.
#
# pod2latex filters Perl pod documents to LaTeX documents.
#
# What pod2latex does:
# 1. Pod file 'perl_doc_entry.pod' is filtered to 'perl_doc_entry.tex'.
# 2. Indented paragraphs are translated into
#    '\begin{verbatim} ... \end{verbatim}'.
# 3. '=head1 heading' command is translated into '\section{heading}'
# 4. '=head2 heading' command is translated into '\subsection*{heading}'
# 5. '=over N' command is translated into
#        '\begin{itemize}'	if following =item starts with *,
#        '\begin{enumerate}'	if following =item starts with 1.,
#        '\begin{description}'	if else.
#      (indentation level N is ignored.)
# 6. '=item * heading' command is translated into '\item heading',
#    '=item 1. heading' command is translated into '\item heading',
#    '=item heading' command(other) is translated into '\item[heading]'.
# 7. '=back' command is translated into
#        '\end{itemize}'	if started with '\begin{itemize}',
#        '\end{enumerate}'	if started with '\begin{enumerate}',
#        '\end{description}'	if started with '\begin{description}'.
# 8. other paragraphs are translated into strings with TeX special characters
#    escaped.
# 9. In heading text, and other paragraphs, the following translation of pod
#    quotes are done, and then TeX special characters are escaped after that.
#      I<text> to {\em text\/},
#      B<text> to {\bf text},
#      S<text> to text1,
#        where text1 is a string with blank characters replaced with ~,
#      C<text> to {\tt text2},
#        where text2 is a string with TeX special characters escaped to
#        obtain a literal printout,
#      E<text> (HTML escape) to TeX escaped string,
#      L<text> to referencing string as is done by pod2man,
#      F<file> to {\em file\/},
#      Z<> to a null string,
# 10. those headings are indexed:
#       '=head1 heading'   =>  \section{heading}\index{heading}
#       '=head2 heading'   =>  \subsection*{heading}\index{heading}
#                 only when heading does not match frequent patterns such as
#                 DESCRIPTION, DIAGNOSTICS,...
#       '=item heading'   =>  \item{heading}\index{heading}
#
# Usage:
#     pod2latex perl_doc_entry.pod
# this will write to a file 'perl_doc_entry.tex'.
#
# To LaTeX:
# The following commands need to be defined in the preamble of the LaTeX
# document:
# \def\C++{{\rm C\kern-.05em\raise.3ex\hbox{\footnotesize ++}}}
# \def\underscore{\leavevmode\kern.04em\vbox{\hrule width 0.4em height 0.3pt}}
# and \parindent should be set zero:
# \setlength{\parindent}{0pt}
#
# Note:
# This script was written modifing pod2man.
#
# Bug:
# If HTML escapes E<text> other than E<amp>,E<lt>,E<gt>,E<quot> are used
# in C<>, translation will produce wrong character strings.
# Translation of HTML escapes of various European accents might be wrong.


$/ = "";			# record separator is blank lines
# TeX special characters.
##$tt_ables = "!@*()-=+|;:'\"`,./?<>";
$backslash_escapables = "#\$%&{}_";
$backslash_escapables2 = "#\$%&{}";	# except _
##$nonverbables = "^\\~";
##$bracketesc = "[]";
##@tex_verb_fences = unpack("aaaaaaaaa","|#@!*+?:;");

@head1_freq_patterns		# =head1 patterns which need not be index'ed
    = ("AUTHOR","Author","BUGS","DATE","DESCRIPTION","DIAGNOSTICS",
       "ENVIRONMENT","EXAMPLES","FILES","INTRODUCTION","NAME","NOTE",
       "SEE ALSO","SYNOPSIS","WARNING");

$indent = 0;

# parse the pods, produce LaTeX.

open(POD,"<$ARGV[0]") || die "cant open $ARGV[0]";
($pod=$ARGV[0]) =~ s/\.pod$//;
open(LATEX,">$pod.tex");
&do_hdr();

$cutting = 1;
$begun = "";
while (<POD>) {
    if ($cutting) {
	next unless /^=/;
	$cutting = 0;
    }
    if ($begun) {
       if (/^=end\s+$begun/) {
           $begun = "";
       }
       elsif ($begun =~ /^(tex|latex)$/) {
           print LATEX $_;
       }
       next;
    }
    chop;
    length || (print LATEX  "\n") && next;

    # translate indented lines as a verabatim paragraph
    if (/^\s/) {
	@lines = split(/\n/);
	print LATEX  "\\begin{verbatim}\n";
	for (@lines) {
	    1 while s
		{^( [^\t]* ) \t ( \t* ) }
		{ $1 . ' ' x (8 - (length($1)%8) + 8*(length($2))) }ex;
	    print LATEX  $_,"\n";
	}
	print LATEX  "\\end{verbatim}\n";
	next;
    }

    if (/^=for\s+(\S+)\s*/s) {
	if ($1 eq "tex" or $1 eq "latex") {
	    print LATEX $',"\n";
	} else {
	    # ignore unknown for
	}
	next;
    }
    elsif (/^=begin\s+(\S+)\s*/s) {
	$begun = $1;
	if ($1 eq "tex" or $1 eq "latex") {
	    print LATEX $'."\n";
	}
	next;
    }

    # preserve '=item' line with pod quotes as they are.
    if (/^=item/) {
	($bareitem = $_) =~ s/^=item\s*//;
    }

    # check for things that'll hosed our noremap scheme; affects $_
    &init_noremap();

    # expand strings "func()" as pod quotes.
    if (!/^=item/) {
	# first hide pod escapes.
	# escaped strings are mapped into the ones with the MSB's on.
	s/([A-Z]<[^<>]*>)/noremap($1)/ge;

	# func() is a reference to a perl function
	s{\b([:\w]+\(\))}{I<$1>}g;
	# func(n) is a reference to a man page
	s{(\w+)(\([^\s,\051]+\))}{I<$1>$2}g;
	# convert simple variable references
#	s/([\$\@%][\w:]+)/C<$1>/g;
#	s/\$[\w:]+\[[0-9]+\]/C<$&>/g;

	if (m{ ([\-\w]+\([^\051]*?[\@\$,][^\051]*?\))
	       }x && $` !~ /([LCI]<[^<>]*|-)$/ && !/^=\w/)
	{
	    warn "``$1'' should be a [LCI]<$1> ref";
	}
	while (/(-[a-zA-Z])\b/g && $` !~ /[\w\-]$/) {
	    warn "``$1'' should be [CB]<$1> ref";
	}

	# put back pod quotes so we get the inside of <> processed;
	$_ = &clear_noremap($_);
    }


    # process TeX special characters

    # First hide HTML quotes E<> since they can be included in C<>.
    s/(E<[^<>]+>)/noremap($1)/ge;

    # Then hide C<> type literal quotes.
    # String inside of C<> will later be expanded into {\tt ..} strings
    # with TeX special characters escaped as needed.
    s/(C<[^<>]*>)/&noremap($1)/ge;

    # Next escape TeX special characters including other pod quotes B< >,...
    #
    # NOTE: s/re/&func($str)/e evaluates $str just once in perl5.
    # (in perl4 evaluation takes place twice before getting passed to func().)

    # - hyphen => ---
    s/(\S+)(\s+)-+(\s+)(\S+)/"$1".&noremap(" --- ")."$4"/ge;
    # '-', '--', "-"  =>  '{\tt -}', '{\tt --}', "{\tt -}"
##    s/("|')(\s*)(-+)(\s*)\1/&noremap("$1$2\{\\tt $3\}$4$1")/ge;
## changed Wed Jan 25 15:26:39 JST 1995
    # '-', '--', "-"  =>  '$-$', '$--$', "$-$"
    s/(\s+)(['"])(-+)([^'"\-]*)\2(\s+|[,.])/"$1$2".&noremap("\$$3\$")."$4$2$5"/ge;
    s/(\s+)(['"])([^'"\-]*)(-+)(\s*)\2(\s+|[,.])/"$1$2$3".&noremap("\$$4\$")."$5$2$6"/ge;
    # (--|-)  =>  ($--$|$-$)
    s/(\s+)\((-+)([=@%\$\+\\\|\w]*)(-*)([=@%\$\+\\\|\w]*)\)(\s+|[,.])/"$1\(".&noremap("\$$2\$")."$3".&noremap("\$$4\$")."$5\)$6"/ge;
    # numeral -  =>  $-$
    s/(\(|[0-9]+|\s+)-(\s*\(?\s*[0-9]+)/&noremap("$1\$-\$$2")/ge;
    # -- in quotes  =>  two separate -
    s/B<([^<>]*)--([^<>]*)>/&noremap("B<$1\{\\tt --\}$2>")/ge;

    # backslash escapable characters except _.
    s/([$backslash_escapables2])/&noremap("\\$1")/ge;
    s/_/&noremap("\\underscore{}")/ge;		# a litle thicker than \_.
    # quote TeX special characters |, ^, ~, \.
    s/\|/&noremap("\$|\$")/ge;
    s/\^/&noremap("\$\\hat{\\hspace{0.4em}}\$")/ge;
    s/\~/&noremap("\$\\tilde{\\hspace{0.4em}}\$")/ge;
    s/\\/&noremap("\$\\backslash{}\$")/ge;
    # quote [ and ] to be used in \item[]
    s/([\[\]])/&noremap("{\\tt $1}")/ge;
    # characters need to be treated differently in TeX
    # keep * if an item heading
    s/^(=item[ \t]+)[*]((.|\n)*)/"$1" . &noremap("*") . "$2"/ge;
    s/[*]/&noremap("\$\\ast\$")/ge;	# other *

    # hide other pod quotes.
    s/([ABD-Z]<[^<>]*>)/&noremap($1)/ge;

    # escape < and > as math strings,
    # now that we are done with hiding pod <> quotes.
    s/</&noremap("\$<\$")/ge;
    s/>/&noremap("\$>\$")/ge;

    # put it back so we get the <> processed again;
    $_ = &clear_noremap($_);


    # Expand pod quotes recursively:
    # (1) type face directives [BIFS]<[^<>]*> to appropriate TeX commands,
    # (2) L<[^<>]*> to reference strings,
    # (3) C<[^<>]*> to TeX literal quotes,
    # (4) HTML quotes E<> inside of C<> quotes.

    # Hide E<> again since they can be included in C<>.
    s/(E<[^<>]+>)/noremap($1)/ge;

    $maxnest = 10;
    while ($maxnest-- && /[A-Z]</) {

	# bold and italic quotes
	s/B<([^<>]*)>/"{\\bf $1}"/eg;
	s#I<([^<>]*)>#"{\\em $1\\/}"#eg;

	# files and filelike refs in italics
	s#F<([^<>]*)>#"{\\em $1\\/}"#eg;

	# no break quote -- usually we want C<> for this
	s/S<([^<>]*)>/&nobreak($1)/eg;

	# LREF: a manpage(3f)
	s:L<([a-zA-Z][^\s\/]+)(\([^\)]+\))?>:the {\\em $1\\/}$2 manpage:g;

	# LREF: an =item on another manpage
	s{
	    L<([^/]+)/([:\w]+(\(\))?)>
	} {the C<$2> entry in the I<$1> manpage}gx;

	# LREF: an =item on this manpage
	s{
	   ((?:L</([:\w]+(\(\))?)>
	    (,?\s+(and\s+)?)?)+)
	} { &internal_lrefs($1) }gex;

	# LREF: a =head2 (head1?), maybe on a manpage, maybe right here
	# the "func" can disambiguate
	s{
	    L<(?:([a-zA-Z]\S+?) /)?"?(.*?)"?>
	}{
	    do {
		$1 	# if no $1, assume it means on this page.
		    ?  "the section on I<$2> in the I<$1> manpage"
		    :  "the section on I<$2>"
	    } 
	}gex;

	s/Z<>/\\&/g;		# the "don't format me" thing

	# comes last because not subject to reprocessing
	s{
	    C<([^<>]*)>
	}{
	    do {
		($str = $1) =~ tr/\200-\377/\000-\177/; #normalize hidden stuff
		# expand HTML escapes if any;
		# WARNING: if HTML escapes other than E<amp>,E<lt>,E<gt>,
		# E<quot> are in C<>, they will not be printed correctly.
		$str = &expand_HTML_escapes($str);
		$strverb = &alltt($str);    # Tex verbatim escape of a string.
		&noremap("$strverb");
	    }
	}gex;

#	if ( /C<([^<>]*)/ ) {
#	    $str = $1;
#	    if ($str !~ /\|/) {		# if includes |
#		s/C<([^<>]*)>/&noremap("\\verb|$str|")/eg;
#	    } else {
#		print STDERR "found \| in C<.*> at paragraph $.\n";
#		# find a character not contained in $str to use it as a
#		# separator of the \verb
#		($chars = $str) =~ s/(\W)/\\$1/g;
#		## ($chars = $str) =~ s/([\$<>,\|"'\-^{}()*+?\\])/\\$1/g;
#		@fence = grep(!/[ $chars]/,@tex_verb_fences);
#		s/C<([^<>]*)>/&noremap("\\verb$fence[0]$str$fence[0]")/eg;
#	    }
#	}
    }


    # process each pod command
    if (s/^=//) {				# if a command
	s/\n/ /g;
	($cmd, $rest) = split(' ', $_, 2);
	$rest =~ s/^\s*//;
	$rest =~ s/\s*$//;

	if (defined $rest) {
	    &escapes;
	}

	$rest = &clear_noremap($rest);
	$rest = &expand_HTML_escapes($rest);

	if ($cmd eq 'cut') {
	    $cutting = 1;
	    $lastcmd = 'cut';
	}
	elsif ($cmd eq 'head1') {	# heading type 1
	    $rest =~ s/^\s*//; $rest =~ s/\s*$//;
	    print LATEX  "\n\\subsection*{$rest}";
	    # put index entry
	    ($index = $rest) =~ s/^(An?\s+|The\s+)//i;	# remove 'A' and 'The'
	    # index only those heads not matching the frequent patterns.
	    foreach $pat (@head1_freq_patterns) {
		if ($index =~ /^$pat/) {
		    goto freqpatt;
		}
	    }
	    print LATEX  "%\n\\index{$index}\n" if ($index);
	  freqpatt:
	    $lastcmd = 'head1';
	}
	elsif ($cmd eq 'head2') {	# heading type 2
	    $rest =~ s/^\s*//; $rest =~ s/\s*$//;
	    print LATEX  "\n\\subsubsection*{$rest}";
	    # put index entry
	    ($index = $rest) =~ s/^(An?\s+|The\s+)//i;	# remove 'A' and 'The'
	    $index =~ s/^Example\s*[1-9][0-9]*\s*:\s*//; # remove 'Example :'
	    print LATEX  "%\n\\index{$index}\n"  if ($index);
	    $lastcmd = 'head2';
	}
	elsif ($cmd eq 'over') {	# 1 level within a listing environment
	    push(@indent,$indent);
	    $indent = $rest + 0;
	    $lastcmd = 'over';
	}
	elsif ($cmd eq 'back') {	# 1 level out of a listing environment
	    $indent = pop(@indent);
	    warn "Unmatched =back\n" unless defined $indent;
	    $listingcmd = pop(@listingcmd);
	    print LATEX  "\n\\end{$listingcmd}\n"  if ($listingcmd);
	    $lastcmd = 'back';
	}
	elsif ($cmd eq 'item') {	# an item paragraph starts
	    if ($lastcmd eq 'over') {	# if we have just entered listing env
		# see what type of list environment we are in.
		if ($rest =~ /^[0-9]\.?/) {	# if numeral heading
		    $listingcmd = 'enumerate';
		} elsif ($rest =~ /^\*\s*/) {	# if * heading
		    $listingcmd = 'itemize';
		} elsif ($rest =~ /^[^*]/) {	# if other headings
		    $listingcmd = 'description';
		} else {
		    warn "unknown list type for item $rest";
		}
		print LATEX  "\n\\begin{$listingcmd}\n";
		push(@listingcmd,$listingcmd);
	    } elsif ($lastcmd ne 'item') {
		warn "Illegal '=item' command without preceding 'over':";
		warn "=item $bareitem";
	    }

	    if ($listingcmd eq 'enumerate') {
		$rest =~ s/^[0-9]+\.?\s*//;	# remove numeral heading
		print LATEX  "\n\\item";
		print LATEX  "{\\bf $rest}" if $rest;
	    } elsif ($listingcmd eq 'itemize') {
		$rest =~ s/^\*\s*//;		# remove * heading
		print LATEX  "\n\\item";
		print LATEX  "{\\bf $rest}" if $rest;
	    } else {				# description item
		print LATEX  "\n\\item[$rest]";
	    }
	    $lastcmd = 'item';
	    $rightafter_item = 'yes';

	    # check if the item heading is short or long.
	    ($itemhead = $rest) =~ s/{\\bf (\S*)}/$1/g;
	    if (length($itemhead) < 4) {
		$itemshort = "yes";
	    } else {
		$itemshort = "no";
	    }
	    # write index entry
	    if ($pod =~ "perldiag") {			# skip 'perldiag.pod'
		goto noindex;
	    }
	    # strip out the item of pod quotes and get a plain text entry
	    $bareitem =~ s/\n/ /g;			# remove newlines
	    $bareitem =~ s/\s*$//;			# remove trailing space
	    $bareitem =~ s/[A-Z]<([^<>]*)>/$1/g;	# remove <> quotes
	    ($index = $bareitem) =~ s/^\*\s+//;		# remove leading '*'
	    $index =~ s/^(An?\s+|The\s+)//i;		# remove 'A' and 'The'
	    $index =~ s/^\s*[1-9][0-9]*\s*[.]\s*$//; # remove numeral only
	    $index =~ s/^\s*\w\s*$//;			# remove 1 char only's
		# quote ", @ and ! with " to be used in makeindex.
	    $index =~ s/"/""/g;				# quote "
	    $index =~ s/@/"@/g;				# quote @
	    $index =~ s/!/"!/g;				# quote !
	    ($rest2=$rest) =~ s/^\*\s+//;	# remove *
	    $rest2 =~ s/"/""/g;				# quote "
	    $rest2 =~ s/@/"@/g;				# quote @
	    $rest2 =~ s/!/"!/g;				# quote !
	    if ($pod =~ "(perlfunc|perlvar)") {	# when doc is perlfunc,perlvar
		# take only the 1st word of item heading
		$index =~ s/^([^{}\s]*)({.*})?([^{}\s]*)\s+.*/\1\2\3/;
		$rest2 =~ s/^([^{}\s]*)({.*})?([^{}\s]*)\s+.*/\1\2\3/;
	    }
	    if ($index =~ /[A-Za-z\$@%]/) {
		    #  write  \index{plain_text_entry@TeX_string_entry}
		print LATEX  "%\n\\index{$index\@$rest2}%\n";
	    }
	  noindex:
	    ;
	}
	elsif ($cmd eq 'pod') {
	    ;	# recognise the pod directive, as no op (hs)
	}
 	elsif ($cmd eq 'pod') {
 	    ;    # recognise the pod directive, as no op (hs)
 	}
	else {
	    warn "Unrecognized directive: $cmd\n";
	}
    }
    else {					# if not command
	&escapes;
	$_ = &clear_noremap($_);
	$_ = &expand_HTML_escapes($_);

	# if the present paragraphs follows an =item declaration,
	# put a line break.
	if ($lastcmd eq 'item' &&
	    $rightafter_item eq 'yes' && $itemshort eq "no") {
	    print LATEX  "\\hfil\\\\";
	    $rightafter_item = 'no';
	}
	print LATEX  "\n",$_;
    }
}

print LATEX  "\n";
close(POD);
close(LATEX);


#########################################################################

sub do_hdr {
    print LATEX "% LaTeX document produced by pod2latex from \"$pod.pod\".\n";
    print LATEX "% The followings need be defined in the preamble of this document:\n";
    print LATEX "%\\def\\C++{{\\rm C\\kern-.05em\\raise.3ex\\hbox{\\footnotesize ++}}}\n";
    print LATEX "%\\def\\underscore{\\leavevmode\\kern.04em\\vbox{\\hrule width 0.4em height 0.3pt}}\n";
    print LATEX "%\\setlength{\\parindent}{0pt}\n";
    print LATEX "\n";
    $podq = &escape_tex_specials("\U$pod\E");
    print LATEX "\\section{$podq}%\n";
    print LATEX "\\index{$podq}";
    print LATEX "\n";
}

sub nobreak {
    my $string = shift;
    $string =~ s/ +/~/g;		# TeX no line break
    $string;
}

sub noremap {
    local($thing_to_hide) = shift;
    $thing_to_hide =~ tr/\000-\177/\200-\377/;
    return $thing_to_hide;
}

sub init_noremap {
	# escape high bit characters in input stream
	s/([\200-\377])/"E<".ord($1).">"/ge;
}

sub clear_noremap {
    local($tmp) = shift;
    $tmp =~ tr/\200-\377/\000-\177/;
    return $tmp;
}

sub expand_HTML_escapes {
    local($s) = $_[0];
    $s =~ s { E<((\d+)|([A-Za-z]+))> }
    {
	do {
		defined($2) 
		? do { chr($2) }
		: 
	    exists $HTML_Escapes{$3}
	    ? do { $HTML_Escapes{$3} }
	    : do {
		warn "Unknown escape: $& in $_";
		"E<$1>";
	    }
	}
    }egx;
    return $s;
}

sub escapes {
    # make C++ into \C++, which is to be defined as
    # \def\C++{{\rm C\kern-.05em\raise.3ex\hbox{\footnotesize ++}}}
    s/\bC\+\+/\\C++{}/g;
}

# Translate a string into a TeX \tt string to obtain a verbatim print out.
# TeX special characters are escaped by \.
# This can be used inside of LaTeX command arguments.
# We don't use LaTeX \verb since it doesn't work inside of command arguments.
sub alltt {
    local($str) = shift;
	# other chars than #,\,$,%,&,{,},_,\,^,~ ([ and ] included).
    $str =~ s/([^${backslash_escapables}\\\^\~]+)/&noremap("$&")/eg;
	# chars #,\,$,%,&,{,}  =>  \# , ...
    $str =~ s/([$backslash_escapables2])/&noremap("\\$&")/eg;
	# chars _,\,^,~  =>  \char`\_ , ...
    $str =~ s/_/&noremap("\\char`\\_")/eg;
    $str =~ s/\\/&noremap("\\char`\\\\")/ge;
    $str =~ s/\^/\\char`\\^/g;
    $str =~ s/\~/\\char`\\~/g;

    $str =~ tr/\200-\377/\000-\177/;		# put back
    $str = "{\\tt ".$str."}";			# make it a \tt string
    return $str;
}

sub escape_tex_specials {
    local($str) = shift;
	# other chars than #,\,$,%,&,{,},  _,\,^,~ ([ and ] included).
    # backslash escapable characters #,\,$,%,&,{,} except _.
    $str =~ s/([$backslash_escapables2])/&noremap("\\$1")/ge;
    $str =~ s/_/&noremap("\\underscore{}")/ge;	# \_ is too thin.
    # quote TeX special characters |, ^, ~, \.
    $str =~ s/\|/&noremap("\$|\$")/ge;
    $str =~ s/\^/&noremap("\$\\hat{\\hspace{0.4em}}\$")/ge;
    $str =~ s/\~/&noremap("\$\\tilde{\\hspace{0.4em}}\$")/ge;
    $str =~ s/\\/&noremap("\$\\backslash{}\$")/ge;
    # characters need to be treated differently in TeX
    # *
    $str =~ s/[*]/&noremap("\$\\ast\$")/ge;
    # escape < and > as math string,
    $str =~ s/</&noremap("\$<\$")/ge;
    $str =~ s/>/&noremap("\$>\$")/ge;
    $str =~ tr/\200-\377/\000-\177/;		# put back
    return $str;
}

sub internal_lrefs {
    local($_) = shift;

    s{L</([^>]+)>}{$1}g;
    my(@items) = split( /(?:,?\s+(?:and\s+)?)/ );
    my $retstr = "the ";
    my $i;
    for ($i = 0; $i <= $#items; $i++) {
	$retstr .= "C<$items[$i]>";
	$retstr .= ", " if @items > 2 && $i != $#items;
	$retstr .= " and " if $i+2 == @items;
    }
    $retstr .= " entr" . ( @items > 1  ? "ies" : "y" )
	    .  " elsewhere in this document";

    return $retstr;
}

# map of HTML escapes to TeX escapes.
BEGIN {
%HTML_Escapes = (
    'amp'	=>	'&',	#   ampersand
    'lt'	=>	'<',	#   left chevron, less-than
    'gt'	=>	'>',	#   right chevron, greater-than
    'quot'	=>	'"',	#   double quote

    "Aacute"	=>	"\\'{A}",	#   capital A, acute accent
    "aacute"	=>	"\\'{a}",	#   small a, acute accent
    "Acirc"	=>	"\\^{A}",	#   capital A, circumflex accent
    "acirc"	=>	"\\^{a}",	#   small a, circumflex accent
    "AElig"	=>	'\\AE',		#   capital AE diphthong (ligature)
    "aelig"	=>	'\\ae',		#   small ae diphthong (ligature)
    "Agrave"	=>	"\\`{A}",	#   capital A, grave accent
    "agrave"	=>	"\\`{a}",	#   small a, grave accent
    "Aring"	=>	'\\u{A}',	#   capital A, ring
    "aring"	=>	'\\u{a}',	#   small a, ring
    "Atilde"	=>	'\\~{A}',	#   capital A, tilde
    "atilde"	=>	'\\~{a}',	#   small a, tilde
    "Auml"	=>	'\\"{A}',	#   capital A, dieresis or umlaut mark
    "auml"	=>	'\\"{a}',	#   small a, dieresis or umlaut mark
    "Ccedil"	=>	'\\c{C}',	#   capital C, cedilla
    "ccedil"	=>	'\\c{c}',	#   small c, cedilla
    "Eacute"	=>	"\\'{E}",	#   capital E, acute accent
    "eacute"	=>	"\\'{e}",	#   small e, acute accent
    "Ecirc"	=>	"\\^{E}",	#   capital E, circumflex accent
    "ecirc"	=>	"\\^{e}",	#   small e, circumflex accent
    "Egrave"	=>	"\\`{E}",	#   capital E, grave accent
    "egrave"	=>	"\\`{e}",	#   small e, grave accent
    "ETH"	=>	'\\OE',		#   capital Eth, Icelandic
    "eth"	=>	'\\oe',		#   small eth, Icelandic
    "Euml"	=>	'\\"{E}',	#   capital E, dieresis or umlaut mark
    "euml"	=>	'\\"{e}',	#   small e, dieresis or umlaut mark
    "Iacute"	=>	"\\'{I}",	#   capital I, acute accent
    "iacute"	=>	"\\'{i}",	#   small i, acute accent
    "Icirc"	=>	"\\^{I}",	#   capital I, circumflex accent
    "icirc"	=>	"\\^{i}",	#   small i, circumflex accent
    "Igrave"	=>	"\\`{I}",	#   capital I, grave accent
    "igrave"	=>	"\\`{i}",	#   small i, grave accent
    "Iuml"	=>	'\\"{I}',	#   capital I, dieresis or umlaut mark
    "iuml"	=>	'\\"{i}',	#   small i, dieresis or umlaut mark
    "Ntilde"	=>	'\\~{N}',	#   capital N, tilde
    "ntilde"	=>	'\\~{n}',	#   small n, tilde
    "Oacute"	=>	"\\'{O}",	#   capital O, acute accent
    "oacute"	=>	"\\'{o}",	#   small o, acute accent
    "Ocirc"	=>	"\\^{O}",	#   capital O, circumflex accent
    "ocirc"	=>	"\\^{o}",	#   small o, circumflex accent
    "Ograve"	=>	"\\`{O}",	#   capital O, grave accent
    "ograve"	=>	"\\`{o}",	#   small o, grave accent
    "Oslash"	=>	"\\O",		#   capital O, slash
    "oslash"	=>	"\\o",		#   small o, slash
    "Otilde"	=>	"\\~{O}",	#   capital O, tilde
    "otilde"	=>	"\\~{o}",	#   small o, tilde
    "Ouml"	=>	'\\"{O}',	#   capital O, dieresis or umlaut mark
    "ouml"	=>	'\\"{o}',	#   small o, dieresis or umlaut mark
    "szlig"	=>	'\\ss{}',	#   small sharp s, German (sz ligature)
    "THORN"	=>	'\\L',		#   capital THORN, Icelandic
    "thorn"	=>	'\\l',,		#   small thorn, Icelandic
    "Uacute"	=>	"\\'{U}",	#   capital U, acute accent
    "uacute"	=>	"\\'{u}",	#   small u, acute accent
    "Ucirc"	=>	"\\^{U}",	#   capital U, circumflex accent
    "ucirc"	=>	"\\^{u}",	#   small u, circumflex accent
    "Ugrave"	=>	"\\`{U}",	#   capital U, grave accent
    "ugrave"	=>	"\\`{u}",	#   small u, grave accent
    "Uuml"	=>	'\\"{U}',	#   capital U, dieresis or umlaut mark
    "uuml"	=>	'\\"{u}',	#   small u, dieresis or umlaut mark
    "Yacute"	=>	"\\'{Y}",	#   capital Y, acute accent
    "yacute"	=>	"\\'{y}",	#   small y, acute accent
    "yuml"	=>	'\\"{y}',	#   small y, dieresis or umlaut mark
);
}
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
