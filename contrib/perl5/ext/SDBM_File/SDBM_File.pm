package SDBM_File;

use strict;

require Tie::Hash;
use XSLoader ();

our @ISA = qw(Tie::Hash);
our $VERSION = "1.02" ;

XSLoader::load 'SDBM_File', $VERSION;

1;

__END__

=head1 NAME

SDBM_File - Tied access to sdbm files

=head1 SYNOPSIS

 use SDBM_File;

 tie(%h, 'SDBM_File', 'Op.dbmx', O_RDWR|O_CREAT, 0640);

 untie %h;

=head1 DESCRIPTION

See L<perlfunc/tie>, L<perldbmfilter>

=cut
