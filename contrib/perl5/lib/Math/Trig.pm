#
# Trigonometric functions, mostly inherited from Math::Complex.
# -- Jarkko Hietaniemi, since April 1997
# -- Raphael Manfredi, September 1996 (indirectly: because of Math::Complex)
#

require Exporter;
package Math::Trig;

use 5.005_64;
use strict;

use Math::Complex qw(:trig);

our($VERSION, $PACKAGE, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

@ISA = qw(Exporter);

$VERSION = 1.00;

my @angcnv = qw(rad2deg rad2grad
	     deg2rad deg2grad
	     grad2rad grad2deg);

@EXPORT = (@{$Math::Complex::EXPORT_TAGS{'trig'}},
	   @angcnv);

my @rdlcnv = qw(cartesian_to_cylindrical
		cartesian_to_spherical
		cylindrical_to_cartesian
		cylindrical_to_spherical
		spherical_to_cartesian
		spherical_to_cylindrical);

@EXPORT_OK = (@rdlcnv, 'great_circle_distance');

%EXPORT_TAGS = ('radial' => [ @rdlcnv ]);

sub pi2  () { 2 * pi }
sub pip2 () { pi / 2 }

sub DR  () { pi2/360 }
sub RD  () { 360/pi2 }
sub DG  () { 400/360 }
sub GD  () { 360/400 }
sub RG  () { 400/pi2 }
sub GR  () { pi2/400 }

#
# Truncating remainder.
#

sub remt ($$) {
    # Oh yes, POSIX::fmod() would be faster. Possibly. If it is available.
    $_[0] - $_[1] * int($_[0] / $_[1]);
}

#
# Angle conversions.
#

sub rad2rad($)     { remt($_[0], pi2) }

sub deg2deg($)     { remt($_[0], 360) }

sub grad2grad($)   { remt($_[0], 400) }

sub rad2deg ($;$)  { my $d = RD * $_[0]; $_[1] ? $d : deg2deg($d) }

sub deg2rad ($;$)  { my $d = DR * $_[0]; $_[1] ? $d : rad2rad($d) }

sub grad2deg ($;$) { my $d = GD * $_[0]; $_[1] ? $d : deg2deg($d) }

sub deg2grad ($;$) { my $d = DG * $_[0]; $_[1] ? $d : grad2grad($d) }

sub rad2grad ($;$) { my $d = RG * $_[0]; $_[1] ? $d : grad2grad($d) }

sub grad2rad ($;$) { my $d = GR * $_[0]; $_[1] ? $d : rad2rad($d) }

sub cartesian_to_spherical {
    my ( $x, $y, $z ) = @_;

    my $rho = sqrt( $x * $x + $y * $y + $z * $z );

    return ( $rho,
             atan2( $y, $x ),
             $rho ? acos( $z / $rho ) : 0 );
}

sub spherical_to_cartesian {
    my ( $rho, $theta, $phi ) = @_;

    return ( $rho * cos( $theta ) * sin( $phi ),
             $rho * sin( $theta ) * sin( $phi ),
             $rho * cos( $phi   ) );
}

sub spherical_to_cylindrical {
    my ( $x, $y, $z ) = spherical_to_cartesian( @_ );

    return ( sqrt( $x * $x + $y * $y ), $_[1], $z );
}

sub cartesian_to_cylindrical {
    my ( $x, $y, $z ) = @_;

    return ( sqrt( $x * $x + $y * $y ), atan2( $y, $x ), $z );
}

sub cylindrical_to_cartesian {
    my ( $rho, $theta, $z ) = @_;

    return ( $rho * cos( $theta ), $rho * sin( $theta ), $z );
}

sub cylindrical_to_spherical {
    return ( cartesian_to_spherical( cylindrical_to_cartesian( @_ ) ) );
}

sub great_circle_distance {
    my ( $theta0, $phi0, $theta1, $phi1, $rho ) = @_;

    $rho = 1 unless defined $rho; # Default to the unit sphere.

    my $lat0 = pip2 - $phi0;
    my $lat1 = pip2 - $phi1;

    return $rho *
        acos(cos( $lat0 ) * cos( $lat1 ) * cos( $theta0 - $theta1 ) +
             sin( $lat0 ) * sin( $lat1 ) );
}

=pod

=head1 NAME

Math::Trig - trigonometric functions

=head1 SYNOPSIS

	use Math::Trig;

	$x = tan(0.9);
	$y = acos(3.7);
	$z = asin(2.4);

	$halfpi = pi/2;

	$rad = deg2rad(120);

=head1 DESCRIPTION

C<Math::Trig> defines many trigonometric functions not defined by the
core Perl which defines only the C<sin()> and C<cos()>.  The constant
B<pi> is also defined as are a few convenience functions for angle
conversions.

=head1 TRIGONOMETRIC FUNCTIONS

The tangent

=over 4

=item B<tan>

=back

The cofunctions of the sine, cosine, and tangent (cosec/csc and cotan/cot
are aliases)

B<csc>, B<cosec>, B<sec>, B<sec>, B<cot>, B<cotan>

The arcus (also known as the inverse) functions of the sine, cosine,
and tangent

B<asin>, B<acos>, B<atan>

The principal value of the arc tangent of y/x

B<atan2>(y, x)

The arcus cofunctions of the sine, cosine, and tangent (acosec/acsc
and acotan/acot are aliases)

B<acsc>, B<acosec>, B<asec>, B<acot>, B<acotan>

The hyperbolic sine, cosine, and tangent

B<sinh>, B<cosh>, B<tanh>

The cofunctions of the hyperbolic sine, cosine, and tangent (cosech/csch
and cotanh/coth are aliases)

B<csch>, B<cosech>, B<sech>, B<coth>, B<cotanh>

The arcus (also known as the inverse) functions of the hyperbolic
sine, cosine, and tangent

B<asinh>, B<acosh>, B<atanh>

The arcus cofunctions of the hyperbolic sine, cosine, and tangent
(acsch/acosech and acoth/acotanh are aliases)

B<acsch>, B<acosech>, B<asech>, B<acoth>, B<acotanh>

The trigonometric constant B<pi> is also defined.

$pi2 = 2 * B<pi>;

=head2 ERRORS DUE TO DIVISION BY ZERO

The following functions

	acoth
	acsc
	acsch
	asec
	asech
	atanh
	cot
	coth
	csc
	csch
	sec
	sech
	tan
	tanh

cannot be computed for all arguments because that would mean dividing
by zero or taking logarithm of zero. These situations cause fatal
runtime errors looking like this

	cot(0): Division by zero.
	(Because in the definition of cot(0), the divisor sin(0) is 0)
	Died at ...

or

	atanh(-1): Logarithm of zero.
	Died at...

For the C<csc>, C<cot>, C<asec>, C<acsc>, C<acot>, C<csch>, C<coth>,
C<asech>, C<acsch>, the argument cannot be C<0> (zero).  For the
C<atanh>, C<acoth>, the argument cannot be C<1> (one).  For the
C<atanh>, C<acoth>, the argument cannot be C<-1> (minus one).  For the
C<tan>, C<sec>, C<tanh>, C<sech>, the argument cannot be I<pi/2 + k *
pi>, where I<k> is any integer.

=head2 SIMPLE (REAL) ARGUMENTS, COMPLEX RESULTS

Please note that some of the trigonometric functions can break out
from the B<real axis> into the B<complex plane>. For example
C<asin(2)> has no definition for plain real numbers but it has
definition for complex numbers.

In Perl terms this means that supplying the usual Perl numbers (also
known as scalars, please see L<perldata>) as input for the
trigonometric functions might produce as output results that no more
are simple real numbers: instead they are complex numbers.

The C<Math::Trig> handles this by using the C<Math::Complex> package
which knows how to handle complex numbers, please see L<Math::Complex>
for more information. In practice you need not to worry about getting
complex numbers as results because the C<Math::Complex> takes care of
details like for example how to display complex numbers. For example:

	print asin(2), "\n";

should produce something like this (take or leave few last decimals):

	1.5707963267949-1.31695789692482i

That is, a complex number with the real part of approximately C<1.571>
and the imaginary part of approximately C<-1.317>.

=head1 PLANE ANGLE CONVERSIONS

(Plane, 2-dimensional) angles may be converted with the following functions.

	$radians  = deg2rad($degrees);
	$radians  = grad2rad($gradians);

	$degrees  = rad2deg($radians);
	$degrees  = grad2deg($gradians);

	$gradians = deg2grad($degrees);
	$gradians = rad2grad($radians);

The full circle is 2 I<pi> radians or I<360> degrees or I<400> gradians.
The result is by default wrapped to be inside the [0, {2pi,360,400}[ circle.
If you don't want this, supply a true second argument:

	$zillions_of_radians  = deg2rad($zillions_of_degrees, 1);
	$negative_degrees     = rad2deg($negative_radians, 1);

You can also do the wrapping explicitly by rad2rad(), deg2deg(), and
grad2grad().

=head1 RADIAL COORDINATE CONVERSIONS

B<Radial coordinate systems> are the B<spherical> and the B<cylindrical>
systems, explained shortly in more detail.

You can import radial coordinate conversion functions by using the
C<:radial> tag:

    use Math::Trig ':radial';

    ($rho, $theta, $z)     = cartesian_to_cylindrical($x, $y, $z);
    ($rho, $theta, $phi)   = cartesian_to_spherical($x, $y, $z);
    ($x, $y, $z)           = cylindrical_to_cartesian($rho, $theta, $z);
    ($rho_s, $theta, $phi) = cylindrical_to_spherical($rho_c, $theta, $z);
    ($x, $y, $z)           = spherical_to_cartesian($rho, $theta, $phi);
    ($rho_c, $theta, $z)   = spherical_to_cylindrical($rho_s, $theta, $phi);

B<All angles are in radians>.

=head2 COORDINATE SYSTEMS

B<Cartesian> coordinates are the usual rectangular I<(x, y,
z)>-coordinates.

Spherical coordinates, I<(rho, theta, pi)>, are three-dimensional
coordinates which define a point in three-dimensional space.  They are
based on a sphere surface.  The radius of the sphere is B<rho>, also
known as the I<radial> coordinate.  The angle in the I<xy>-plane
(around the I<z>-axis) is B<theta>, also known as the I<azimuthal>
coordinate.  The angle from the I<z>-axis is B<phi>, also known as the
I<polar> coordinate.  The `North Pole' is therefore I<0, 0, rho>, and
the `Bay of Guinea' (think of the missing big chunk of Africa) I<0,
pi/2, rho>.  In geographical terms I<phi> is latitude (northward
positive, southward negative) and I<theta> is longitude (eastward
positive, westward negative).

B<BEWARE>: some texts define I<theta> and I<phi> the other way round,
some texts define the I<phi> to start from the horizontal plane, some
texts use I<r> in place of I<rho>.

Cylindrical coordinates, I<(rho, theta, z)>, are three-dimensional
coordinates which define a point in three-dimensional space.  They are
based on a cylinder surface.  The radius of the cylinder is B<rho>,
also known as the I<radial> coordinate.  The angle in the I<xy>-plane
(around the I<z>-axis) is B<theta>, also known as the I<azimuthal>
coordinate.  The third coordinate is the I<z>, pointing up from the
B<theta>-plane.

=head2 3-D ANGLE CONVERSIONS

Conversions to and from spherical and cylindrical coordinates are
available.  Please notice that the conversions are not necessarily
reversible because of the equalities like I<pi> angles being equal to
I<-pi> angles.

=over 4

=item cartesian_to_cylindrical

        ($rho, $theta, $z) = cartesian_to_cylindrical($x, $y, $z);

=item cartesian_to_spherical

        ($rho, $theta, $phi) = cartesian_to_spherical($x, $y, $z);

=item cylindrical_to_cartesian

        ($x, $y, $z) = cylindrical_to_cartesian($rho, $theta, $z);

=item cylindrical_to_spherical

        ($rho_s, $theta, $phi) = cylindrical_to_spherical($rho_c, $theta, $z);

Notice that when C<$z> is not 0 C<$rho_s> is not equal to C<$rho_c>.

=item spherical_to_cartesian

        ($x, $y, $z) = spherical_to_cartesian($rho, $theta, $phi);

=item spherical_to_cylindrical

        ($rho_c, $theta, $z) = spherical_to_cylindrical($rho_s, $theta, $phi);

Notice that when C<$z> is not 0 C<$rho_c> is not equal to C<$rho_s>.

=back

=head1 GREAT CIRCLE DISTANCES

You can compute spherical distances, called B<great circle distances>,
by importing the C<great_circle_distance> function:

	use Math::Trig 'great_circle_distance'

  $distance = great_circle_distance($theta0, $phi0, $theta1, $phi1, [, $rho]);

The I<great circle distance> is the shortest distance between two
points on a sphere.  The distance is in C<$rho> units.  The C<$rho> is
optional, it defaults to 1 (the unit sphere), therefore the distance
defaults to radians.

If you think geographically the I<theta> are longitudes: zero at the
Greenwhich meridian, eastward positive, westward negative--and the
I<phi> are latitudes: zero at the North Pole, northward positive,
southward negative.  B<NOTE>: this formula thinks in mathematics, not
geographically: the I<phi> zero is at the North Pole, not at the
Equator on the west coast of Africa (Bay of Guinea).  You need to
subtract your geographical coordinates from I<pi/2> (also known as 90
degrees).

  $distance = great_circle_distance($lon0, pi/2 - $lat0,
                                    $lon1, pi/2 - $lat1, $rho);

=head1 EXAMPLES

To calculate the distance between London (51.3N 0.5W) and Tokyo (35.7N
139.8E) in kilometers:

        use Math::Trig qw(great_circle_distance deg2rad);

        # Notice the 90 - latitude: phi zero is at the North Pole.
	@L = (deg2rad(-0.5), deg2rad(90 - 51.3));
        @T = (deg2rad(139.8),deg2rad(90 - 35.7));

        $km = great_circle_distance(@L, @T, 6378);

The answer may be off by few percentages because of the irregular
(slightly aspherical) form of the Earth.  The used formula

	lat0 = 90 degrees - phi0
	lat1 = 90 degrees - phi1
	d = R * arccos(cos(lat0) * cos(lat1) * cos(lon1 - lon01) +
                       sin(lat0) * sin(lat1))

is also somewhat unreliable for small distances (for locations
separated less than about five degrees) because it uses arc cosine
which is rather ill-conditioned for values close to zero.

=head1 BUGS

Saying C<use Math::Trig;> exports many mathematical routines in the
caller environment and even overrides some (C<sin>, C<cos>).  This is
construed as a feature by the Authors, actually... ;-)

The code is not optimized for speed, especially because we use
C<Math::Complex> and thus go quite near complex numbers while doing
the computations even when the arguments are not. This, however,
cannot be completely avoided if we want things like C<asin(2)> to give
an answer instead of giving a fatal runtime error.

=head1 AUTHORS

Jarkko Hietaniemi <F<jhi@iki.fi>> and 
Raphael Manfredi <F<Raphael_Manfredi@pobox.com>>.

=cut

# eof
