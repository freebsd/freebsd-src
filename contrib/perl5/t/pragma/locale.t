#!./perl -wT

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if (!$Config{d_setlocale} || $Config{ccflags} =~ /\bD?NO_LOCALE\b/) {
	print "1..0\n";
	exit;
    }
}

use strict;

my $have_setlocale = 0;
eval {
    require POSIX;
    import POSIX ':locale_h';
    $have_setlocale++;
};

# Visual C's CRT goes silly on strings of the form "en_US.ISO8859-1"
# and mingw32 uses said silly CRT
$have_setlocale = 0 if $^O eq 'MSWin32' && $Config{cc} =~ /^(cl|gcc)/i;

# 103 (the last test) may fail but that is okay.
# (It indicates something broken in the environment, not Perl)
# Therefore .. only until 102, not 103.
print "1..", ($have_setlocale ? 102 : 98), "\n";

use vars qw($a
	    $English $German $French $Spanish
	    @C @English @German @French @Spanish
	    $Locale @Locale %iLocale %UPPER %lower @Neoalpha);

$a = 'abc %';

sub ok {
    my ($n, $result) = @_;

    print 'not ' unless ($result);
    print "ok $n\n";
}

# First we'll do a lot of taint checking for locales.
# This is the easiest to test, actually, as any locale,
# even the default locale will taint under 'use locale'.

