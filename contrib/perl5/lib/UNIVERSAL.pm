package UNIVERSAL;

# UNIVERSAL should not contain any extra subs/methods beyond those
# that it exists to define. The use of Exporter below is a historical
# accident that should be fixed sometime.
require Exporter;
*import = \&Exporter::import;
@EXPORT_OK = qw(isa can);

1;
__END__

=head1 NAME

UNIVERSAL - base class for ALL classes (blessed references)

=head1 SYNOPSIS

    $io = $fd->isa("IO::Handle");
    $sub = $obj->can('print');

    $yes = UNIVERSAL::isa($ref, "HASH");

=head1 DESCRIPTION

C<UNIVERSAL> is the base class which all bless references will inherit from,
see L<perlobj>

C<UNIVERSAL> provides the following methods

=over 4

=item isa ( TYPE )

C<isa> returns I<true> if C<REF> is blessed into package C<TYPE>
or inherits from package C<TYPE>.

C<isa> can be called as either a static or object method call.

=item can ( METHOD )

C<can> checks if the object has a method called C<METHOD>. If it does
then a reference to the sub is returned. If it does not then I<undef>
is returned.

C<can> can be called as either a static or object method call.

=item VERSION ( [ REQUIRE ] )

C<VERSION> will return the value of the variable C<$VERSION> in the
package the object is blessed into. If C<REQUIRE> is given then
it will do a comparison and die if the package version is not
greater than or equal to C<REQUIRE>.

C<VERSION> can be called as either a static or object method call.

=back

The C<isa> and C<can> methods can also be called as subroutines

=over 4

=item UNIVERSAL::isa ( VAL, TYPE )

C<isa> returns I<true> if one of the following statements is true.

=over 8

=item *

C<VAL> is a reference blessed into either package C<TYPE> or a package
which inherits from package C<TYPE>.

=item *

C<VAL> is a reference to a C<TYPE> of Perl variable (e.g. 'HASH').

=item *

C<VAL> is the name of a package that inherits from (or is itself)
package C<TYPE>.

=back

=item UNIVERSAL::can ( VAL, METHOD )

If C<VAL> is a blessed reference which has a method called C<METHOD>,
C<can> returns a reference to the subroutine.   If C<VAL> is not
a blessed reference, or if it does not have a method C<METHOD>,
I<undef> is returned.

=back

These subroutines should I<not> be imported via S<C<use UNIVERSAL qw(...)>>.
If you want simple local access to them you can do

  *isa = \&UNIVERSAL::isa;

to import isa into your package.

=cut
