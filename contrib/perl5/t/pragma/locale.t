#!./perl -wT

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    unshift @INC, '.';
    require Config; import Config;
    if (!$Config{d_setlocale} || $Config{ccflags} =~ /\bD?NO_LOCALE\b/) {
	print "1..0\n";
	exit;
    }
}

use strict;

my $debug = 1;

use Dumpvalue;

my $dumper = Dumpvalue->new(
                            tick => qq{"},
                            quoteHighBit => 0,
                            unctrl => "quote"
                           );
sub debug {
  return unless $debug;
  my($mess) = join "", @_;
  chop $mess;
  print $dumper->stringify($mess,1), "\n";
}

sub debugf {
    printf @_ if $debug;
}

my $have_setlocale = 0;
eval {
    require POSIX;
    import POSIX ':locale_h';
    $have_setlocale++;
};

# Visual C's CRT goes silly on strings of the form "en_US.ISO8859-1"
# and mingw32 uses said silly CRT
$have_setlocale = 0 if $^O eq 'MSWin32' && $Config{cc} =~ /^(cl|gcc)/i;

my $last = $have_setlocale ? 116 : 98;

print "1..$last\n";

use vars qw(&LC_ALL);

my $a = 'abc %';

sub ok {
    my ($n, $result) = @_;

    print 'not ' unless ($result);
    print "ok $n\n";
}

# First we'll do a lot of taint checking for locales.
# This is the easiest to test, actually, as any locale,
# even the default locale will taint under 'use locale'.

sub is_tainted { # hello, camel two.
    no warnings 'uninitialized' ;
    my $dummy;
    not eval { $dummy = join("", @_), kill 0; 1 }
}

sub check_taint ($$) {
    ok $_[0], is_tainted($_[1]);
}

sub check_taint_not ($$) {
    ok $_[0], not is_tainted($_[1]);
}

use locale;	# engage locale and therefore locale taint.

check_taint_not   1, $a;

check_taint       2, uc($a);
check_taint       3, "\U$a";
check_taint       4, ucfirst($a);
check_taint       5, "\u$a";
check_taint       6, lc($a);
check_taint       7, "\L$a";
check_taint       8, lcfirst($a);
check_taint       9, "\l$a";

check_taint_not  10, sprintf('%e', 123.456);
check_taint_not  11, sprintf('%f', 123.456);
check_taint_not  12, sprintf('%g', 123.456);
check_taint_not  13, sprintf('%d', 123.456);
check_taint_not  14, sprintf('%x', 123.456);

$_ = $a;	# untaint $_

$_ = uc($a);	# taint $_

check_taint      15, $_;

/(\w)/;	# taint $&, $`, $', $+, $1.
check_taint      16, $&;
check_taint      17, $`;
check_taint      18, $';
check_taint      19, $+;
check_taint      20, $1;
check_taint_not  21, $2;

/(.)/;	# untaint $&, $`, $', $+, $1.
check_taint_not  22, $&;
check_taint_not  23, $`;
check_taint_not  24, $';
check_taint_not  25, $+;
check_taint_not  26, $1;
check_taint_not  27, $2;

/(\W)/;	# taint $&, $`, $', $+, $1.
check_taint      28, $&;
check_taint      29, $`;
check_taint      30, $';
check_taint      31, $+;
check_taint      32, $1;
check_taint_not  33, $2;

/(\s)/;	# taint $&, $`, $', $+, $1.
check_taint      34, $&;
check_taint      35, $`;
check_taint      36, $';
check_taint      37, $+;
check_taint      38, $1;
check_taint_not  39, $2;

/(\S)/;	# taint $&, $`, $', $+, $1.
check_taint      40, $&;
check_taint      41, $`;
check_taint      42, $';
check_taint      43, $+;
check_taint      44, $1;
check_taint_not  45, $2;

$_ = $a;	# untaint $_

check_taint_not  46, $_;

/(b)/;		# this must not taint
check_taint_not  47, $&;
check_taint_not  48, $`;
check_taint_not  49, $';
check_taint_not  50, $+;
check_taint_not  51, $1;
check_taint_not  52, $2;

$_ = $a;	# untaint $_

check_taint_not  53, $_;

$b = uc($a);	# taint $b
s/(.+)/$b/;	# this must taint only the $_

check_taint      54, $_;
check_taint_not  55, $&;
check_taint_not  56, $`;
check_taint_not  57, $';
check_taint_not  58, $+;
check_taint_not  59, $1;
check_taint_not  60, $2;

$_ = $a;	# untaint $_

s/(.+)/b/;	# this must not taint
check_taint_not  61, $_;
check_taint_not  62, $&;
check_taint_not  63, $`;
check_taint_not  64, $';
check_taint_not  65, $+;
check_taint_not  66, $1;
check_taint_not  67, $2;

$b = $a;	# untaint $b

($b = $a) =~ s/\w/$&/;
check_taint      68, $b;	# $b should be tainted.
check_taint_not  69, $a;	# $a should be not.

$_ = $a;	# untaint $_

s/(\w)/\l$1/;	# this must taint
check_taint      70, $_;
check_taint      71, $&;
check_taint      72, $`;
check_taint      73, $';
check_taint      74, $+;
check_taint      75, $1;
check_taint_not  76, $2;

$_ = $a;	# untaint $_

s/(\w)/\L$1/;	# this must taint
check_taint      77, $_;
check_taint      78, $&;
check_taint      79, $`;
check_taint      80, $';
check_taint      81, $+;
check_taint      82, $1;
check_taint_not  83, $2;

$_ = $a;	# untaint $_

s/(\w)/\u$1/;	# this must taint
check_taint      84, $_;
check_taint      85, $&;
check_taint      86, $`;
check_taint      87, $';
check_taint      88, $+;
check_taint      89, $1;
check_taint_not  90, $2;

$_ = $a;	# untaint $_

s/(\w)/\U$1/;	# this must taint
check_taint      91, $_;
check_taint      92, $&;
check_taint      93, $`;
check_taint      94, $';
check_taint      95, $+;
check_taint      96, $1;
check_taint_not  97, $2;

# After all this tainting $a should be cool.

check_taint_not  98, $a;

# I think we've seen quite enough of taint.
# Let us do some *real* locale work now,
# unless setlocale() is missing (i.e. minitest).

exit unless $have_setlocale;

# Find locales.

debug "# Scanning for locales...\n";

# Note that it's okay that some languages have their native names
# capitalized here even though that's not "right".  They are lowercased
# anyway later during the scanning process (and besides, some clueless
# vendor might have them capitalized errorneously anyway).

my $locales = <<EOF;
Afrikaans:af:za:1 15
Arabic:ar:dz eg sa:6 arabic8
Brezhoneg Breton:br:fr:1 15
Bulgarski Bulgarian:bg:bg:5
Chinese:zh:cn tw:cn.EUC eucCN eucTW euc.CN euc.TW Big5 GB2312 tw.EUC
Hrvatski Croatian:hr:hr:2
Cymraeg Welsh:cy:cy:1 14 15
Czech:cs:cz:2
Dansk Danish:dk:da:1 15
Nederlands Dutch:nl:be nl:1 15
English American British:en:au ca gb ie nz us uk zw:1 15 cp850
Esperanto:eo:eo:3
Eesti Estonian:et:ee:4 6 13
Suomi Finnish:fi:fi:1 15
Flamish::fl:1 15
Deutsch German:de:at be ch de lu:1 15
Euskaraz Basque:eu:es fr:1 15
Galego Galician:gl:es:1 15
Ellada Greek:el:gr:7 g8
Frysk:fy:nl:1 15
Greenlandic:kl:gl:4 6
Hebrew:iw:il:8 hebrew8
Hungarian:hu:hu:2
Indonesian:in:id:1 15
Gaeilge Irish:ga:IE:1 14 15
Italiano Italian:it:ch it:1 15
Nihongo Japanese:ja:jp:euc eucJP jp.EUC sjis
Korean:ko:kr:
Latine Latin:la:va:1 15
Latvian:lv:lv:4 6 13
Lithuanian:lt:lt:4 6 13
Macedonian:mk:mk:1 15
Maltese:mt:mt:3
Moldovan:mo:mo:2
Norsk Norwegian:no no\@nynorsk:no:1 15
Occitan:oc:es:1 15
Polski Polish:pl:pl:2
Rumanian:ro:ro:2
Russki Russian:ru:ru su ua:5 koi8 koi8r KOI8-R koi8u cp1251 cp866
Serbski Serbian:sr:yu:5
Slovak:sk:sk:2
Slovene Slovenian:sl:si:2
Sqhip Albanian:sq:sq:1 15
Svenska Swedish:sv:fi se:1 15
Thai:th:th:11 tis620
Turkish:tr:tr:9 turkish8
Yiddish:yi::1 15
EOF

if ($^O eq 'os390') {
    # These cause heartburn.  Broken locales?
    $locales =~ s/Svenska Swedish:sv:fi se:1 15\n//;
    $locales =~ s/Thai:th:th:11 tis620\n//;
}

sub in_utf8 () { $^H & 0x08 }

if (in_utf8) {
    require "pragma/locale/utf8";
} else {
    require "pragma/locale/latin1";
}

my @Locale;
my $Locale;
my @Alnum_;

sub getalnum_ {
    sort grep /\w/, map { chr } 0..255
}

sub trylocale {
    my $locale = shift;
    if (setlocale(LC_ALL, $locale)) {
	push @Locale, $locale;
    }
}

sub decode_encodings {
    my @enc;

    foreach (split(/ /, shift)) {
	if (/^(\d+)$/) {
	    push @enc, "ISO8859-$1";
	    push @enc, "iso8859$1";	# HP
	    if ($1 eq '1') {
		 push @enc, "roman8";	# HP
	    }
	} else {
	    push @enc, $_;
   	    push @enc, "$_.UTF-8";
	}
    }
    if ($^O eq 'os390') {
	push @enc, qw(IBM-037 IBM-819 IBM-1047);
    }

    return @enc;
}

trylocale("C");
trylocale("POSIX");
foreach (0..15) {
    trylocale("ISO8859-$_");
    trylocale("iso8859$_");
    trylocale("iso8859-$_");
    trylocale("iso_8859_$_");
    trylocale("isolatin$_");
    trylocale("isolatin-$_");
    trylocale("iso_latin_$_");
}

# Sanitize the environment so that we can run the external 'locale'
# program without the taint mode getting grumpy.

# $ENV{PATH} is special in VMS.
delete $ENV{PATH} if $^O ne 'VMS' or $Config{d_setenv};

# Other subversive stuff.
delete @ENV{qw(IFS CDPATH ENV BASH_ENV)};

if (-x "/usr/bin/locale" && open(LOCALES, "/usr/bin/locale -a 2>/dev/null|")) {
    while (<LOCALES>) {
        chomp;
	trylocale($_);
    }
    close(LOCALES);
} elsif ($^O eq 'VMS' && defined($ENV{'SYS$I18N_LOCALE'}) && -d 'SYS$I18N_LOCALE') {
# The SYS$I18N_LOCALE logical name search list was not present on 
# VAX VMS V5.5-12, but was on AXP && VAX VMS V6.2 as well as later versions.
    opendir(LOCALES, "SYS\$I18N_LOCALE:");
    while ($_ = readdir(LOCALES)) {
        chomp;
        trylocale($_);
    }
    close(LOCALES);
} else {

    # This is going to be slow.

    foreach my $locale (split(/\n/, $locales)) {
	my ($locale_name, $language_codes, $country_codes, $encodings) =
	    split(/:/, $locale);
	my @enc = decode_encodings($encodings);
	foreach my $loc (split(/ /, $locale_name)) {
	    trylocale($loc);
	    foreach my $enc (@enc) {
		trylocale("$loc.$enc");
	    }
	    $loc = lc $loc;
	    foreach my $enc (@enc) {
		trylocale("$loc.$enc");
	    }
	}
	foreach my $lang (split(/ /, $language_codes)) {
	    trylocale($lang);
	    foreach my $country (split(/ /, $country_codes)) {
		my $lc = "${lang}_${country}";
		trylocale($lc);
		foreach my $enc (@enc) {
		    trylocale("$lc.$enc");
		}
		my $lC = "${lang}_\U${country}";
		trylocale($lC);
		foreach my $enc (@enc) {
		    trylocale("$lC.$enc");
		}
	    }
	}
    }
}

setlocale(LC_ALL, "C");

sub utf8locale { $_[0] =~ /utf-?8/i }

@Locale = sort @Locale;

debug "# Locales = @Locale\n";

my %Problem;
my %Okay;
my %Testing;
my @Neoalpha;
my %Neoalpha;

sub tryneoalpha {
    my ($Locale, $i, $test) = @_;
    unless ($test) {
	$Problem{$i}{$Locale} = 1;
	debug "# failed $i with locale '$Locale'\n";
    } else {
	push @{$Okay{$i}}, $Locale;
    }
}

foreach $Locale (@Locale) {
    debug "# Locale = $Locale\n";
    @Alnum_ = getalnum_();
    debug "# w = ", join("",@Alnum_), "\n";

    unless (setlocale(LC_ALL, $Locale)) {
	foreach (99..103) {
	    $Problem{$_}{$Locale} = -1;
	}
	next;
    }

    # Sieve the uppercase and the lowercase.
    
    my %UPPER = ();
    my %lower = ();
    my %BoThCaSe = ();
    for (@Alnum_) {
	if (/[^\d_]/) { # skip digits and the _
	    if (uc($_) eq $_) {
		$UPPER{$_} = $_;
	    }
	    if (lc($_) eq $_) {
		$lower{$_} = $_;
	    }
	}
    }
    foreach (keys %UPPER) {
	$BoThCaSe{$_}++ if exists $lower{$_};
    }
    foreach (keys %lower) {
	$BoThCaSe{$_}++ if exists $UPPER{$_};
    }
    foreach (keys %BoThCaSe) {
	delete $UPPER{$_};
	delete $lower{$_};
    }

    debug "# UPPER    = ", join("", sort keys %UPPER   ), "\n";
    debug "# lower    = ", join("", sort keys %lower   ), "\n";
    debug "# BoThCaSe = ", join("", sort keys %BoThCaSe), "\n";

    # Find the alphabets that are not alphabets in the default locale.

    {
	no locale;
    
	@Neoalpha = ();
	for (keys %UPPER, keys %lower) {
	    push(@Neoalpha, $_) if (/\W/);
	    $Neoalpha{$_} = $_;
	}
    }

    @Neoalpha = sort @Neoalpha;

    debug "# Neoalpha = ", join("",@Neoalpha), "\n";

    if (@Neoalpha == 0) {
	# If we have no Neoalphas the remaining tests are no-ops.
	debug "# no Neoalpha, skipping tests 99..102 for locale '$Locale'\n";
	foreach (99..102) {
	    push @{$Okay{$_}}, $Locale;
	}
    } else {

	# Test \w.
    
	if (utf8locale($Locale)) {
	    # Until the polymorphic regexen arrive.
	    debug "# skipping UTF-8 locale '$Locale'\n";
	} else {
	    my $word = join('', @Neoalpha);

	    $word =~ /^(\w+)$/;

	    tryneoalpha($Locale, 99, $1 eq $word);
	}

	# Cross-check the whole 8-bit character set.

	for (map { chr } 0..255) {
	    tryneoalpha($Locale, 100,
			(/\w/ xor /\W/) ||
			(/\d/ xor /\D/) ||
			(/\s/ xor /\S/));
	}

	# Test for read-only scalars' locale vs non-locale comparisons.

	{
	    no locale;
	    $a = "qwerty";
	    {
		use locale;
		tryneoalpha($Locale, 101, ($a cmp "qwerty") == 0);
	    }
	}

	{
	    my ($from, $to, $lesser, $greater,
		@test, %test, $test, $yes, $no, $sign);

	    for (0..9) {
		# Select a slice.
		$from = int(($_*@Alnum_)/10);
		$to = $from + int(@Alnum_/10);
		$to = $#Alnum_ if ($to > $#Alnum_);
		$lesser  = join('', @Alnum_[$from..$to]);
		# Select a slice one character on.
		$from++; $to++;
		$to = $#Alnum_ if ($to > $#Alnum_);
		$greater = join('', @Alnum_[$from..$to]);
		($yes, $no, $sign) = ($lesser lt $greater
				      ? ("    ", "not ", 1)
				      : ("not ", "    ", -1));
		# all these tests should FAIL (return 0).
		# Exact lt or gt cannot be tested because
		# in some locales, say, eacute and E may test equal.
		@test = 
		    (
		     $no.'    ($lesser  le $greater)',  # 1
		     'not      ($lesser  ne $greater)', # 2
		     '         ($lesser  eq $greater)', # 3
		     $yes.'    ($lesser  ge $greater)', # 4
		     $yes.'    ($lesser  ge $greater)', # 5
		     $yes.'    ($greater le $lesser )', # 7
		     'not      ($greater ne $lesser )', # 8
		     '         ($greater eq $lesser )', # 9
		     $no.'     ($greater ge $lesser )', # 10
		     'not (($lesser cmp $greater) == -$sign)' # 12
		     );
		@test{@test} = 0 x @test;
		$test = 0;
		for my $ti (@test) {
		    $test{$ti} = eval $ti;
		    $test ||= $test{$ti}
		}
		tryneoalpha($Locale, 102, $test == 0);
		if ($test) {
		    debug "# lesser  = '$lesser'\n";
		    debug "# greater = '$greater'\n";
		    debug "# lesser cmp greater = ",
		          $lesser cmp $greater, "\n";
		    debug "# greater cmp lesser = ",
		          $greater cmp $lesser, "\n";
		    debug "# (greater) from = $from, to = $to\n";
		    for my $ti (@test) {
			debugf("# %-40s %-4s", $ti,
			       $test{$ti} ? 'FAIL' : 'ok');
			if ($ti =~ /\(\.*(\$.+ +cmp +\$[^\)]+)\.*\)/) {
			    debugf("(%s == %4d)", $1, eval $1);
			}
			debug "\n#";
		    }

		    last;
		}
	    }
	}
    }

    use locale;

    my ($x, $y) = (1.23, 1.23);

    my $a = "$x";
    printf ''; # printf used to reset locale to "C"
    my $b = "$y";

    debug "# 103..107: a = $a, b = $b, Locale = $Locale\n";

    tryneoalpha($Locale, 103, $a eq $b);

    my $c = "$x";
    my $z = sprintf ''; # sprintf used to reset locale to "C"
    my $d = "$y";

    debug "# 104..107: c = $c, d = $d, Locale = $Locale\n";

    tryneoalpha($Locale, 104, $c eq $d); 

    {
	use warnings;
	my $w = 0;
	local $SIG{__WARN__} = sub { $w++ };

	# the == (among other ops) used to warn for locales
	# that had something else than "." as the radix character

	tryneoalpha($Locale, 105, $c == 1.23);

	tryneoalpha($Locale, 106, $c == $x);

	tryneoalpha($Locale, 107, $c == $d);

	{
	    no locale;
	
	    my $e = "$x";

	    debug "# 108..110: e = $e, Locale = $Locale\n";

	    tryneoalpha($Locale, 108, $e == 1.23);

	    tryneoalpha($Locale, 109, $e == $x);
	    
	    tryneoalpha($Locale, 110, $e == $c);
	}
	
	tryneoalpha($Locale, 111, $w == 0);

	my $f = "1.23";

	debug "# 112..114: f = $f, locale = $Locale\n";

	tryneoalpha($Locale, 112, $f == 1.23);

	tryneoalpha($Locale, 113, $f == $x);
	
	tryneoalpha($Locale, 114, $f == $c);
    }

    # Does taking lc separately differ from taking
    # the lc "in-line"?  (This was the bug 19990704.002, change #3568.)
    # The bug was in the caching of the 'o'-magic.
    {
	use locale;

	sub lcA {
	    my $lc0 = lc $_[0];
	    my $lc1 = lc $_[1];
	    return $lc0 cmp $lc1;
	}

        sub lcB {
	    return lc($_[0]) cmp lc($_[1]);
	}

        my $x = "ab";
        my $y = "aa";
        my $z = "AB";

        tryneoalpha($Locale, 115,
		    lcA($x, $y) == 1 && lcB($x, $y) == 1 ||
		    lcA($x, $z) == 0 && lcB($x, $z) == 0);
    }

    # Does lc of an UPPER (if different from the UPPER) match
    # case-insensitively the UPPER, and does the UPPER match
    # case-insensitively the lc of the UPPER.  And vice versa.
    if (utf8locale($Locale)) {
        # Until the polymorphic regexen arrive.
        debug "# skipping UTF-8 locale '$Locale'\n";
    } else {
	use locale;

	my @f = ();
	foreach my $x (keys %UPPER) {
	    my $y = lc $x;
	    next unless uc $y eq $x;
	    push @f, $x unless $x =~ /$y/i && $y =~ /$x/i;
	}
	foreach my $x (keys %lower) {
	    my $y = uc $x;
	    next unless lc $y eq $x;
	    push @f, $x unless $x =~ /$y/i && $y =~ /$x/i;
	}
	tryneoalpha($Locale, 116, @f == 0);
        if (@f) {
	    print "# failed 116 locale '$Locale' characters @f\n"
        }
    }

}

