package attrs;
require DynaLoader;
use vars '@ISA';
@ISA = 'DynaLoader';

use vars qw($VERSION);
$VERSION = "1.0";

=head1 NAME

attrs - set/get attributes of a subroutine

=head1 SYNOPSIS

    sub foo {
        use attrs qw(locked method);
        ...
    }

    @a = attrs::get(\&foo);

=head1 DESCRIPTION

This module lets you set and get attributes for subroutines.
Setting attributes takes place at compile time; trying to set
invalid attribute names causes a compile-time error. Calling
C<attr::get> on a subroutine reference or name returns its list
of attribute names. Notice that C<attr::get> is not exported.
Valid attributes are as follows.

=over

=item method

Indicates that the invoking subroutine is a method.

=item locked

Setting this attribute is only meaningful when the subroutine or
method is to be called by multiple threads. When set on a method
subroutine (i.e. one marked with the B<method> attribute above),
perl ensures that any invocation of it implicitly locks its first
argument before execution. When set on a non-method subroutine,
perl ensures that a lock is taken on the subroutine itself before
execution. The semantics of the lock are exactly those of one
explicitly taken with the C<lock> operator immediately after the
subroutine is entered.

=back

=cut

bootstrap attrs $VERSION;

1;
