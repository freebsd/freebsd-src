#!./perl

print "1..106\n";

#P = start of string  Q = start of substr  R = end of substr  S = end of string

$a = 'abcdefxyz';
BEGIN { $^W = 1 };

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

sub fail { !defined(shift) && $w-- };

print (substr($a,0,3) eq 'abc' ? "ok 1\n" : "not ok 1\n");   # P=Q R S
print (substr($a,3,3) eq 'def' ? "ok 2\n" : "not ok 2\n");   # P Q R S
print (substr($a,6,999) eq 'xyz' ? "ok 3\n" : "not ok 3\n"); # P Q S R
print (fail(substr($a,999,999)) ? "ok 4\n" : "not ok 4\n");  # P R Q S
print (substr($a,0,-6) eq 'abc' ? "ok 5\n" : "not ok 5\n");  # P=Q R S
print (substr($a,-3,1) eq 'x' ? "ok 6\n" : "not ok 6\n");    # P Q R S

$[ = 1;

print (substr($a,1,3) eq 'abc' ? "ok 7\n" : "not ok 7\n");   # P=Q R S
print (substr($a,4,3) eq 'def' ? "ok 8\n" : "not ok 8\n");   # P Q R S
print (substr($a,7,999) eq 'xyz' ? "ok 9\n" : "not ok 9\n"); # P Q S R
print (fail(substr($a,999,999)) ? "ok 10\n" : "not ok 10\n");# P R Q S
print (substr($a,1,-6) eq 'abc' ? "ok 11\n" : "not ok 11\n");# P=Q R S
print (substr($a,-3,1) eq 'x' ? "ok 12\n" : "not ok 12\n");  # P Q R S

$[ = 0;

substr($a,3,3) = 'XYZ';
print $a eq 'abcXYZxyz' ? "ok 13\n" : "not ok 13\n";
substr($a,0,2) = '';
print $a eq 'cXYZxyz' ? "ok 14\n" : "not ok 14\n";
substr($a,0,0) = 'ab';
print $a eq 'abcXYZxyz' ? "ok 15\n" : "not ok 15 $a\n";
substr($a,0,0) = '12345678';
print $a eq '12345678abcXYZxyz' ? "ok 16\n" : "not ok 16\n";
substr($a,-3,3) = 'def';
print $a eq '12345678abcXYZdef' ? "ok 17\n" : "not ok 17\n";
substr($a,-3,3) = '<';
print $a eq '12345678abcXYZ<' ? "ok 18\n" : "not ok 18\n";
substr($a,-1,1) = '12345678';
print $a eq '12345678abcXYZ12345678' ? "ok 19\n" : "not ok 19\n";

$a = 'abcdefxyz';

print (substr($a,6) eq 'xyz' ? "ok 20\n" : "not ok 20\n");   # P Q R=S
print (substr($a,-3) eq 'xyz' ? "ok 21\n" : "not ok 21\n");  # P Q R=S
print (fail(substr($a,999)) ? "ok 22\n" : "not ok 22\n");    # P R=S Q
print (substr($a,0) eq 'abcdefxyz' ? "ok 23\n" : "not ok 23\n");# P=Q R=S
print (substr($a,9) eq '' ? "ok 24\n" : "not ok 24\n");      # P Q=R=S
print (substr($a,-11) eq 'abcdefxyz' ? "ok 25\n" : "not ok 25\n");# Q P R=S
print (substr($a,-9) eq 'abcdefxyz' ? "ok 26\n" : "not ok 26\n");  # P=Q R=S

$a = '54321';

print (fail(substr($a,-7, 1)) ? "ok 27\n" : "not ok 27\n");  # Q R P S
print (fail(substr($a,-7,-6)) ? "ok 28\n" : "not ok 28\n");  # Q R P S
print (substr($a,-5,-7) eq '' ? "ok 29\n" : "not ok 29\n");  # R P=Q S
print (substr($a, 2,-7) eq '' ? "ok 30\n" : "not ok 30\n");  # R P Q S
print (substr($a,-3,-7) eq '' ? "ok 31\n" : "not ok 31\n");  # R P Q S
print (substr($a, 2,-5) eq '' ? "ok 32\n" : "not ok 32\n");  # P=R Q S
print (substr($a,-3,-5) eq '' ? "ok 33\n" : "not ok 33\n");  # P=R Q S
print (substr($a, 2,-4) eq '' ? "ok 34\n" : "not ok 34\n");  # P R Q S
print (substr($a,-3,-4) eq '' ? "ok 35\n" : "not ok 35\n");  # P R Q S
print (substr($a, 5,-6) eq '' ? "ok 36\n" : "not ok 36\n");  # R P Q=S
print (substr($a, 5,-5) eq '' ? "ok 37\n" : "not ok 37\n");  # P=R Q S
print (substr($a, 5,-3) eq '' ? "ok 38\n" : "not ok 38\n");  # P R Q=S
print (fail(substr($a, 7,-7)) ? "ok 39\n" : "not ok 39\n");  # R P S Q
print (fail(substr($a, 7,-5)) ? "ok 40\n" : "not ok 40\n");  # P=R S Q
print (fail(substr($a, 7,-3)) ? "ok 41\n" : "not ok 41\n");  # P R S Q
print (fail(substr($a, 7, 0)) ? "ok 42\n" : "not ok 42\n");  # P S Q=R

print (substr($a,-7,2) eq '' ? "ok 43\n" : "not ok 43\n");   # Q P=R S
print (substr($a,-7,4) eq '54' ? "ok 44\n" : "not ok 44\n"); # Q P R S
print (substr($a,-7,7) eq '54321' ? "ok 45\n" : "not ok 45\n");# Q P R=S
print (substr($a,-7,9) eq '54321' ? "ok 46\n" : "not ok 46\n");# Q P S R
print (substr($a,-5,0) eq '' ? "ok 47\n" : "not ok 47\n");   # P=Q=R S
print (substr($a,-5,3) eq '543' ? "ok 48\n" : "not ok 48\n");# P=Q R S
print (substr($a,-5,5) eq '54321' ? "ok 49\n" : "not ok 49\n");# P=Q R=S
print (substr($a,-5,7) eq '54321' ? "ok 50\n" : "not ok 50\n");# P=Q S R
print (substr($a,-3,0) eq '' ? "ok 51\n" : "not ok 51\n");   # P Q=R S
print (substr($a,-3,3) eq '321' ? "ok 52\n" : "not ok 52\n");# P Q R=S
print (substr($a,-2,3) eq '21' ? "ok 53\n" : "not ok 53\n"); # P Q S R
print (substr($a,0,-5) eq '' ? "ok 54\n" : "not ok 54\n");   # P=Q=R S
print (substr($a,2,-3) eq '' ? "ok 55\n" : "not ok 55\n");   # P Q=R S
print (substr($a,0,0) eq '' ? "ok 56\n" : "not ok 56\n");    # P=Q=R S
print (substr($a,0,5) eq '54321' ? "ok 57\n" : "not ok 57\n");# P=Q R=S
print (substr($a,0,7) eq '54321' ? "ok 58\n" : "not ok 58\n");# P=Q S R
print (substr($a,2,0) eq '' ? "ok 59\n" : "not ok 59\n");    # P Q=R S
print (substr($a,2,3) eq '321' ? "ok 60\n" : "not ok 60\n"); # P Q R=S
print (substr($a,5,0) eq '' ? "ok 61\n" : "not ok 61\n");    # P Q=R=S
print (substr($a,5,2) eq '' ? "ok 62\n" : "not ok 62\n");    # P Q=S R
print (substr($a,-7,-5) eq '' ? "ok 63\n" : "not ok 63\n");  # Q P=R S
print (substr($a,-7,-2) eq '543' ? "ok 64\n" : "not ok 64\n");# Q P R S
print (substr($a,-5,-5) eq '' ? "ok 65\n" : "not ok 65\n");  # P=Q=R S
print (substr($a,-5,-2) eq '543' ? "ok 66\n" : "not ok 66\n");# P=Q R S
print (substr($a,-3,-3) eq '' ? "ok 67\n" : "not ok 67\n");  # P Q=R S
print (substr($a,-3,-1) eq '32' ? "ok 68\n" : "not ok 68\n");# P Q R S

$a = '';

print (substr($a,-2,2) eq '' ? "ok 69\n" : "not ok 69\n");   # Q P=R=S
print (substr($a,0,0) eq '' ? "ok 70\n" : "not ok 70\n");    # P=Q=R=S
print (substr($a,0,1) eq '' ? "ok 71\n" : "not ok 71\n");    # P=Q=S R
print (substr($a,-2,3) eq '' ? "ok 72\n" : "not ok 72\n");   # Q P=S R
print (substr($a,-2) eq '' ? "ok 73\n" : "not ok 73\n");     # Q P=R=S
print (substr($a,0) eq '' ? "ok 74\n" : "not ok 74\n");      # P=Q=R=S


print (substr($a,0,-1) eq '' ? "ok 75\n" : "not ok 75\n");   # R P=Q=S
print (fail(substr($a,-2,0)) ? "ok 76\n" : "not ok 76\n");   # Q=R P=S
print (fail(substr($a,-2,1)) ? "ok 77\n" : "not ok 77\n");   # Q R P=S
print (fail(substr($a,-2,-1)) ? "ok 78\n" : "not ok 78\n");  # Q R P=S
print (fail(substr($a,-2,-2)) ? "ok 79\n" : "not ok 79\n");  # Q=R P=S
print (fail(substr($a,1,-2)) ? "ok 80\n" : "not ok 81\n");   # R P=S Q
print (fail(substr($a,1,1)) ? "ok 81\n" : "not ok 81\n");    # P=S Q R
print (fail(substr($a,1,0)) ? "ok 82\n" : "not ok 82\n");    # P=S Q=R
print (fail(substr($a,1)) ? "ok 83\n" : "not ok 83\n");      # P=R=S Q


my $a = 'zxcvbnm';
substr($a,2,0) = '';
print $a eq 'zxcvbnm' ? "ok 84\n" : "not ok 84\n";
substr($a,7,0) = '';
print $a eq 'zxcvbnm' ? "ok 85\n" : "not ok 85\n";
substr($a,5,0) = '';
print $a eq 'zxcvbnm' ? "ok 86\n" : "not ok 86\n";
substr($a,0,2) = 'pq';
print $a eq 'pqcvbnm' ? "ok 87\n" : "not ok 87\n";
substr($a,2,0) = 'r';
print $a eq 'pqrcvbnm' ? "ok 88\n" : "not ok 88\n";
substr($a,8,0) = 'asd';
print $a eq 'pqrcvbnmasd' ? "ok 89\n" : "not ok 89\n";
substr($a,0,2) = 'iop';
print $a eq 'ioprcvbnmasd' ? "ok 90\n" : "not ok 90\n";
substr($a,0,5) = 'fgh';
print $a eq 'fghvbnmasd' ? "ok 91\n" : "not ok 91\n";
substr($a,3,5) = 'jkl';
print $a eq 'fghjklsd' ? "ok 92\n" : "not ok 92\n";
substr($a,3,2) = '1234';
print $a eq 'fgh1234lsd' ? "ok 93\n" : "not ok 93\n";


# with lexicals (and in re-entered scopes)
for (0,1) {
  my $txt;
  unless ($_) {
    $txt = "Foo";
    substr($txt, -1) = "X";
    print $txt eq "FoX" ? "ok 94\n" : "not ok 94\n";
  }
  else {
    local $^W = 0;    # because of (spurious?) "uninitialised value"
    substr($txt, 0, 1) = "X";
    print $txt eq "X" ? "ok 95\n" : "not ok 95\n";
  }
}

# coercion of references
{
  my $s = [];
  substr($s, 0, 1) = 'Foo';
  print substr($s,0,7) eq "FooRRAY" && !($w-=2) ? "ok 96\n" : "not ok 96\n";
}

# check no spurious warnings
print $w ? "not ok 97\n" : "ok 97\n";

# check new 4 arg replacement syntax
$a = "abcxyz";
$w = 0;
print "not " unless substr($a, 0, 3, "") eq "abc" && $a eq "xyz";
print "ok 98\n";
print "not " unless substr($a, 0, 0, "abc") eq "" && $a eq "abcxyz";
print "ok 99\n";
print "not " unless substr($a, 3, -1, "") eq "xy" && $a eq "abcz";
print "ok 100\n";

print "not " unless substr($a, 3, undef, "xy") eq "" && $a eq "abcxyz"
                 && $w == 3;
print "ok 101\n";
$w = 0;

print "not " unless substr($a, 3, 9999999, "") eq "xyz" && $a eq "abc";
print "ok 102\n";
print "not " unless fail(substr($a, -99, 0, ""));
print "ok 103\n";
print "not " unless fail(substr($a, 99, 3, ""));
print "ok 104\n";

substr($a, 0, length($a), "foo");
print "not " unless $a eq "foo" && !$w;
print "ok 105\n";

# using 4 arg substr as lvalue is a compile time error
eval 'substr($a,0,0,"") = "abc"';
print "not " unless $@ && $@ =~ /Can't modify substr/ && $a eq "foo";
print "ok 106\n";
