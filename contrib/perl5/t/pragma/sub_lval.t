print "1..64\n";

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

sub a : lvalue { my $a = 34; ${\(bless \$a)} }  # Return a temporary
sub b : lvalue { ${\shift} }

my $out = a(b());		# Check that temporaries are allowed.
print "# `$out'\nnot " unless ref $out eq 'main'; # Not reached if error.
print "ok 1\n";

my @out = grep /main/, a(b()); # Check that temporaries are allowed.
print "# `@out'\nnot " unless @out==1; # Not reached if error.
print "ok 2\n";

my $in;

# Check that we can return localized values from subroutines:

sub in : lvalue { $in = shift; }
sub neg : lvalue {  #(num_str) return num_str
    local $_ = shift;
    s/^\+/-/;
    $_;
}
in(neg("+2"));


print "# `$in'\nnot " unless $in eq '-2';
print "ok 3\n";

sub get_lex : lvalue { $in }
sub get_st : lvalue { $blah }
sub id : lvalue { ${\shift} }
sub id1 : lvalue { $_[0] }
sub inc : lvalue { ${\++$_[0]} }

$in = 5;
$blah = 3;

get_st = 7;

print "# `$blah' ne 7\nnot " unless $blah eq 7;
print "ok 4\n";

get_lex = 7;

print "# `$in' ne 7\nnot " unless $in eq 7;
print "ok 5\n";

++get_st;

print "# `$blah' ne 8\nnot " unless $blah eq 8;
print "ok 6\n";

++get_lex;

print "# `$in' ne 8\nnot " unless $in eq 8;
print "ok 7\n";

id(get_st) = 10;

print "# `$blah' ne 10\nnot " unless $blah eq 10;
print "ok 8\n";

id(get_lex) = 10;

print "# `$in' ne 10\nnot " unless $in eq 10;
print "ok 9\n";

++id(get_st);

print "# `$blah' ne 11\nnot " unless $blah eq 11;
print "ok 10\n";

++id(get_lex);

print "# `$in' ne 11\nnot " unless $in eq 11;
print "ok 11\n";

id1(get_st) = 20;

print "# `$blah' ne 20\nnot " unless $blah eq 20;
print "ok 12\n";

id1(get_lex) = 20;

print "# `$in' ne 20\nnot " unless $in eq 20;
print "ok 13\n";

++id1(get_st);

print "# `$blah' ne 21\nnot " unless $blah eq 21;
print "ok 14\n";

++id1(get_lex);

print "# `$in' ne 21\nnot " unless $in eq 21;
print "ok 15\n";

inc(get_st);

print "# `$blah' ne 22\nnot " unless $blah eq 22;
print "ok 16\n";

inc(get_lex);

print "# `$in' ne 22\nnot " unless $in eq 22;
print "ok 17\n";

inc(id(get_st));

print "# `$blah' ne 23\nnot " unless $blah eq 23;
print "ok 18\n";

inc(id(get_lex));

print "# `$in' ne 23\nnot " unless $in eq 23;
print "ok 19\n";

++inc(id1(id(get_st)));

print "# `$blah' ne 25\nnot " unless $blah eq 25;
print "ok 20\n";

++inc(id1(id(get_lex)));

print "# `$in' ne 25\nnot " unless $in eq 25;
print "ok 21\n";

@a = (1) x 3;
@b = (undef) x 2;
$#c = 3;			# These slots are not fillable.

# Explanation: empty slots contain &sv_undef.

=for disabled constructs

sub a3 :lvalue {@a}
sub b2 : lvalue {@b}
sub c4: lvalue {@c}

$_ = '';

eval <<'EOE' or $_ = $@;
  ($x, a3, $y, b2, $z, c4, $t) = (34 .. 78);
  1;
EOE

#@out = ($x, a3, $y, b2, $z, c4, $t);
#@in = (34 .. 41, (undef) x 4, 46);
#print "# `@out' ne `@in'\nnot " unless "@out" eq "@in";

print "# '$_'.\nnot "
  unless /Can\'t return an uninitialized value from lvalue subroutine/;
=cut

print "ok 22\n";

my $var;

sub a::var : lvalue { $var }

"a"->var = 45;

print "# `$var' ne 45\nnot " unless $var eq 45;
print "ok 23\n";

my $oo;
$o = bless \$oo, "a";

$o->var = 47;

print "# `$var' ne 47\nnot " unless $var eq 47;
print "ok 24\n";

sub o : lvalue { $o }

o->var = 49;

print "# `$var' ne 49\nnot " unless $var eq 49;
print "ok 25\n";

sub nolv () { $x0, $x1 } # Not lvalue

$_ = '';

