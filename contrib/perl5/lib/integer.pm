package integer;

=head1 NAME

integer - Perl pragma to compute arithmetic in integer instead of double

=head1 SYNOPSIS

    use integer;
    $x = 10/3;
    # $x is now 3, not 3.33333333333333333

=head1 DESCRIPTION

This tells the compiler to use integer operations
from here to the end of the enclosing BLOCK.  On many machines, 
this doesn't matter a great deal for most computations, but on those 
without floating point hardware, it can make a big difference.

Note that this affects the operations, not the numbers.  If you run this
code

    use integer;
    $x = 1.5;
    $y = $x + 1;
    $z = -1.5;

you'll be left with C<$x == 1.5>, C<$y == 2> and C<$z == -1>.  The $z
case happens because unary C<-> counts as an operation.

See L<perlmod/Pragmatic Modules>.

=cut

sub import {
    $^H |= 1;
}

sub unimport {
    $^H &= ~1;
}

1;
