package CGI::Switch;
use Carp;
use strict;
use vars qw($VERSION @Pref);
$VERSION = '0.06';
@Pref = qw(CGI::Apache CGI); #default

sub import {
    my($self,@arg) = @_;
    @Pref = @arg if @arg;
}

sub new {
    shift;
    my($file,$pack);
    for $pack (@Pref) {
	($file = $pack) =~ s|::|/|g;
	eval { require "$file.pm"; };
	if ($@) {
#XXX	    warn $@;
	    next;
	} else {
#XXX	    warn "Going to try $pack\->new\n";
	    my $obj;
	    eval {$obj = $pack->new(@_)};
	    if ($@) {
#XXX		warn $@;
	    } else {
		return $obj;
	    }
	}
    }
    Carp::croak "Couldn't load+construct any of @Pref\n";
}

1;
__END__

=head1 NAME

CGI::Switch - Try more than one constructors and return the first object available

=head1 SYNOPSIS

 
 use CGISwitch;

  -or-

 use CGI::Switch This, That, CGI::XA, Foo, Bar, CGI;

 my $q = new CGI::Switch;

=head1 DESCRIPTION

Per default the new() method tries to call new() in the three packages
Apache::CGI, CGI::XA, and CGI. It returns the first CGI object it
succeeds with.

The import method allows you to set up the default order of the
modules to be tested.

=head1 SEE ALSO

perl(1), Apache(3), CGI(3), CGI::XA(3)

=head1 AUTHOR

Andreas KE<ouml>nig E<lt>a.koenig@mind.deE<gt>

=cut