eval <<'EOE' or $_ = $@;
  nolv = (2,3);
  1;
EOE

print "not "
  unless /Can\'t modify non-lvalue subroutine call in scalar assignment/;
print "ok 26\n";

$_ = '';

eval <<'EOE' or $_ = $@;
  nolv = (2,3) if $_;
  1;
EOE

print "not "
  unless /Can\'t modify non-lvalue subroutine call in scalar assignment/;
print "ok 27\n";

$_ = '';

eval <<'EOE' or $_ = $@;
  &nolv = (2,3) if $_;
  1;
EOE

print "not "
  unless /Can\'t modify non-lvalue subroutine call in scalar assignment/;
print "ok 28\n";

$x0 = $x1 = $_ = undef;
$nolv = \&nolv;

eval <<'EOE' or $_ = $@;
  $nolv->() = (2,3) if $_;
  1;
EOE

print "# '$_', '$x0', '$x1'.\nnot " if defined $_;
print "ok 29\n";

$x0 = $x1 = $_ = undef;
$nolv = \&nolv;

eval <<'EOE' or $_ = $@;
  $nolv->() = (2,3);
  1;
EOE

print "# '$_', '$x0', '$x1'.\nnot "
  unless /Can\'t modify non-lvalue subroutine call/;
print "ok 30\n";

sub lv0 : lvalue { }		# Converted to lv10 in scalar context

$_ = undef;
eval <<'EOE' or $_ = $@;
  lv0 = (2,3);
  1;
EOE

print "# '$_'.\nnot "
  unless /Can\'t return a readonly value from lvalue subroutine/;
print "ok 31\n";

sub lv10 : lvalue {}

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv0) = (2,3);
  1;
EOE

print "# '$_'.\nnot " if defined $_;
print "ok 32\n";

sub lv1u :lvalue { undef }

$_ = undef;
eval <<'EOE' or $_ = $@;
  lv1u = (2,3);
  1;
EOE

print "# '$_'.\nnot "
  unless /Can\'t return a readonly value from lvalue subroutine/;
print "ok 33\n";

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv1u) = (2,3);
  1;
EOE

print "# '$_'.\nnot "
  unless /Can\'t return an uninitialized value from lvalue subroutine/;
print "ok 34\n";

$x = '1234567';

$_ = undef;
eval <<'EOE' or $_ = $@;
  sub lv1t : lvalue { index $x, 2 }
  lv1t = (2,3);
  1;
EOE

print "# '$_'.\nnot "
  unless /Can\'t modify index in lvalue subroutine return/;
print "ok 35\n";

$_ = undef;
eval <<'EOE' or $_ = $@;
  sub lv2t : lvalue { shift }
  (lv2t) = (2,3);
  1;
EOE

print "# '$_'.\nnot "
  unless /Can\'t modify shift in lvalue subroutine return/;
print "ok 36\n";

$xxx = 'xxx';
sub xxx () { $xxx }  # Not lvalue

$_ = undef;
eval <<'EOE' or $_ = $@;
  sub lv1tmp : lvalue { xxx }			# is it a TEMP?
  lv1tmp = (2,3);
  1;
EOE

print "# '$_'.\nnot "
  unless /Can\'t modify non-lvalue subroutine call in lvalue subroutine return/;
print "ok 37\n";

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv1tmp) = (2,3);
  1;
EOE

print "# '$_'.\nnot "
  unless /Can\'t return a temporary from lvalue subroutine/;
print "ok 38\n";

sub yyy () { 'yyy' } # Const, not lvalue

$_ = undef;
eval <<'EOE' or $_ = $@;
  sub lv1tmpr : lvalue { yyy }			# is it read-only?
  lv1tmpr = (2,3);
  1;
EOE

print "# '$_'.\nnot "
  unless /Can\'t modify constant item in lvalue subroutine return/;
print "ok 39\n";

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv1tmpr) = (2,3);
  1;
EOE

print "# '$_'.\nnot "
  unless /Can\'t return a readonly value from lvalue subroutine/;
print "ok 40\n";

sub lva : lvalue {@a}

$_ = undef;
@a = ();
$a[1] = 12;
eval <<'EOE' or $_ = $@;
  (lva) = (2,3);
  1;
EOE

print "# '$_'.\nnot " unless "'@a' $_" eq "'2 3' ";
print "ok 41\n";

$_ = undef;
@a = ();
$a[0] = undef;
$a[1] = 12;
eval <<'EOE' or $_ = $@;
  (lva) = (2,3);
  1;
EOE

print "# '$_'.\nnot " unless "'@a' $_" eq "'2 3' ";
print "ok 42\n";

