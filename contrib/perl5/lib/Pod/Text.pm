package Pod::Text;

=head1 NAME

Pod::Text - convert POD data to formatted ASCII text

=head1 SYNOPSIS

	use Pod::Text;

	pod2text("perlfunc.pod");

Also:

	pod2text [B<-a>] [B<->I<width>] < input.pod

=head1 DESCRIPTION

Pod::Text is a module that can convert documentation in the POD format (such
as can be found throughout the Perl distribution) into formatted ASCII.
Termcap is optionally supported for boldface/underline, and can enabled via
C<$Pod::Text::termcap=1>. If termcap has not been enabled, then backspaces
will be used to simulate bold and underlined text.

A separate F<pod2text> program is included that is primarily a wrapper for
Pod::Text.

The single function C<pod2text()> can take the optional options B<-a>
for an alternative output format, then a B<->I<width> option with the
max terminal width, followed by one or two arguments. The first
should be the name of a file to read the pod from, or "E<lt>&STDIN" to read from
STDIN. A second argument, if provided, should be a filehandle glob where
output should be sent.

=head1 AUTHOR

Tom Christiansen E<lt>F<tchrist@mox.perl.com>E<gt>

=head1 TODO

Cleanup work. The input and output locations need to be more flexible,
termcap shouldn't be a global variable, and the terminal speed needs to
be properly calculated.

=cut

use Term::Cap;
require Exporter;
@ISA = Exporter;
@EXPORT = qw(pod2text);

use vars qw($VERSION);
$VERSION = "1.0203";

use locale;	# make \w work right in non-ASCII lands

$termcap=0;

$opt_alt_format = 0;

#$use_format=1;

$UNDL = "\x1b[4m";
$INV = "\x1b[7m";
$BOLD = "\x1b[1m";
$NORM = "\x1b[0m";