# Recount the errors.

foreach (99..$last) {
    if ($Problem{$_} || !defined $Okay{$_} || !@{$Okay{$_}}) {
	if ($_ == 102) {
	    print "# The failure of test 102 is not necessarily fatal.\n";
	    print "# It usually indicates a problem in the enviroment,\n";
	    print "# not in Perl itself.\n";
	}
	print "not ";
    }
    print "ok $_\n";
}

# Give final advice.

my $didwarn = 0;

foreach (99..$last) {
    if ($Problem{$_}) {
	my @f = sort keys %{ $Problem{$_} };
	my $f = join(" ", @f);
	$f =~ s/(.{50,60}) /$1\n#\t/g;
	print
	    "#\n",
            "# The locale ", (@f == 1 ? "definition" : "definitions"), "\n#\n",
	    "#\t", $f, "\n#\n",
	    "# on your system may have errors because the locale test $_\n",
            "# failed in ", (@f == 1 ? "that locale" : "those locales"),
            ".\n";
	print <<EOW;
#
# If your users are not using these locales you are safe for the moment,
# but please report this failure first to perlbug\@perl.com using the
# perlbug script (as described in the INSTALL file) so that the exact
# details of the failures can be sorted out first and then your operating
# system supplier can be alerted about these anomalies.
#
EOW
	$didwarn = 1;
    }
}

# Tell which locales were okay and which were not.

if ($didwarn) {
    my (@s, @F);
    
    foreach my $l (@Locale) {
	my $p = 0;
	foreach my $t (102..$last) {
	    $p++ if $Problem{$t}{$l};
	}
	push @s, $l if $p == 0;
      push @F, $l unless $p == 0;
    }
    
    if (@s) {
        my $s = join(" ", @s);
        $s =~ s/(.{50,60}) /$1\n#\t/g;

        warn
    	    "# The following locales\n#\n",
            "#\t", $s, "\n#\n",
	    "# tested okay.\n#\n",
    } else {
        warn "# None of your locales were fully okay.\n";
    }

    if (@F) {
        my $F = join(" ", @F);
        $F =~ s/(.{50,60}) /$1\n#\t/g;

        warn
          "# The following locales\n#\n",
            "#\t", $F, "\n#\n",
          "# had problems.\n#\n",
    } else {
        warn "# None of your locales were broken.\n";
    }
}

# eof
