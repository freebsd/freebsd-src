package Tie::Hash;

=head1 NAME

Tie::Hash, Tie::StdHash - base class definitions for tied hashes

=head1 SYNOPSIS

    package NewHash;
    require Tie::Hash;
    
    @ISA = (Tie::Hash);
    
    sub DELETE { ... }		# Provides needed method
    sub CLEAR { ... }		# Overrides inherited method
    
    
    package NewStdHash;
    require Tie::Hash;
    
    @ISA = (Tie::StdHash);
    
    # All methods provided by default, define only those needing overrides
    sub DELETE { ... }
    
    
    package main;
    
    tie %new_hash, 'NewHash';
    tie %new_std_hash, 'NewStdHash';

=head1 DESCRIPTION

This module provides some skeletal methods for hash-tying classes. See
L<perltie> for a list of the functions required in order to tie a hash
to a package. The basic B<Tie::Hash> package provides a C<new> method, as well
as methods C<TIEHASH>, C<EXISTS> and C<CLEAR>. The B<Tie::StdHash> package
provides most methods required for hashes in L<perltie>. It inherits from
B<Tie::Hash>, and causes tied hashes to behave exactly like standard hashes,
allowing for selective overloading of methods. The C<new> method is provided
as grandfathering in the case a class forgets to include a C<TIEHASH> method.

For developers wishing to write their own tied hashes, the required methods
are briefly defined below. See the L<perltie> section for more detailed
descriptive, as well as example code:

=over

=item TIEHASH classname, LIST

The method invoked by the command C<tie %hash, classname>. Associates a new
hash instance with the specified class. C<LIST> would represent additional
arguments (along the lines of L<AnyDBM_File> and compatriots) needed to
complete the association.

=item STORE this, key, value

Store datum I<value> into I<key> for the tied hash I<this>.

=item FETCH this, key

Retrieve the datum in I<key> for the tied hash I<this>.

=item FIRSTKEY this

Return the (key, value) pair for the first key in the hash.

=item NEXTKEY this, lastkey

Return the next key for the hash.

=item EXISTS this, key

Verify that I<key> exists with the tied hash I<this>.

The B<Tie::Hash> implementation is a stub that simply croaks.

=item DELETE this, key

Delete the key I<key> from the tied hash I<this>.

=item CLEAR this

Clear all values from the tied hash I<this>.

=back

=head1 CAVEATS

The L<perltie> documentation includes a method called C<DESTROY> as
a necessary method for tied hashes. Neither B<Tie::Hash> nor B<Tie::StdHash>
define a default for this method. This is a standard for class packages,
but may be omitted in favor of a simple default.

=head1 MORE INFORMATION

The packages relating to various DBM-related implementations (F<DB_File>,
F<NDBM_File>, etc.) show examples of general tied hashes, as does the
L<Config> module. While these do not utilize B<Tie::Hash>, they serve as
good working examples.

=cut

use Carp;
use warnings::register;

sub new {
    my $pkg = shift;
    $pkg->TIEHASH(@_);
}

# Grandfather "new"

sub TIEHASH {
    my $pkg = shift;
    if (defined &{"${pkg}::new"}) {
	warnings::warnif("WARNING: calling ${pkg}->new since ${pkg}->TIEHASH is missing");
	$pkg->new(@_);
    }
    else {
	croak "$pkg doesn't define a TIEHASH method";
    }
}

sub EXISTS {
    my $pkg = ref $_[0];
    croak "$pkg doesn't define an EXISTS method";
}

sub CLEAR {
    my $self = shift;
    my $key = $self->FIRSTKEY(@_);
    my @keys;

    while (defined $key) {
	push @keys, $key;
	$key = $self->NEXTKEY(@_, $key);
    }
    foreach $key (@keys) {
	$self->DELETE(@_, $key);
    }
}

# The Tie::StdHash package implements standard perl hash behaviour.
# It exists to act as a base class for classes which only wish to
# alter some parts of their behaviour.

package Tie::StdHash;
@ISA = qw(Tie::Hash);

sub TIEHASH  { bless {}, $_[0] }
sub STORE    { $_[0]->{$_[1]} = $_[2] }
sub FETCH    { $_[0]->{$_[1]} }
sub FIRSTKEY { my $a = scalar keys %{$_[0]}; each %{$_[0]} }
sub NEXTKEY  { each %{$_[0]} }
sub EXISTS   { exists $_[0]->{$_[1]} }
sub DELETE   { delete $_[0]->{$_[1]} }
sub CLEAR    { %{$_[0]} = () }

1;
