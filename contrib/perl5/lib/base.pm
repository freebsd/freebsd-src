=head1 NAME

base - Establish IS-A relationship with base class at compile time

=head1 SYNOPSIS

    package Baz;
    use base qw(Foo Bar);

=head1 DESCRIPTION

Roughly similar in effect to

    BEGIN {
	require Foo;
	require Bar;
	push @ISA, qw(Foo Bar);
    }

Will also initialize the %FIELDS hash if one of the base classes has
it.  Multiple inheritance of %FIELDS is not supported.  The 'base'
pragma will croak if multiple base classes has a %FIELDS hash.  See
L<fields> for a description of this feature.

When strict 'vars' is in scope I<base> also let you assign to @ISA
without having to declare @ISA with the 'vars' pragma first.

This module was introduced with Perl 5.004_04.

=head1 SEE ALSO

L<fields>

=cut

package base;

sub import {
    my $class = shift;
    my $fields_base;

    foreach my $base (@_) {
	unless (defined %{"$base\::"}) {
	    eval "require $base";
	    # Only ignore "Can't locate" errors from our eval require.
	    # Other fatal errors (syntax etc) must be reported.
	    die if $@ && $@ !~ /^Can't locate .*? at \(eval /;
	    unless (defined %{"$base\::"}) {
		require Carp;
		Carp::croak("Base class package \"$base\" is empty.\n",
			    "\t(Perhaps you need to 'use' the module ",
			    "which defines that package first.)");
	    }
	}

	# A simple test like (defined %{"$base\::FIELDS"}) will
	# sometimes produce typo warnings because it would create
	# the hash if it was not present before.
	my $fglob;
	if ($fglob = ${"$base\::"}{"FIELDS"} and *$fglob{HASH}) {
	    if ($fields_base) {
		require Carp;
		Carp::croak("Can't multiply inherit %FIELDS");
	    } else {
		$fields_base = $base;
	    }
	}
    }
    my $pkg = caller(0);
    push @{"$pkg\::ISA"}, @_;
    if ($fields_base) {
	require fields;
	fields::inherit($pkg, $fields_base);
    }
}

1;
