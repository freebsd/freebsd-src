package constant;

$VERSION = '1.00';

=head1 NAME

constant - Perl pragma to declare constants

=head1 SYNOPSIS

    use constant BUFFER_SIZE	=> 4096;
    use constant ONE_YEAR	=> 365.2425 * 24 * 60 * 60;
    use constant PI		=> 4 * atan2 1, 1;
    use constant DEBUGGING	=> 0;
    use constant ORACLE		=> 'oracle@cs.indiana.edu';
    use constant USERNAME	=> scalar getpwuid($<);
    use constant USERINFO	=> getpwuid($<);

    sub deg2rad { PI * $_[0] / 180 }

    print "This line does nothing"		unless DEBUGGING;

    # references can be declared constant
    use constant CHASH		=> { foo => 42 };
    use constant CARRAY		=> [ 1,2,3,4 ];
    use constant CPSEUDOHASH	=> [ { foo => 1}, 42 ];
    use constant CCODE		=> sub { "bite $_[0]\n" };

    print CHASH->{foo};
    print CARRAY->[$i];
    print CPSEUDOHASH->{foo};
    print CCODE->("me");
    print CHASH->[10];				# compile-time error

=head1 DESCRIPTION

This will declare a symbol to be a constant with the given scalar
or list value.

When you declare a constant such as C<PI> using the method shown
above, each machine your script runs upon can have as many digits
of accuracy as it can use. Also, your program will be easier to
read, more likely to be maintained (and maintained correctly), and
far less likely to send a space probe to the wrong planet because
nobody noticed the one equation in which you wrote C<3.14195>.

=head1 NOTES

The value or values are evaluated in a list context. You may override
this with C<scalar> as shown above.

These constants do not directly interpolate into double-quotish
strings, although you may do so indirectly. (See L<perlref> for
details about how this works.)

    print "The value of PI is @{[ PI ]}.\n";

List constants are returned as lists, not as arrays.

    $homedir = USERINFO[7];		# WRONG
    $homedir = (USERINFO)[7];		# Right

The use of all caps for constant names is merely a convention,
although it is recommended in order to make constants stand out
and to help avoid collisions with other barewords, keywords, and
subroutine names. Constant names must begin with a letter.

Constant symbols are package scoped (rather than block scoped, as
C<use strict> is). That is, you can refer to a constant from package
Other as C<Other::CONST>.

As with all C<use> directives, defining a constant happens at
compile time. Thus, it's probably not correct to put a constant
declaration inside of a conditional statement (like C<if ($foo)
{ use constant ... }>).

Omitting the value for a symbol gives it the value of C<undef> in
a scalar context or the empty list, C<()>, in a list context. This
isn't so nice as it may sound, though, because in this case you
must either quote the symbol name, or use a big arrow, (C<=E<gt>>),
with nothing to point to. It is probably best to declare these
explicitly.

    use constant UNICORNS	=> ();
    use constant LOGFILE	=> undef;

The result from evaluating a list constant in a scalar context is
not documented, and is B<not> guaranteed to be any particular value
in the future. In particular, you should not rely upon it being
the number of elements in the list, especially since it is not
B<necessarily> that value in the current implementation.

Magical values, tied values, and references can be made into
constants at compile time, allowing for way cool stuff like this.
(These error numbers aren't totally portable, alas.)

    use constant E2BIG => ($! = 7);
    print   E2BIG, "\n";	# something like "Arg list too long"
    print 0+E2BIG, "\n";	# "7"

Errors in dereferencing constant references are trapped at compile-time.

=head1 TECHNICAL NOTE

In the current implementation, scalar constants are actually
inlinable subroutines. As of version 5.004 of Perl, the appropriate
scalar constant is inserted directly in place of some subroutine
calls, thereby saving the overhead of a subroutine call. See
L<perlsub/"Constant Functions"> for details about how and when this
happens.

=head1 BUGS

In the current version of Perl, list constants are not inlined
and some symbols may be redefined without generating a warning.

It is not possible to have a subroutine or keyword with the same
name as a constant. This is probably a Good Thing.

Unlike constants in some languages, these cannot be overridden
on the command line or via environment variables.

You can get into trouble if you use constants in a context which
automatically quotes barewords (as is true for any subroutine call).
For example, you can't say C<$hash{CONSTANT}> because C<CONSTANT> will
be interpreted as a string.  Use C<$hash{CONSTANT()}> or
C<$hash{+CONSTANT}> to prevent the bareword quoting mechanism from
kicking in.  Similarly, since the C<=E<gt>> operator quotes a bareword
immediately to its left you have to say C<CONSTANT() =E<gt> 'value'>
instead of C<CONSTANT =E<gt> 'value'>.

=head1 AUTHOR

Tom Phoenix, E<lt>F<rootbeer@teleport.com>E<gt>, with help from
many other folks.

=head1 COPYRIGHT

Copyright (C) 1997, Tom Phoenix

This module is free software; you can redistribute it or modify it
under the same terms as Perl itself.

=cut

use strict;
use Carp;
use vars qw($VERSION);

#=======================================================================

# Some of this stuff didn't work in version 5.003, alas.
require 5.003_96;

#=======================================================================
# import() - import symbols into user's namespace
#
# What we actually do is define a function in the caller's namespace
# which returns the value. The function we create will normally
# be inlined as a constant, thereby avoiding further sub calling 
# overhead.
#=======================================================================
sub import {
    my $class = shift;
    my $name = shift or return;			# Ignore 'use constant;'
    croak qq{Can't define "$name" as constant} .
	    qq{ (name contains invalid characters or is empty)}
	unless $name =~ /^[^\W_0-9]\w*$/;

    my $pkg = caller;
    {
	no strict 'refs';
	if (@_ == 1) {
	    my $scalar = $_[0];
	    *{"${pkg}::$name"} = sub () { $scalar };
	} elsif (@_) {
	    my @list = @_;
	    *{"${pkg}::$name"} = sub () { @list };
	} else {
	    *{"${pkg}::$name"} = sub () { };
	}
    }

}

1;
