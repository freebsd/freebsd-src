package ODBM_File;

use strict;

require Tie::Hash;
use XSLoader ();

our @ISA = qw(Tie::Hash);
our $VERSION = "1.02";

XSLoader::load 'ODBM_File', $VERSION;

1;

__END__

=head1 NAME

ODBM_File - Tied access to odbm files

=head1 SYNOPSIS

 use ODBM_File;

 tie(%h, 'ODBM_File', 'Op.dbmx', O_RDWR|O_CREAT, 0640);

 untie %h;

=head1 DESCRIPTION

See L<perlfunc/tie>, L<perldbmfilter>

=cut
