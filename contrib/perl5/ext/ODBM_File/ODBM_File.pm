package ODBM_File;

use strict;
use vars qw($VERSION @ISA);

require Tie::Hash;
require DynaLoader;

@ISA = qw(Tie::Hash DynaLoader);

$VERSION = "1.00";

bootstrap ODBM_File $VERSION;

1;

__END__

=head1 NAME

ODBM_File - Tied access to odbm files

=head1 SYNOPSIS

 use ODBM_File;

 tie(%h, 'ODBM_File', 'Op.dbmx', O_RDWR|O_CREAT, 0640);

 untie %h;

=head1 DESCRIPTION

See L<perlfunc/tie>

=cut
