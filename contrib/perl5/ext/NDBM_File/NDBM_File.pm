package NDBM_File;

BEGIN {
    if ($] >= 5.002) {
	use strict;
    }
}

require Tie::Hash;
use XSLoader ();

our @ISA = qw(Tie::Hash);
our $VERSION = "1.03";

XSLoader::load 'NDBM_File', $VERSION;

1;

__END__

=head1 NAME

NDBM_File - Tied access to ndbm files

=head1 SYNOPSIS

 use NDBM_File;
 use Fcntl;       # for O_ constants

 tie(%h, 'NDBM_File', 'Op.dbmx', O_RDWR|O_CREAT, 0640);

 untie %h;

=head1 DESCRIPTION

See L<perlfunc/tie>, L<perldbmfilter>

=cut
