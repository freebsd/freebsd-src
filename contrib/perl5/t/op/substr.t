#!./perl

print "1..174\n";

#P = start of string  Q = start of substr  R = end of substr  S = end of string

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}
use warnings ;

$a = 'abcdefxyz';
$SIG{__WARN__} = sub {
     if ($_[0] =~ /^substr outside of string/) {
          $w++;
     } elsif ($_[0] =~ /^Attempt to use reference as lvalue in substr/) {
          $w += 2;
     } elsif ($_[0] =~ /^Use of uninitialized value/) {
          $w += 3;
     } else {
          warn $_[0];
     }
};

sub ok { print (($_[1] ? "" : "not ") . "ok $_[0]\n") }

$FATAL_MSG = '^substr outside of string' ;

ok 1, substr($a,0,3) eq 'abc';   # P=Q R S
ok 2, substr($a,3,3) eq 'def';   # P Q R S
ok 3, substr($a,6,999) eq 'xyz'; # P Q S R
$b = substr($a,999,999) ; # warn # P R Q S
ok 4, $w-- == 1 ;
eval{substr($a,999,999) = "" ; };# P R Q S
ok 5, $@ =~ /$FATAL_MSG/;
ok 6, substr($a,0,-6) eq 'abc';  # P=Q R S
ok 7, substr($a,-3,1) eq 'x';    # P Q R S