$_ = undef;
@a = ();
$a[0] = undef;
$a[1] = 12;
eval <<'EOE' or $_ = $@;
  (lva) = (2,3);
  1;
EOE

print "# '$_'.\nnot " unless "'@a' $_" eq "'2 3' ";
print "ok 43\n";

sub lv1n : lvalue { $newvar }

$_ = undef;
eval <<'EOE' or $_ = $@;
  lv1n = (3,4);
  1;
EOE

print "# '$_', '$newvar'.\nnot " unless "'$newvar' $_" eq "'4' ";
print "ok 44\n";

sub lv1nn : lvalue { $nnewvar }

$_ = undef;
eval <<'EOE' or $_ = $@;
  (lv1nn) = (3,4);
  1;
EOE

print "# '$_'.\nnot " unless "'$nnewvar' $_" eq "'3' ";
print "ok 45\n";

$a = \&lv1nn;
$a->() = 8;
print "# '$nnewvar'.\nnot " unless $nnewvar eq '8';
print "ok 46\n";

# This must happen at run time
eval {
    sub AUTOLOAD : lvalue { $newvar };
};
foobar() = 12;
print "# '$newvar'.\nnot " unless $newvar eq "12";
print "ok 47\n";

# Testing DWIM of foo = bar;
sub foo : lvalue {
    $a;
}
$a = "not ok 48\n";
foo = "ok 48\n";
print $a;

open bar, ">nothing" or die $!; 
bar = *STDOUT;
print bar "ok 49\n";
unlink "nothing";

{
my %hash; my @array;
sub alv : lvalue { $array[1] }
sub alv2 : lvalue { $array[$_[0]] }
sub hlv : lvalue { $hash{"foo"} }
sub hlv2 : lvalue { $hash{$_[0]} }
$array[1] = "not ok 51\n";
alv() = "ok 50\n";
print alv();

alv2(20) = "ok 51\n";
print $array[20];

$hash{"foo"} = "not ok 52\n";
hlv() = "ok 52\n";
print $hash{foo};

$hash{bar} = "not ok 53\n";
hlv("bar") = "ok 53\n";
print hlv("bar");

sub array : lvalue  { @array  }
sub array2 : lvalue { @array2 } # This is a global.
sub hash : lvalue   { %hash   }
sub hash2 : lvalue  { %hash2  } # So's this.
@array2 = qw(foo bar);
%hash2 = qw(foo bar);

(array()) = qw(ok 54);
print "not " unless "@array" eq "ok 54";
print "ok 54\n";

(array2()) = qw(ok 55);
print "not " unless "@array2" eq "ok 55";
print "ok 55\n";

(hash()) = qw(ok 56);
print "not " unless $hash{ok} == 56;
print "ok 56\n";

(hash2()) = qw(ok 57);
print "not " unless $hash2{ok} == 57;
print "ok 57\n";

@array = qw(a b c d);
sub aslice1 : lvalue { @array[0,2] };
(aslice1()) = ("ok", "already");
print "# @array\nnot " unless "@array" eq "ok b already d";
print "ok 58\n";

@array2 = qw(a B c d);
sub aslice2 : lvalue { @array2[0,2] };
(aslice2()) = ("ok", "already");
print "not " unless "@array2" eq "ok B already d";
print "ok 59\n";

%hash = qw(a Alpha b Beta c Gamma);
sub hslice : lvalue { @hash{"c", "b"} }
(hslice()) = ("CISC", "BogoMIPS");
print "not " unless join("/",@hash{"c","a","b"}) eq "CISC/Alpha/BogoMIPS";
print "ok 60\n";
}

$str = "Hello, world!";
sub sstr : lvalue { substr($str, 1, 4) }
sstr() = "i";
print "not " unless $str eq "Hi, world!";
print "ok 61\n";

$str = "Made w/ JavaScript";
sub veclv : lvalue { vec($str, 2, 32) }
if (ord('A') != 193) {
    veclv() = 0x5065726C;
}
else { # EBCDIC?
    veclv() = 0xD7859993;
}
print "# $str\nnot " unless $str eq "Made w/ PerlScript";
print "ok 62\n";

sub position : lvalue { pos }
@p = ();
$_ = "fee fi fo fum";
while (/f/g) {
    push @p, position;
    position() += 6;
}
print "# @p\nnot " unless "@p" eq "1 8";
print "ok 63\n";

# Bug 20001223.002: split thought that the list had only one element
@ary = qw(4 5 6);
sub lval1 : lvalue { $ary[0]; }
sub lval2 : lvalue { $ary[1]; }
(lval1(), lval2()) = split ' ', "1 2 3 4";
print "not " unless join(':', @ary) eq "1:2:6";
print "ok 64\n";