sub pod2text {
shift if $opt_alt_format = ($_[0] eq '-a');

if($termcap and !$setuptermcap) {
	$setuptermcap=1;

    my($term) = Tgetent Term::Cap { TERM => undef, OSPEED => 9600 };
    $UNDL = $term->{'_us'};
    $INV = $term->{'_mr'};
    $BOLD = $term->{'_md'};
    $NORM = $term->{'_me'};
}

$SCREEN = ($_[0] =~ /^-(\d+)/ && (shift, $1))
       ||  $ENV{COLUMNS}
       || ($ENV{TERMCAP} =~ /co#(\d+)/)[0]
       || ($^O ne 'MSWin32' && $^O ne 'dos' && (`stty -a 2>/dev/null` =~ /(\d+) columns/)[0])
       || 72;

@_ = ("<&STDIN") unless @_;
local($file,*OUTPUT) = @_;
*OUTPUT = *STDOUT if @_<2;

local $: = $:;
$: = " \n" if $opt_alt_format;	# Do not break ``-L/lib/'' into ``- L/lib/''.

$/ = "";

$FANCY = 0;

$cutting = 1;
$DEF_INDENT = 4;
$indent = $DEF_INDENT;
$needspace = 0;
$begun = "";

open(IN, $file) || die "Couldn't open $file: $!";

POD_DIRECTIVE: while (<IN>) {
    if ($cutting) {
	next unless /^=/;
	$cutting = 0;
    }
    if ($begun) {
        if (/^=end\s+$begun/) {
             $begun = "";
        }
        elsif ($begun eq "text") {
            print OUTPUT $_;
        }
        next;
    }
    1 while s{^(.*?)(\t+)(.*)$}{
	$1
	. (' ' x (length($2) * 8 - length($1) % 8))
	. $3
    }me;
    # Translate verbatim paragraph
    if (/^\s/) {
	output($_);
	next;
    }

    if (/^=for\s+(\S+)\s*(.*)/s) {
        if ($1 eq "text") {
            print OUTPUT $2,"";
        } else {
            # ignore unknown for
        }
        next;
    }
    elsif (/^=begin\s+(\S+)\s*(.*)/s) {
        $begun = $1;
        if ($1 eq "text") {
            print OUTPUT $2."";
        }
        next;
    }

sub prepare_for_output {

    s/\s*$/\n/;
    &init_noremap;

    # need to hide E<> first; they're processed in clear_noremap
    s/(E<[^<>]+>)/noremap($1)/ge;
    $maxnest = 10;
    while ($maxnest-- && /[A-Z]</) {
	unless ($FANCY) {
	    if ($opt_alt_format) {
		s/[BC]<(.*?)>/``$1''/sg;
		s/F<(.*?)>/"$1"/sg;
	    } else {
		s/C<(.*?)>/`$1'/sg;
	    }
	} else {
	    s/C<(.*?)>/noremap("E<lchevron>${1}E<rchevron>")/sge;
	}
        # s/[IF]<(.*?)>/italic($1)/ge;
        s/I<(.*?)>/*$1*/sg;
        # s/[CB]<(.*?)>/bold($1)/ge;
	s/X<.*?>//sg;

	# LREF: a la HREF L<show this text|man/section>
	s:L<([^|>]+)\|[^>]+>:$1:g;

	# LREF: a manpage(3f)
	s:L<([a-zA-Z][^\s\/]+)(\([^\)]+\))?>:the $1$2 manpage:g;
	# LREF: an =item on another manpage
	s{
	    L<
		([^/]+)
		/
		(
		    [:\w]+
		    (\(\))?
		)
	    >
	} {the "$2" entry in the $1 manpage}gx;

	# LREF: an =item on this manpage
	s{
	   ((?:
	    L<
		/
		(
		    [:\w]+
		    (\(\))?
		)
	    >
	    (,?\s+(and\s+)?)?
	  )+)
	} { internal_lrefs($1) }gex;

	# LREF: a =head2 (head1?), maybe on a manpage, maybe right here
	# the "func" can disambiguate
	s{
	    L<
		(?:
		    ([a-zA-Z]\S+?) / 
		)?
		"?(.*?)"?
	    >
	}{
	    do {
		$1 	# if no $1, assume it means on this page.
		    ?  "the section on \"$2\" in the $1 manpage"
		    :  "the section on \"$2\""
	    }
	}sgex;

        s/[A-Z]<(.*?)>/$1/sg;
    }
    clear_noremap(1);
}

    &prepare_for_output;

    if (s/^=//) {
	# $needspace = 0;		# Assume this.
	# s/\n/ /g;
	($Cmd, $_) = split(' ', $_, 2);
	# clear_noremap(1);
	if ($Cmd eq 'cut') {
	    $cutting = 1;
	}
	elsif ($Cmd eq 'pod') {
	    $cutting = 0;
	}
	elsif ($Cmd eq 'head1') {
	    makespace();
	    if ($opt_alt_format) {
		print OUTPUT "\n";
		s/^(.+?)[ \t]*$/==== $1 ====/;
	    }
	    print OUTPUT;
	    # print OUTPUT uc($_);
	    $needspace = $opt_alt_format;
	}
	elsif ($Cmd eq 'head2') {
	    makespace();
	    # s/(\w+)/\u\L$1/g;
	    #print ' ' x $DEF_INDENT, $_;
	    # print "\xA7";
	    s/(\w)/\xA7 $1/ if $FANCY;
	    if ($opt_alt_format) {
		s/^(.+?)[ \t]*$/==   $1   ==/;
		print OUTPUT "\n", $_;
	    } else {
		print OUTPUT ' ' x ($DEF_INDENT/2), $_, "\n";
	    }
	    $needspace = $opt_alt_format;
	}
	elsif ($Cmd eq 'over') {
	    push(@indent,$indent);
	    $indent += ($_ + 0) || $DEF_INDENT;
	}
	elsif ($Cmd eq 'back') {
	    $indent = pop(@indent);
	    warn "Unmatched =back\n" unless defined $indent;
	}
	elsif ($Cmd eq 'item') {
	    makespace();
	    # s/\A(\s*)\*/$1\xb7/ if $FANCY;
	    # s/^(\s*\*\s+)/$1 /;
	    {
		if (length() + 3 < $indent) {
		    my $paratag = $_;
		    $_ = <IN>;
		    if (/^=/) {  # tricked!
			local($indent) = $indent[$#indent - 1] || $DEF_INDENT;
			output($paratag);
			redo POD_DIRECTIVE;
		    }
		    &prepare_for_output;
		    IP_output($paratag, $_);
		} else {
		    local($indent) = $indent[$#indent - 1] || $DEF_INDENT;
		    output($_, 0);
		}
	    }
	}
	else {
	    warn "Unrecognized directive: $Cmd\n";
	}
    }
    else {
	# clear_noremap(1);
	makespace();
	output($_, 1);
    }
}

close(IN);

}

#########################################################################

sub makespace {
    if ($needspace) {
	print OUTPUT "\n";
	$needspace = 0;
    }
}

sub bold {
    my $line = shift;
    return $line if $use_format;
    if($termcap) {
    	$line = "$BOLD$line$NORM";
    } else {
	    $line =~ s/(.)/$1\b$1/g;
	}
#    $line = "$BOLD$line$NORM" if $ansify;
    return $line;
}

sub italic {
    my $line = shift;
    return $line if $use_format;
    if($termcap) {
    	$line = "$UNDL$line$NORM";
    } else {
	    $line =~ s/(.)/$1\b_/g;
    }
#    $line = "$UNDL$line$NORM" if $ansify;
    return $line;
}

# Fill a paragraph including underlined and overstricken chars.
# It's not perfect for words longer than the margin, and it's probably
# slow, but it works.
sub fill {
    local $_ = shift;
    my $par = "";
    my $indent_space = " " x $indent;
    my $marg = $SCREEN-$indent;
    my $line = $indent_space;
    my $line_length;
    foreach (split) {
	my $word_length = length;
	$word_length -= 2 while /\010/g;  # Subtract backspaces

	if ($line_length + $word_length > $marg) {
	    $par .= $line . "\n";
	    $line= $indent_space . $_;
	    $line_length = $word_length;
	}
	else {
	    if ($line_length) {
		$line_length++;
		$line .= " ";
	    }
	    $line_length += $word_length;
	    $line .= $_;
	}
    }
    $par .= "$line\n" if $line;
    $par .= "\n";
    return $par;
}

sub IP_output {
    local($tag, $_) = @_;
    local($tag_indent) = $indent[$#indent - 1] || $DEF_INDENT;
    $tag_cols = $SCREEN - $tag_indent;
    $cols = $SCREEN - $indent;
    $tag =~ s/\s*$//;
    s/\s+/ /g;
    s/^ //;
    $str = "format OUTPUT = \n"
	. (($opt_alt_format && $tag_indent > 1)
	   ? ":" . " " x ($tag_indent - 1)
	   : " " x ($tag_indent))
	. '@' . ('<' x ($indent - $tag_indent - 1))
	. "^" .  ("<" x ($cols - 1)) . "\n"
	. '$tag, $_'
	. "\n~~"
	. (" " x ($indent-2))
	. "^" .  ("<" x ($cols - 5)) . "\n"
	. '$_' . "\n\n.\n1";
    #warn $str; warn "tag is $tag, _ is $_";
    eval $str || die;
    write OUTPUT;
}

sub output {
    local($_, $reformat) = @_;
    if ($reformat) {
	$cols = $SCREEN - $indent;
	s/\s+/ /g;
	s/^ //;
	$str = "format OUTPUT = \n~~"
	    . (" " x ($indent-2))
	    . "^" .  ("<" x ($cols - 5)) . "\n"
	    . '$_' . "\n\n.\n1";
	eval $str || die;
	write OUTPUT;
    } else {
	s/^/' ' x $indent/gem;
	s/^\s+\n$/\n/gm;
	s/^  /: /s if defined($reformat) && $opt_alt_format;
	print OUTPUT;
    }
}

sub noremap {
    local($thing_to_hide) = shift;
    $thing_to_hide =~ tr/\000-\177/\200-\377/;
    return $thing_to_hide;
}

sub init_noremap {
    die "unmatched init" if $mapready++;
    #mask off high bit characters in input stream
    s/([\200-\377])/"E<".ord($1).">"/ge;
}

sub clear_noremap {
    my $ready_to_print = $_[0];
    die "unmatched clear" unless $mapready--;
    tr/\200-\377/\000-\177/;
    # now for the E<>s, which have been hidden until now
    # otherwise the interative \w<> processing would have
    # been hosed by the E<gt>
    s {
	    E<
	    (
	    	( \d+ )
	    	| ( [A-Za-z]+ )
	    )
	    >	
    } {
	 do {
	 	defined $2
	 	? chr($2)
	 	:
	     defined $HTML_Escapes{$3}
		? do { $HTML_Escapes{$3} }
		: do {
		    warn "Unknown escape: E<$1> in $_";
		    "E<$1>";
		}
	 }
    }egx if $ready_to_print;
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
	    .  " elsewhere in this document ";

    return $retstr;

}

BEGIN {

%HTML_Escapes = (
    'amp'	=>	'&',	#   ampersand
    'lt'	=>	'<',	#   left chevron, less-than
    'gt'	=>	'>',	#   right chevron, greater-than
    'quot'	=>	'"',	#   double quote

    "Aacute"	=>	"\xC1",	#   capital A, acute accent
    "aacute"	=>	"\xE1",	#   small a, acute accent
    "Acirc"	=>	"\xC2",	#   capital A, circumflex accent
    "acirc"	=>	"\xE2",	#   small a, circumflex accent
    "AElig"	=>	"\xC6",	#   capital AE diphthong (ligature)
    "aelig"	=>	"\xE6",	#   small ae diphthong (ligature)
    "Agrave"	=>	"\xC0",	#   capital A, grave accent
    "agrave"	=>	"\xE0",	#   small a, grave accent
    "Aring"	=>	"\xC5",	#   capital A, ring
    "aring"	=>	"\xE5",	#   small a, ring
    "Atilde"	=>	"\xC3",	#   capital A, tilde
    "atilde"	=>	"\xE3",	#   small a, tilde
    "Auml"	=>	"\xC4",	#   capital A, dieresis or umlaut mark
    "auml"	=>	"\xE4",	#   small a, dieresis or umlaut mark
    "Ccedil"	=>	"\xC7",	#   capital C, cedilla
    "ccedil"	=>	"\xE7",	#   small c, cedilla
    "Eacute"	=>	"\xC9",	#   capital E, acute accent
    "eacute"	=>	"\xE9",	#   small e, acute accent
    "Ecirc"	=>	"\xCA",	#   capital E, circumflex accent
    "ecirc"	=>	"\xEA",	#   small e, circumflex accent
    "Egrave"	=>	"\xC8",	#   capital E, grave accent
    "egrave"	=>	"\xE8",	#   small e, grave accent
    "ETH"	=>	"\xD0",	#   capital Eth, Icelandic
    "eth"	=>	"\xF0",	#   small eth, Icelandic
    "Euml"	=>	"\xCB",	#   capital E, dieresis or umlaut mark
    "euml"	=>	"\xEB",	#   small e, dieresis or umlaut mark
    "Iacute"	=>	"\xCD",	#   capital I, acute accent
    "iacute"	=>	"\xED",	#   small i, acute accent
    "Icirc"	=>	"\xCE",	#   capital I, circumflex accent
    "icirc"	=>	"\xEE",	#   small i, circumflex accent
    "Igrave"	=>	"\xCD",	#   capital I, grave accent
    "igrave"	=>	"\xED",	#   small i, grave accent
    "Iuml"	=>	"\xCF",	#   capital I, dieresis or umlaut mark
    "iuml"	=>	"\xEF",	#   small i, dieresis or umlaut mark
    "Ntilde"	=>	"\xD1",		#   capital N, tilde
    "ntilde"	=>	"\xF1",		#   small n, tilde
    "Oacute"	=>	"\xD3",	#   capital O, acute accent
    "oacute"	=>	"\xF3",	#   small o, acute accent
    "Ocirc"	=>	"\xD4",	#   capital O, circumflex accent
    "ocirc"	=>	"\xF4",	#   small o, circumflex accent
    "Ograve"	=>	"\xD2",	#   capital O, grave accent
    "ograve"	=>	"\xF2",	#   small o, grave accent
    "Oslash"	=>	"\xD8",	#   capital O, slash
    "oslash"	=>	"\xF8",	#   small o, slash
    "Otilde"	=>	"\xD5",	#   capital O, tilde
    "otilde"	=>	"\xF5",	#   small o, tilde
    "Ouml"	=>	"\xD6",	#   capital O, dieresis or umlaut mark
    "ouml"	=>	"\xF6",	#   small o, dieresis or umlaut mark
    "szlig"	=>	"\xDF",		#   small sharp s, German (sz ligature)
    "THORN"	=>	"\xDE",	#   capital THORN, Icelandic
    "thorn"	=>	"\xFE",	#   small thorn, Icelandic
    "Uacute"	=>	"\xDA",	#   capital U, acute accent
    "uacute"	=>	"\xFA",	#   small u, acute accent
    "Ucirc"	=>	"\xDB",	#   capital U, circumflex accent
    "ucirc"	=>	"\xFB",	#   small u, circumflex accent
    "Ugrave"	=>	"\xD9",	#   capital U, grave accent
    "ugrave"	=>	"\xF9",	#   small u, grave accent
    "Uuml"	=>	"\xDC",	#   capital U, dieresis or umlaut mark
    "uuml"	=>	"\xFC",	#   small u, dieresis or umlaut mark
    "Yacute"	=>	"\xDD",	#   capital Y, acute accent
    "yacute"	=>	"\xFD",	#   small y, acute accent
    "yuml"	=>	"\xFF",	#   small y, dieresis or umlaut mark

    "lchevron"	=>	"\xAB",	#   left chevron (double less than)
    "rchevron"	=>	"\xBB",	#   right chevron (double greater than)
);
}

1;