$[ = 1;

ok 8, substr($a,1,3) eq 'abc' ;  # P=Q R S
ok 9, substr($a,4,3) eq 'def' ;  # P Q R S
ok 10, substr($a,7,999) eq 'xyz';# P Q S R
$b = substr($a,999,999) ; # warn # P R Q S
ok 11, $w-- == 1 ;
eval{substr($a,999,999) = "" ; } ; # P R Q S
ok 12, $@ =~ /$FATAL_MSG/;
ok 13, substr($a,1,-6) eq 'abc' ;# P=Q R S
ok 14, substr($a,-3,1) eq 'x' ;  # P Q R S

$[ = 0;

substr($a,3,3) = 'XYZ';
ok 15, $a eq 'abcXYZxyz' ;
substr($a,0,2) = '';
ok 16, $a eq 'cXYZxyz' ;
substr($a,0,0) = 'ab';
ok 17, $a eq 'abcXYZxyz' ;
substr($a,0,0) = '12345678';
ok 18, $a eq '12345678abcXYZxyz' ;
substr($a,-3,3) = 'def';
ok 19, $a eq '12345678abcXYZdef';
substr($a,-3,3) = '<';
ok 20, $a eq '12345678abcXYZ<' ;
substr($a,-1,1) = '12345678';
ok 21, $a eq '12345678abcXYZ12345678' ;

$a = 'abcdefxyz';

ok 22, substr($a,6) eq 'xyz' ;        # P Q R=S
ok 23, substr($a,-3) eq 'xyz' ;       # P Q R=S
$b = substr($a,999,999) ; # warning   # P R=S Q
ok 24, $w-- == 1 ;
eval{substr($a,999,999) = "" ; } ;    # P R=S Q
ok 25, $@ =~ /$FATAL_MSG/;
ok 26, substr($a,0) eq 'abcdefxyz' ;  # P=Q R=S
ok 27, substr($a,9) eq '' ;           # P Q=R=S
ok 28, substr($a,-11) eq 'abcdefxyz'; # Q P R=S
ok 29, substr($a,-9) eq 'abcdefxyz';  # P=Q R=S

$a = '54321';

$b = substr($a,-7, 1) ; # warn  # Q R P S
ok 30, $w-- == 1 ;
eval{substr($a,-7, 1) = "" ; }; # Q R P S
ok 31, $@ =~ /$FATAL_MSG/;
$b = substr($a,-7,-6) ; # warn  # Q R P S
ok 32, $w-- == 1 ;
eval{substr($a,-7,-6) = "" ; }; # Q R P S
ok 33, $@ =~ /$FATAL_MSG/;
ok 34, substr($a,-5,-7) eq '';  # R P=Q S
ok 35, substr($a, 2,-7) eq '';  # R P Q S
ok 36, substr($a,-3,-7) eq '';  # R P Q S
ok 37, substr($a, 2,-5) eq '';  # P=R Q S
ok 38, substr($a,-3,-5) eq '';  # P=R Q S
ok 39, substr($a, 2,-4) eq '';  # P R Q S
ok 40, substr($a,-3,-4) eq '';  # P R Q S
ok 41, substr($a, 5,-6) eq '';  # R P Q=S
ok 42, substr($a, 5,-5) eq '';  # P=R Q S
ok 43, substr($a, 5,-3) eq '';  # P R Q=S
$b = substr($a, 7,-7) ; # warn  # R P S Q
ok 44, $w-- == 1 ;
eval{substr($a, 7,-7) = "" ; }; # R P S Q
ok 45, $@ =~ /$FATAL_MSG/;
$b = substr($a, 7,-5) ; # warn  # P=R S Q
ok 46, $w-- == 1 ;
eval{substr($a, 7,-5) = "" ; }; # P=R S Q
ok 47, $@ =~ /$FATAL_MSG/;
$b = substr($a, 7,-3) ; # warn  # P Q S Q
ok 48, $w-- == 1 ;
eval{substr($a, 7,-3) = "" ; }; # P Q S Q
ok 49, $@ =~ /$FATAL_MSG/;
$b = substr($a, 7, 0) ; # warn  # P S Q=R
ok 50, $w-- == 1 ;
eval{substr($a, 7, 0) = "" ; }; # P S Q=R
ok 51, $@ =~ /$FATAL_MSG/;

ok 52, substr($a,-7,2) eq '';   # Q P=R S
ok 53, substr($a,-7,4) eq '54'; # Q P R S
ok 54, substr($a,-7,7) eq '54321';# Q P R=S
ok 55, substr($a,-7,9) eq '54321';# Q P S R
ok 56, substr($a,-5,0) eq '';   # P=Q=R S
ok 57, substr($a,-5,3) eq '543';# P=Q R S
ok 58, substr($a,-5,5) eq '54321';# P=Q R=S
ok 59, substr($a,-5,7) eq '54321';# P=Q S R
ok 60, substr($a,-3,0) eq '';   # P Q=R S
ok 61, substr($a,-3,3) eq '321';# P Q R=S
ok 62, substr($a,-2,3) eq '21'; # P Q S R
ok 63, substr($a,0,-5) eq '';   # P=Q=R S
ok 64, substr($a,2,-3) eq '';   # P Q=R S
ok 65, substr($a,0,0) eq '';    # P=Q=R S
ok 66, substr($a,0,5) eq '54321';# P=Q R=S
ok 67, substr($a,0,7) eq '54321';# P=Q S R
ok 68, substr($a,2,0) eq '';    # P Q=R S
ok 69, substr($a,2,3) eq '321'; # P Q R=S
ok 70, substr($a,5,0) eq '';    # P Q=R=S
ok 71, substr($a,5,2) eq '';    # P Q=S R
ok 72, substr($a,-7,-5) eq '';  # Q P=R S
ok 73, substr($a,-7,-2) eq '543';# Q P R S
ok 74, substr($a,-5,-5) eq '';  # P=Q=R S
ok 75, substr($a,-5,-2) eq '543';# P=Q R S
ok 76, substr($a,-3,-3) eq '';  # P Q=R S
ok 77, substr($a,-3,-1) eq '32';# P Q R S

$a = '';

ok 78, substr($a,-2,2) eq '';   # Q P=R=S
ok 79, substr($a,0,0) eq '';    # P=Q=R=S
ok 80, substr($a,0,1) eq '';    # P=Q=S R
ok 81, substr($a,-2,3) eq '';   # Q P=S R
ok 82, substr($a,-2) eq '';     # Q P=R=S
ok 83, substr($a,0) eq '';      # P=Q=R=S


ok 84, substr($a,0,-1) eq '';   # R P=Q=S
$b = substr($a,-2, 0) ; # warn  # Q=R P=S
ok 85, $w-- == 1 ;
eval{substr($a,-2, 0) = "" ; }; # Q=R P=S
ok 86, $@ =~ /$FATAL_MSG/;

$b = substr($a,-2, 1) ; # warn  # Q R P=S
ok 87, $w-- == 1 ;
eval{substr($a,-2, 1) = "" ; }; # Q R P=S
ok 88, $@ =~ /$FATAL_MSG/;

$b = substr($a,-2,-1) ; # warn  # Q R P=S
ok 89, $w-- == 1 ;
eval{substr($a,-2,-1) = "" ; }; # Q R P=S
ok 90, $@ =~ /$FATAL_MSG/;

$b = substr($a,-2,-2) ; # warn  # Q=R P=S
ok 91, $w-- == 1 ;
eval{substr($a,-2,-2) = "" ; }; # Q=R P=S
ok 92, $@ =~ /$FATAL_MSG/;

$b = substr($a, 1,-2) ; # warn  # R P=S Q
ok 93, $w-- == 1 ;
eval{substr($a, 1,-2) = "" ; }; # R P=S Q
ok 94, $@ =~ /$FATAL_MSG/;

$b = substr($a, 1, 1) ; # warn  # P=S Q R
ok 95, $w-- == 1 ;
eval{substr($a, 1, 1) = "" ; }; # P=S Q R
ok 96, $@ =~ /$FATAL_MSG/;

$b = substr($a, 1, 0) ;# warn   # P=S Q=R
ok 97, $w-- == 1 ;
eval{substr($a, 1, 0) = "" ; }; # P=S Q=R
ok 98, $@ =~ /$FATAL_MSG/;

$b = substr($a,1) ; # warning   # P=R=S Q
ok 99, $w-- == 1 ;
eval{substr($a,1) = "" ; };     # P=R=S Q
ok 100, $@ =~ /$FATAL_MSG/;

my $a = 'zxcvbnm';
substr($a,2,0) = '';
ok 101, $a eq 'zxcvbnm';
substr($a,7,0) = '';
ok 102, $a eq 'zxcvbnm';
substr($a,5,0) = '';
ok 103, $a eq 'zxcvbnm';
substr($a,0,2) = 'pq';
ok 104, $a eq 'pqcvbnm';
substr($a,2,0) = 'r';
ok 105, $a eq 'pqrcvbnm';
substr($a,8,0) = 'asd';
ok 106, $a eq 'pqrcvbnmasd';
substr($a,0,2) = 'iop';
ok 107, $a eq 'ioprcvbnmasd';
substr($a,0,5) = 'fgh';
ok 108, $a eq 'fghvbnmasd';
substr($a,3,5) = 'jkl';
ok 109, $a eq 'fghjklsd';
substr($a,3,2) = '1234';
ok 110, $a eq 'fgh1234lsd';


# with lexicals (and in re-entered scopes)
for (0,1) {
  my $txt;
  unless ($_) {
    $txt = "Foo";
    substr($txt, -1) = "X";
    ok 111, $txt eq "FoX";
  }
  else {
    substr($txt, 0, 1) = "X";
    ok 112, $txt eq "X";
  }
}

$w = 0 ;
# coercion of references
{
  my $s = [];
  substr($s, 0, 1) = 'Foo';
  ok 113, substr($s,0,7) eq "FooRRAY" && !($w-=2);
}

# check no spurious warnings
ok 114, $w == 0;

# check new 4 arg replacement syntax
$a = "abcxyz";
$w = 0;
ok 115, substr($a, 0, 3, "") eq "abc" && $a eq "xyz";
ok 116, substr($a, 0, 0, "abc") eq "" && $a eq "abcxyz";
ok 117, substr($a, 3, -1, "") eq "xy" && $a eq "abcz";

ok 118, substr($a, 3, undef, "xy") eq "" && $a eq "abcxyz"
                 && $w == 3;

$w = 0;

ok 119, substr($a, 3, 9999999, "") eq "xyz" && $a eq "abc";
eval{substr($a, -99, 0, "") };
ok 120, $@ =~ /$FATAL_MSG/;
eval{substr($a, 99, 3, "") };
ok 121, $@ =~ /$FATAL_MSG/;

substr($a, 0, length($a), "foo");
ok 122, $a eq "foo" && !$w;

# using 4 arg substr as lvalue is a compile time error
eval 'substr($a,0,0,"") = "abc"';
ok 123, $@ && $@ =~ /Can't modify substr/ && $a eq "foo";

$a = "abcdefgh";
ok 124, sub { shift }->(substr($a, 0, 4, "xxxx")) eq 'abcd';
ok 125, $a eq 'xxxxefgh';

{
    my $y = 10;
    $y = "2" . $y;
    ok 126, $y+0 == 210;
}

# utf8 sanity
{
    my $x = substr("a\x{263a}b",0);
    ok 127, length($x) == 3;
    $x = substr($x,1,1);
    ok 128, $x eq "\x{263a}";
    $x = $x x 2;
    ok 129, length($x) == 2;
    substr($x,0,1) = "abcd";
    ok 130, $x eq "abcd\x{263a}";
    ok 131, length($x) == 5;
    $x = reverse $x;
    ok 132, length($x) == 5;
    ok 133, $x eq "\x{263a}dcba";

    my $z = 10;
    $z = "21\x{263a}" . $z;
    ok 134, length($z) == 5;
    ok 135, $z eq "21\x{263a}10";
}

# replacement should work on magical values
require Tie::Scalar;
my %data;
tie $data{'a'}, 'Tie::StdScalar';  # makes $data{'a'} magical
$data{a} = "firstlast";
ok 136, substr($data{'a'}, 0, 5, "") eq "first" && $data{'a'} eq "last";

# more utf8

# The following two originally from Ignasi Roca.

$x = "\xF1\xF2\xF3";
substr($x, 0, 1) = "\x{100}"; # Ignasi had \x{FF}
ok 137, length($x) == 3 &&
        $x eq "\x{100}\xF2\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{F3}";

$x = "\xF1\xF2\xF3";
substr($x, 0, 1) = "\x{100}\x{FF}"; # Ignasi had \x{FF}
ok 138, length($x) == 4 &&
        $x eq "\x{100}\x{FF}\xF2\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{F2}" &&
        substr($x, 3, 1) eq "\x{F3}";

# more utf8 lval exercise

$x = "\xF1\xF2\xF3";
substr($x, 0, 2) = "\x{100}\xFF";
ok 139, length($x) == 3 &&
        $x eq "\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{F3}";

$x = "\xF1\xF2\xF3";
substr($x, 1, 1) = "\x{100}\xFF";
ok 140, length($x) == 4 &&
        $x eq "\xF1\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{F1}" &&
        substr($x, 1, 1) eq "\x{100}" &&
        substr($x, 2, 1) eq "\x{FF}" &&
        substr($x, 3, 1) eq "\x{F3}";

$x = "\xF1\xF2\xF3";
substr($x, 2, 1) = "\x{100}\xFF";
ok 141, length($x) == 4 &&
        $x eq "\xF1\xF2\x{100}\xFF" &&
        substr($x, 0, 1) eq "\x{F1}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{100}" &&
        substr($x, 3, 1) eq "\x{FF}";

$x = "\xF1\xF2\xF3";
substr($x, 3, 1) = "\x{100}\xFF";
ok 142, length($x) == 5 &&
        $x eq "\xF1\xF2\xF3\x{100}\xFF" &&
        substr($x, 0, 1) eq "\x{F1}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{F3}" &&
        substr($x, 3, 1) eq "\x{100}" &&
        substr($x, 4, 1) eq "\x{FF}";

$x = "\xF1\xF2\xF3";
substr($x, -1, 1) = "\x{100}\xFF";
ok 143, length($x) == 4 &&
        $x eq "\xF1\xF2\x{100}\xFF" &&
        substr($x, 0, 1) eq "\x{F1}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{100}" &&
        substr($x, 3, 1) eq "\x{FF}";

$x = "\xF1\xF2\xF3";
substr($x, -1, 0) = "\x{100}\xFF";
ok 144, length($x) == 5 &&
        $x eq "\xF1\xF2\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{F1}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{100}" &&
        substr($x, 3, 1) eq "\x{FF}" &&
        substr($x, 4, 1) eq "\x{F3}";

$x = "\xF1\xF2\xF3";
substr($x, 0, -1) = "\x{100}\xFF";
ok 145, length($x) == 3 &&
        $x eq "\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{F3}";

$x = "\xF1\xF2\xF3";
substr($x, 0, -2) = "\x{100}\xFF";
ok 146, length($x) == 4 &&
        $x eq "\x{100}\xFF\xF2\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{F2}" &&
        substr($x, 3, 1) eq "\x{F3}";

$x = "\xF1\xF2\xF3";
substr($x, 0, -3) = "\x{100}\xFF";
ok 147, length($x) == 5 &&
        $x eq "\x{100}\xFF\xF1\xF2\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{F1}" &&
        substr($x, 3, 1) eq "\x{F2}" &&
        substr($x, 4, 1) eq "\x{F3}";

$x = "\xF1\xF2\xF3";
substr($x, 1, -1) = "\x{100}\xFF";
ok 148, length($x) == 4 &&
        $x eq "\xF1\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{F1}" &&
        substr($x, 1, 1) eq "\x{100}" &&
        substr($x, 2, 1) eq "\x{FF}" &&
        substr($x, 3, 1) eq "\x{F3}";

$x = "\xF1\xF2\xF3";
substr($x, -1, -1) = "\x{100}\xFF";
ok 149, length($x) == 5 &&
        $x eq "\xF1\xF2\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{F1}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{100}" &&
        substr($x, 3, 1) eq "\x{FF}" &&
        substr($x, 4, 1) eq "\x{F3}";

# And tests for already-UTF8 one

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, 1) = "\x{100}";
ok 150, length($x) == 3 &&
        $x eq "\x{100}\xF2\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{F3}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, 1) = "\x{100}\x{FF}";
ok 151, length($x) == 4 &&
        $x eq "\x{100}\x{FF}\xF2\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{F2}" &&
        substr($x, 3, 1) eq "\x{F3}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, 2) = "\x{100}\xFF";
ok 152, length($x) == 3 &&
        $x eq "\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{F3}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 1, 1) = "\x{100}\xFF";
ok 153, length($x) == 4 &&
        $x eq "\x{101}\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{101}" &&
        substr($x, 1, 1) eq "\x{100}" &&
        substr($x, 2, 1) eq "\x{FF}" &&
        substr($x, 3, 1) eq "\x{F3}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 2, 1) = "\x{100}\xFF";
ok 154, length($x) == 4 &&
        $x eq "\x{101}\xF2\x{100}\xFF" &&
        substr($x, 0, 1) eq "\x{101}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{100}" &&
        substr($x, 3, 1) eq "\x{FF}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 3, 1) = "\x{100}\xFF";
ok 155, length($x) == 5 &&
        $x eq "\x{101}\x{F2}\x{F3}\x{100}\xFF" &&
        substr($x, 0, 1) eq "\x{101}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{F3}" &&
        substr($x, 3, 1) eq "\x{100}" &&
        substr($x, 4, 1) eq "\x{FF}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, -1, 1) = "\x{100}\xFF";
ok 156, length($x) == 4 &&
        $x eq "\x{101}\xF2\x{100}\xFF" &&
        substr($x, 0, 1) eq "\x{101}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{100}" &&
        substr($x, 3, 1) eq "\x{FF}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, -1, 0) = "\x{100}\xFF";
ok 157, length($x) == 5 &&
        $x eq "\x{101}\xF2\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{101}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{100}" &&
        substr($x, 3, 1) eq "\x{FF}" &&
        substr($x, 4, 1) eq "\x{F3}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, -1) = "\x{100}\xFF";
ok 158, length($x) == 3 &&
        $x eq "\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{F3}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, -2) = "\x{100}\xFF";
ok 159, length($x) == 4 &&
        $x eq "\x{100}\xFF\xF2\xF3" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{F2}" &&
        substr($x, 3, 1) eq "\x{F3}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 0, -3) = "\x{100}\xFF";
ok 160, length($x) == 5 &&
        $x eq "\x{100}\xFF\x{101}\x{F2}\x{F3}" &&
        substr($x, 0, 1) eq "\x{100}" &&
        substr($x, 1, 1) eq "\x{FF}" &&
        substr($x, 2, 1) eq "\x{101}" &&
        substr($x, 3, 1) eq "\x{F2}" &&
        substr($x, 4, 1) eq "\x{F3}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, 1, -1) = "\x{100}\xFF";
ok 161, length($x) == 4 &&
        $x eq "\x{101}\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{101}" &&
        substr($x, 1, 1) eq "\x{100}" &&
        substr($x, 2, 1) eq "\x{FF}" &&
        substr($x, 3, 1) eq "\x{F3}";

$x = "\x{101}\x{F2}\x{F3}";
substr($x, -1, -1) = "\x{100}\xFF";
ok 162, length($x) == 5 &&
        $x eq "\x{101}\xF2\x{100}\xFF\xF3" &&
        substr($x, 0, 1) eq "\x{101}" &&
        substr($x, 1, 1) eq "\x{F2}" &&
        substr($x, 2, 1) eq "\x{100}" &&
        substr($x, 3, 1) eq "\x{FF}" &&
        substr($x, 4, 1) eq "\x{F3}";

substr($x = "ab", 0, 0, "\x{100}\x{200}");
ok 163, $x eq "\x{100}\x{200}ab";

substr($x = "\x{100}\x{200}", 0, 0, "ab");
ok 164, $x eq "ab\x{100}\x{200}";

substr($x = "ab", 1, 0, "\x{100}\x{200}");
ok 165, $x eq "a\x{100}\x{200}b";

substr($x = "\x{100}\x{200}", 1, 0, "ab");
ok 166, $x eq "\x{100}ab\x{200}";

substr($x = "ab", 2, 0, "\x{100}\x{200}");
ok 167, $x eq "ab\x{100}\x{200}";

substr($x = "\x{100}\x{200}", 2, 0, "ab");
ok 168, $x eq "\x{100}\x{200}ab";

substr($x = "\xFFb", 0, 0, "\x{100}\x{200}");
ok 169, $x eq "\x{100}\x{200}\xFFb";

substr($x = "\x{100}\x{200}", 0, 0, "\xFFb");
ok 170, $x eq "\xFFb\x{100}\x{200}";

substr($x = "\xFFb", 1, 0, "\x{100}\x{200}");
ok 171, $x eq "\xFF\x{100}\x{200}b";

substr($x = "\x{100}\x{200}", 1, 0, "\xFFb");
ok 172, $x eq "\x{100}\xFFb\x{200}";

substr($x = "\xFFb", 2, 0, "\x{100}\x{200}");
ok 173, $x eq "\xFFb\x{100}\x{200}";

substr($x = "\x{100}\x{200}", 2, 0, "\xFFb");
ok 174, $x eq "\x{100}\x{200}\xFFb";

