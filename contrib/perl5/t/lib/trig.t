#!./perl 

#
# Regression tests for the Math::Trig package
#
# The tests are quite modest as the Math::Complex tests exercise
# these quite vigorously.
# 
# -- Jarkko Hietaniemi, April 1997

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Math::Trig;

use strict;

use vars qw($x $y $z);

my $eps = 1e-11;

if ($^O eq 'unicos') { # See lib/Math/Complex.pm and t/lib/complex.t.
    $eps = 1e-10;
}

sub near ($$;$) {
    my $e = defined $_[2] ? $_[2] : $eps;
    $_[1] ? (abs($_[0]/$_[1] - 1) < $e) : abs($_[0]) < $e;
}

print "1..23\n";

$x = 0.9;
print 'not ' unless (near(tan($x), sin($x) / cos($x)));
print "ok 1\n";

print 'not ' unless (near(sinh(2), 3.62686040784702));
print "ok 2\n";

print 'not ' unless (near(acsch(0.1), 2.99822295029797));
print "ok 3\n";

$x = asin(2);
print 'not ' unless (ref $x eq 'Math::Complex');
print "ok 4\n";

# avoid using Math::Complex here
$x =~ /^([^-]+)(-[^i]+)i$/;
($y, $z) = ($1, $2);
print 'not ' unless (near($y,  1.5707963267949) and
		     near($z, -1.31695789692482));
print "ok 5\n";

print 'not ' unless (near(deg2rad(90), pi/2));
print "ok 6\n";

print 'not ' unless (near(rad2deg(pi), 180));
print "ok 7\n";

use Math::Trig ':radial';

{
    my ($r,$t,$z) = cartesian_to_cylindrical(1,1,1);

    print 'not ' unless (near($r, sqrt(2)))     and
	                (near($t, deg2rad(45))) and
			(near($z, 1));
    print "ok 8\n";

    ($x,$y,$z) = cylindrical_to_cartesian($r, $t, $z);

    print 'not ' unless (near($x, 1)) and
	                (near($y, 1)) and
			(near($z, 1));
    print "ok 9\n";

    ($r,$t,$z) = cartesian_to_cylindrical(1,1,0);

    print 'not ' unless (near($r, sqrt(2)))     and
	                (near($t, deg2rad(45))) and
			(near($z, 0));
    print "ok 10\n";

    ($x,$y,$z) = cylindrical_to_cartesian($r, $t, $z);

    print 'not ' unless (near($x, 1)) and
	                (near($y, 1)) and
			(near($z, 0));
    print "ok 11\n";
}

{
    my ($r,$t,$f) = cartesian_to_spherical(1,1,1);

    print 'not ' unless (near($r, sqrt(3)))     and
	                (near($t, deg2rad(45))) and
			(near($f, atan2(sqrt(2), 1)));
    print "ok 12\n";

    ($x,$y,$z) = spherical_to_cartesian($r, $t, $f);

    print 'not ' unless (near($x, 1)) and
	                (near($y, 1)) and
			(near($z, 1));
    print "ok 13\n";

    ($r,$t,$f) = cartesian_to_spherical(1,1,0);

    print 'not ' unless (near($r, sqrt(2)))     and
	                (near($t, deg2rad(45))) and
			(near($f, deg2rad(90)));
    print "ok 14\n";

    ($x,$y,$z) = spherical_to_cartesian($r, $t, $f);

    print 'not ' unless (near($x, 1)) and
	                (near($y, 1)) and
			(near($z, 0));
    print "ok 15\n";
}

{
    my ($r,$t,$z) = cylindrical_to_spherical(spherical_to_cylindrical(1,1,1));

    print 'not ' unless (near($r, 1)) and
	                (near($t, 1)) and
			(near($z, 1));
    print "ok 16\n";

    ($r,$t,$z) = spherical_to_cylindrical(cylindrical_to_spherical(1,1,1));

    print 'not ' unless (near($r, 1)) and
	                (near($t, 1)) and
			(near($z, 1));
    print "ok 17\n";
}

{
    use Math::Trig 'great_circle_distance';

    print 'not '
	unless (near(great_circle_distance(0, 0, 0, pi/2), pi/2));
    print "ok 18\n";

    print 'not '
	unless (near(great_circle_distance(0, 0, pi, pi), pi));
    print "ok 19\n";

    # London to Tokyo.
    my @L = (deg2rad(-0.5), deg2rad(90 - 51.3));
    my @T = (deg2rad(139.8),deg2rad(90 - 35.7));

    my $km = great_circle_distance(@L, @T, 6378);

    print 'not ' unless (near($km, 9605.26637021388));
    print "ok 20\n";
}

{
    my $R2D = 57.295779513082320876798154814169;

    sub frac { $_[0] - int($_[0]) }

    my $lotta_radians = deg2rad(1E+20, 1);
    print "not " unless near($lotta_radians,  1E+20/$R2D);
    print "ok 21\n";

    my $negat_degrees = rad2deg(-1E20, 1);
    print "not " unless near($negat_degrees, -1E+20*$R2D);
    print "ok 22\n";

    my $posit_degrees = rad2deg(-10000, 1);
    print "not " unless near($posit_degrees, -10000*$R2D);
    print "ok 23\n";
}

# eof
