package CGI::Apache;
use Apache ();
use vars qw(@ISA $VERSION);
require CGI;
@ISA = qw(CGI);

$VERSION = (qw$Revision: 1.1 $)[1];
$CGI::DefaultClass = 'CGI::Apache';
$CGI::Apache::AutoloadClass = 'CGI';

sub import {
    my $self = shift;
    my ($callpack, $callfile, $callline) = caller;
    ${"${callpack}::AutoloadClass"} = 'CGI';
}

sub new {
    my($class) = shift;
    my($r) = Apache->request;
    %ENV = $r->cgi_env unless defined $ENV{GATEWAY_INTERFACE}; #PerlSetupEnv On 
    my $self = $class->SUPER::new(@_);
    $self->{'.req'} = $r;
    $self;
}

sub header {
    my ($self,@rest) = CGI::self_or_default(@_);
    my $r = $self->{'.req'};
    $r->basic_http_header;
    return CGI::header($self,@rest);
}		     

sub print {
    my($self,@rest) = CGI::self_or_default(@_);
    $self->{'.req'}->print(@rest);
}

sub read_from_client {
    my($self, $fh, $buff, $len, $offset) = @_;
    my $r = $self->{'.req'} || Apache->request;
    return $r->read($$buff, $len, $offset);
}

sub new_MultipartBuffer {
    my $self = shift;
    my $new = CGI::Apache::MultipartBuffer->new($self, @_); 
    $new->{'.req'} = $self->{'.req'} || Apache->request;
    return $new;
}

package CGI::Apache::MultipartBuffer;
use vars qw(@ISA);
@ISA = qw(MultipartBuffer);

$CGI::Apache::MultipartBuffer::AutoloadClass = 'MultipartBuffer';
*CGI::Apache::MultipartBuffer::read_from_client = 
    \&CGI::Apache::read_from_client;


1;

__END__

=head1 NAME

CGI::Apache - Make things work with CGI.pm against Perl-Apache API

=head1 SYNOPSIS

 require CGI::Apache;

 my $q = new Apache::CGI;

 $q->print($q->header);

 #do things just like you do with CGI.pm

=head1 DESCRIPTION

When using the Perl-Apache API, your applications are faster, but the
environment is different than CGI.
This module attempts to set-up that environment as best it can.

=head1 NOTE 1

This module used to be named Apache::CGI.  Sorry for the confusion.

=head1 NOTE 2

If you're going to inherit from this class, make sure to "use" it
after your package declaration rather than "require" it.  This is
because CGI.pm does a little magic during the import() step in order
to make autoloading work correctly.

=head1 SEE ALSO

perl(1), Apache(3), CGI(3)

=head1 AUTHOR

Doug MacEachern E<lt>dougm@osf.orgE<gt>, hacked over by Andreas KE<ouml>nig E<lt>a.koenig@mind.deE<gt>, modified by Lincoln Stein <lt>lstein@genome.wi.mit.edu<gt>

=cut
