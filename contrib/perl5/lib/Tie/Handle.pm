package Tie::Handle;

use 5.005_64;
our $VERSION = '4.0';

=head1 NAME

Tie::Handle, Tie::StdHandle  - base class definitions for tied handles

=head1 SYNOPSIS

    package NewHandle;
    require Tie::Handle;

    @ISA = (Tie::Handle);

    sub READ { ... }		# Provide a needed method
    sub TIEHANDLE { ... }	# Overrides inherited method


    package main;

    tie *FH, 'NewHandle';

=head1 DESCRIPTION

This module provides some skeletal methods for handle-tying classes. See
L<perltie> for a list of the functions required in tying a handle to a package.
The basic B<Tie::Handle> package provides a C<new> method, as well as methods
C<TIEHANDLE>, C<PRINT>, C<PRINTF> and C<GETC>. 

For developers wishing to write their own tied-handle classes, the methods
are summarized below. The L<perltie> section not only documents these, but
has sample code as well:

=over

=item TIEHANDLE classname, LIST

The method invoked by the command C<tie *glob, classname>. Associates a new
glob instance with the specified class. C<LIST> would represent additional
arguments (along the lines of L<AnyDBM_File> and compatriots) needed to
complete the association.

=item WRITE this, scalar, length, offset

Write I<length> bytes of data from I<scalar> starting at I<offset>.

=item PRINT this, LIST

Print the values in I<LIST>

=item PRINTF this, format, LIST

Print the values in I<LIST> using I<format>

=item READ this, scalar, length, offset

Read I<length> bytes of data into I<scalar> starting at I<offset>.

=item READLINE this

Read a single line

=item GETC this

Get a single character

=item CLOSE this

Close the handle

=item OPEN this, filename

(Re-)open the handle

=item BINMODE this

Specify content is binary

=item EOF this

Test for end of file.

=item TELL this

Return position in the file.

=item SEEK this, offset, whence

Position the file.

Test for end of file.

=item DESTROY this

Free the storage associated with the tied handle referenced by I<this>.
This is rarely needed, as Perl manages its memory quite well. But the
option exists, should a class wish to perform specific actions upon the
destruction of an instance.

=back

=head1 MORE INFORMATION

The L<perltie> section contains an example of tying handles.

=head1 COMPATIBILITY

This version of Tie::Handle is neither related to nor compatible with
the Tie::Handle (3.0) module available on CPAN. It was due to an
accident that two modules with the same name appeared. The namespace
clash has been cleared in favor of this module that comes with the
perl core in September 2000 and accordingly the version number has
been bumped up to 4.0.

=cut

use Carp;
use warnings::register;

sub new {
    my $pkg = shift;
    $pkg->TIEHANDLE(@_);
}

# "Grandfather" the new, a la Tie::Hash

sub TIEHANDLE {
    my $pkg = shift;
    if (defined &{"{$pkg}::new"}) {
	warnings::warnif("WARNING: calling ${pkg}->new since ${pkg}->TIEHANDLE is missing");
	$pkg->new(@_);
    }
    else {
	croak "$pkg doesn't define a TIEHANDLE method";
    }
}

sub PRINT {
    my $self = shift;
    if($self->can('WRITE') != \&WRITE) {
	my $buf = join(defined $, ? $, : "",@_);
	$buf .= $\ if defined $\;
	$self->WRITE($buf,length($buf),0);
    }
    else {
	croak ref($self)," doesn't define a PRINT method";
    }
}

sub PRINTF {
    my $self = shift;
    
    if($self->can('WRITE') != \&WRITE) {
	my $buf = sprintf(shift,@_);
	$self->WRITE($buf,length($buf),0);
    }
    else {
	croak ref($self)," doesn't define a PRINTF method";
    }
}

sub READLINE {
    my $pkg = ref $_[0];
    croak "$pkg doesn't define a READLINE method";
}

sub GETC {
    my $self = shift;
    
    if($self->can('READ') != \&READ) {
	my $buf;
	$self->READ($buf,1);
	return $buf;
    }
    else {
	croak ref($self)," doesn't define a GETC method";
    }
}

sub READ {
    my $pkg = ref $_[0];
    croak "$pkg doesn't define a READ method";
}

sub WRITE {
    my $pkg = ref $_[0];
    croak "$pkg doesn't define a WRITE method";
}

sub CLOSE {
    my $pkg = ref $_[0];
    croak "$pkg doesn't define a CLOSE method";
}

package Tie::StdHandle; 
our @ISA = 'Tie::Handle';
use Carp;

sub TIEHANDLE 
{
 my $class = shift;
 my $fh    = do { \local *HANDLE};
 bless $fh,$class;
 $fh->OPEN(@_) if (@_);
 return $fh;
}

sub EOF     { eof($_[0]) }
sub TELL    { tell($_[0]) }
sub FILENO  { fileno($_[0]) }
sub SEEK    { seek($_[0],$_[1],$_[2]) }
sub CLOSE   { close($_[0]) }
sub BINMODE { binmode($_[0]) }

sub OPEN
{
 $_[0]->CLOSE if defined($_[0]->FILENO);
 @_ == 2 ? open($_[0], $_[1]) : open($_[0], $_[1], $_[2]);
}

sub READ     { read($_[0],$_[1],$_[2]) }
sub READLINE { my $fh = $_[0]; <$fh> }
sub GETC     { getc($_[0]) }

sub WRITE
{
 my $fh = $_[0];
 print $fh substr($_[1],0,$_[2])
}


1;