sub is_tainted { # hello, camel two.
    local $^W;	# no warnings 'undef'
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

check_taint      10, sprintf('%e', 123.456);
check_taint      11, sprintf('%f', 123.456);
check_taint      12, sprintf('%g', 123.456);
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
#  unless setlocale() is missing (i.e. minitest).

exit unless $have_setlocale;

sub getalnum {
    sort grep /\w/, map { chr } 0..255
}

sub locatelocale ($$@) {
    my ($lcall, $alnum, @try) = @_;

    undef $$lcall;

    for (@try) {
	local $^W = 0; # suppress "Subroutine LC_ALL redefined"
	if (setlocale(&LC_ALL, $_)) {
	    $$lcall = $_;
	    @$alnum = &getalnum;
	    last;
	}
    }

    @$alnum = () unless (defined $$lcall);
}

# Find some default locale

locatelocale(\$Locale, \@Locale, qw(C POSIX));

# Find some English locale

locatelocale(\$English, \@English,
	     qw(en_US.ISO8859-1 en_GB.ISO8859-1
		en en_US en_UK en_IE en_CA en_AU en_NZ
		english english.iso88591
		american american.iso88591
		british british.iso88591
		));

# Find some German locale

locatelocale(\$German, \@German,
	     qw(de_DE.ISO8859-1 de_AT.ISO8859-1 de_CH.ISO8859-1
		de de_DE de_AT de_CH
		german german.iso88591));

# Find some French locale

locatelocale(\$French, \@French,
	     qw(fr_FR.ISO8859-1 fr_BE.ISO8859-1 fr_CA.ISO8859-1 fr_CH.ISO8859-1
		fr fr_FR fr_BE fr_CA fr_CH
		french french.iso88591));

# Find some Spanish locale

locatelocale(\$Spanish, \@Spanish,
	     qw(es_AR.ISO8859-1 es_BO.ISO8859-1 es_CL.ISO8859-1
		es_CO.ISO8859-1 es_CR.ISO8859-1 es_EC.ISO8859-1
		es_ES.ISO8859-1 es_GT.ISO8859-1 es_MX.ISO8859-1
		es_NI.ISO8859-1 es_PA.ISO8859-1 es_PE.ISO8859-1
		es_PY.ISO8859-1 es_SV.ISO8859-1 es_UY.ISO8859-1 es_VE.ISO8859-1
		es es_AR es_BO es_CL
		es_CO es_CR es_EC
		es_ES es_GT es_MX
		es_NI es_PA es_PE
		es_PY es_SV es_UY es_VE
		spanish spanish.iso88591));

# Select the largest of the alpha(num)bets.

($Locale, @Locale) = ($English, @English)
    if (@English > @Locale);
($Locale, @Locale) = ($German, @German)
    if (@German  > @Locale);
($Locale, @Locale) = ($French, @French)
    if (@French  > @Locale);
($Locale, @Locale) = ($Spanish, @Spanish)
    if (@Spanish > @Locale);

{
    local $^W = 0;
    setlocale(&LC_ALL, $Locale);
}

# Sort it now that LC_ALL has been set.

@Locale = sort @Locale;

print "# Locale = $Locale\n";
print "# Alnum_ = @Locale\n";

{
    my $i = 0;

    for (@Locale) {
	$iLocale{$_} = $i++;
    }
}

# Sieve the uppercase and the lowercase.

for (@Locale) {
    if (/[^\d_]/) { # skip digits and the _
	if (lc eq $_) {
	    $UPPER{$_} = uc;
	} else {
	    $lower{$_} = lc;
	}
    }
}

# Find the alphabets that are not alphabets in the default locale.

{
    no locale;
    
    for (keys %UPPER, keys %lower) {
	push(@Neoalpha, $_) if (/\W/);
    }
}

@Neoalpha = sort @Neoalpha;

# Test \w.

{
    my $word = join('', @Neoalpha);

    $word =~ /^(\w*)$/;

    print 'not ' if ($1 ne $word);
}
print "ok 99\n";

# Find places where the collation order differs from the default locale.

print "# testing 100\n";
{
    my (@k, $i, $j, @d);

    {
	no locale;

	@k = sort (keys %UPPER, keys %lower); 
    }

    for ($i = 0; $i < @k; $i++) {
	for ($j = $i + 1; $j < @k; $j++) {
	    if ($iLocale{$k[$j]} < $iLocale{$k[$i]}) {
		push(@d, [$k[$j], $k[$i]]);
	    }
	}
    }

    # Cross-check those places.

    for (@d) {
	($i, $j) = @$_;
	if ($i gt $j) {
	    print "# failed 100 at:\n";
	    print "# i = $i, j = $j, i ",
	          $i le $j ? 'le' : 'gt', " j\n";
	    print 'not ';
	    last;
	}
    }
}
print "ok 100\n";

# Cross-check whole character set.

print "# testing 101\n";
for (map { chr } 0..255) {
    if (/\w/ and /\W/) { print 'not '; last }
    if (/\d/ and /\D/) { print 'not '; last }
    if (/\s/ and /\S/) { print 'not '; last }
    if (/\w/ and /\D/ and not /_/ and
	not (exists $UPPER{$_} or exists $lower{$_})) {
	print "# failed 101 at:\n";
	print "# ", ord($_), " '$_'\n";
	print 'not ';
	last;
    }
}
print "ok 101\n";

# Test for read-onlys.

print "# testing 102\n";
{
    no locale;
    $a = "qwerty";
    {
	use locale;
	print "not " if $a cmp "qwerty";
    }
}
print "ok 102\n";

# This test must be the last one because its failure is not fatal.
# The @Locale should be internally consistent.
# Thanks to Hallvard Furuseth <h.b.furuseth@usit.uio.no>
# for inventing a way to test for ordering consistency
# without requiring any particular order.
# <jhi@iki.fi>

print "# testing 103\n";
{
    my ($from, $to, $lesser, $greater, @test, %test, $test, $yes, $no, $sign);

    for (0..9) {
	# Select a slice.
	$from = int(($_*@Locale)/10);
	$to = $from + int(@Locale/10);
        $to = $#Locale if ($to > $#Locale);
	$lesser  = join('', @Locale[$from..$to]);
	# Select a slice one character on.
	$from++; $to++;
        $to = $#Locale if ($to > $#Locale);
	$greater = join('', @Locale[$from..$to]);
	($yes, $no, $sign) = ($lesser lt $greater
				? ("    ", "not ", 1)
				: ("not ", "    ", -1));
	# all these tests should FAIL (return 0).
	@test = 
	    (
	     $no.'    ($lesser  lt $greater)',  # 0
	     $no.'    ($lesser  le $greater)',  # 1
	     'not      ($lesser  ne $greater)', # 2
	     '         ($lesser  eq $greater)', # 3
	     $yes.'    ($lesser  ge $greater)', # 4
	     $yes.'    ($lesser  gt $greater)', # 5
	     $yes.'    ($greater lt $lesser )', # 6
	     $yes.'    ($greater le $lesser )', # 7
	     'not      ($greater ne $lesser )', # 8
	     '         ($greater eq $lesser )', # 9
	     $no.'     ($greater ge $lesser )', # 10
	     $no.'     ($greater gt $lesser )', # 11
	     'not (($lesser cmp $greater) == -$sign)' # 12
	     );
	@test{@test} = 0 x @test;
	$test = 0;
	for my $ti (@test) { $test{$ti} = eval $ti ; $test ||= $test{$ti} }
	if ($test) {
	    print "# failed 103 at:\n";
	    print "# lesser  = '$lesser'\n";
	    print "# greater = '$greater'\n";
	    print "# lesser cmp greater = ", $lesser cmp $greater, "\n";
	    print "# greater cmp lesser = ", $greater cmp $lesser, "\n";
	    print "# (greater) from = $from, to = $to\n";
	    for my $ti (@test) {
		printf("# %-40s %-4s", $ti,
		       $test{$ti} ? 'FAIL' : 'ok');
		if ($ti =~ /\(\.*(\$.+ +cmp +\$[^\)]+)\.*\)/) {
		    printf("(%s == %4d)", $1, eval $1);
	        }
		print "\n";
	    }

	    warn "The locale definition on your system may have errors.\n";
	    last;
	}
    }
}

# eof
