package strict;

=head1 NAME

strict - Perl pragma to restrict unsafe constructs

=head1 SYNOPSIS

    use strict;

    use strict "vars";
    use strict "refs";
    use strict "subs";

    use strict;
    no strict "vars";

=head1 DESCRIPTION

If no import list is supplied, all possible restrictions are assumed.
(This is the safest mode to operate in, but is sometimes too strict for
casual programming.)  Currently, there are three possible things to be
strict about:  "subs", "vars", and "refs".

=over 6

=item C<strict refs>

This generates a runtime error if you 
use symbolic references (see L<perlref>).

    use strict 'refs';
    $ref = \$foo;
    print $$ref;	# ok
    $ref = "foo";
    print $$ref;	# runtime error; normally ok

=item C<strict vars>

This generates a compile-time error if you access a variable that wasn't
declared via C<use vars>,
localized via C<my()> or wasn't fully qualified.  Because this is to avoid
variable suicide problems and subtle dynamic scoping issues, a merely
local() variable isn't good enough.  See L<perlfunc/my> and
L<perlfunc/local>.

    use strict 'vars';
    $X::foo = 1;	 # ok, fully qualified
    my $foo = 10;	 # ok, my() var
    local $foo = 9;	 # blows up

    package Cinna;
    use vars qw/ $bar /;	# Declares $bar in current package
    $bar = 'HgS';		# ok, global declared via pragma

The local() generated a compile-time error because you just touched a global
name without fully qualifying it.

=item C<strict subs>

This disables the poetry optimization, generating a compile-time error if
you try to use a bareword identifier that's not a subroutine, unless it
appears in curly braces or on the left hand side of the "=E<gt>" symbol.


    use strict 'subs';
    $SIG{PIPE} = Plumber;   	# blows up
    $SIG{PIPE} = "Plumber"; 	# just fine: bareword in curlies always ok
    $SIG{PIPE} = \&Plumber; 	# preferred form



=back

See L<perlmodlib/Pragmatic Modules>.


=cut

$strict::VERSION = "1.01";

my %bitmask = (
refs => 0x00000002,
subs => 0x00000200,
vars => 0x00000400
);

sub bits {
    my $bits = 0;
    foreach my $s (@_){ $bits |= $bitmask{$s} || 0; };
    $bits;
}

sub import {
    shift;
    $^H |= bits(@_ ? @_ : qw(refs subs vars));
}

sub unimport {
    shift;
    $^H &= ~ bits(@_ ? @_ : qw(refs subs vars));
}

1;
