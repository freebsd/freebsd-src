package constant;

use strict;
use 5.005_64;
use warnings::register;

our($VERSION, %declared);
$VERSION = '1.02';

#=======================================================================

# Some names are evil choices.
my %keywords = map +($_, 1), qw{ BEGIN INIT CHECK END DESTROY AUTOLOAD };

my %forced_into_main = map +($_, 1),
    qw{ STDIN STDOUT STDERR ARGV ARGVOUT ENV INC SIG };

my %forbidden = (%keywords, %forced_into_main);

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
    return unless @_;			# Ignore 'use constant;'
    my $name = shift;
    unless (defined $name) {
        require Carp;
	Carp::croak("Can't use undef as constant name");
    }
    my $pkg = caller;

    # Normal constant name
    if ($name =~ /^_?[^\W_0-9]\w*\z/ and !$forbidden{$name}) {
        # Everything is okay

    # Name forced into main, but we're not in main. Fatal.
    } elsif ($forced_into_main{$name} and $pkg ne 'main') {
	require Carp;
	Carp::croak("Constant name '$name' is forced into main::");

    # Starts with double underscore. Fatal.
    } elsif ($name =~ /^__/) {
	require Carp;
	Carp::croak("Constant name '$name' begins with '__'");

    # Maybe the name is tolerable
    } elsif ($name =~ /^[A-Za-z_]\w*\z/) {
	# Then we'll warn only if you've asked for warnings
	if (warnings::enabled()) {
	    if ($keywords{$name}) {
		warnings::warn("Constant name '$name' is a Perl keyword");
	    } elsif ($forced_into_main{$name}) {
		warnings::warn("Constant name '$name' is " .
		    "forced into package main::");
	    } else {
		# Catch-all - what did I miss? If you get this error,
		# please let me know what your constant's name was.
		# Write to <rootbeer@redcat.com>. Thanks!
		warnings::warn("Constant name '$name' has unknown problems");
	    }
	}

    # Looks like a boolean
    # 		use constant FRED == fred;
    } elsif ($name =~ /^[01]?\z/) {
        require Carp;
	if (@_) {
	    Carp::croak("Constant name '$name' is invalid");
	} else {
	    Carp::croak("Constant name looks like boolean value");
	}

    } else {
	# Must have bad characters
        require Carp;
	Carp::croak("Constant name '$name' has invalid characters");
    }

    {
	no strict 'refs';
	my $full_name = "${pkg}::$name";
	$declared{$full_name}++;
	if (@_ == 1) {
	    my $scalar = $_[0];
	    *$full_name = sub () { $scalar };
	} elsif (@_) {
	    my @list = @_;
	    *$full_name = sub () { @list };
	} else {
	    *$full_name = sub () { };
	}
    }

}

1;

__END__

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

    # references can be constants
    use constant CHASH		=> { foo => 42 };
    use constant CARRAY		=> [ 1,2,3,4 ];
    use constant CPSEUDOHASH	=> [ { foo => 1}, 42 ];
    use constant CCODE		=> sub { "bite $_[0]\n" };

    print CHASH->{foo};
    print CARRAY->[$i];
    print CPSEUDOHASH->{foo};
    print CCODE->("me");
    print CHASH->[10];			# compile-time error

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
subroutine names. Constant names must begin with a letter or
underscore. Names beginning with a double underscore are reserved. Some
poor choices for names will generate warnings, if warnings are enabled at
compile time.

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

Dereferencing constant references incorrectly (such as using an array
subscript on a constant hash reference, or vice versa) will be trapped at
compile time.

In the rare case in which you need to discover at run time whether a
particular constant has been declared via this module, you may use
this function to examine the hash C<%constant::declared>. If the given
constant name does not include a package name, the current package is
used.

    sub declared ($) {
	use constant 1.01;		# don't omit this!
	my $name = shift;
	$name =~ s/^::/main::/;
	my $pkg = caller;
	my $full_name = $name =~ /::/ ? $name : "${pkg}::$name";
	$constant::declared{$full_name};
    }

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
name as a constant in the same package. This is probably a Good Thing.

A constant with a name in the list C<STDIN STDOUT STDERR ARGV ARGVOUT
ENV INC SIG> is not allowed anywhere but in package C<main::>, for
technical reasons. 

Even though a reference may be declared as a constant, the reference may
point to data which may be changed, as this code shows.

    use constant CARRAY		=> [ 1,2,3,4 ];
    print CARRAY->[1];
    CARRAY->[1] = " be changed";
    print CARRAY->[1];

Unlike constants in some languages, these cannot be overridden
on the command line or via environment variables.

You can get into trouble if you use constants in a context which
automatically quotes barewords (as is true for any subroutine call).
For example, you can't say C<$hash{CONSTANT}> because C<CONSTANT> will
be interpreted as a string.  Use C<$hash{CONSTANT()}> or
C<$hash{+CONSTANT}> to prevent the bareword quoting mechanism from
kicking in.  Similarly, since the C<=E<gt>> operator quotes a bareword
immediately to its left, you have to say C<CONSTANT() =E<gt> 'value'>
(or simply use a comma in place of the big arrow) instead of
C<CONSTANT =E<gt> 'value'>.

=head1 AUTHOR

Tom Phoenix, E<lt>F<rootbeer@redcat.com>E<gt>, with help from
many other folks.

=head1 COPYRIGHT

Copyright (C) 1997, 1999 Tom Phoenix

This module is free software; you can redistribute it or modify it
under the same terms as Perl itself.

=cut
