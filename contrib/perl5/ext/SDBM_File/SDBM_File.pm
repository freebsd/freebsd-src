package SDBM_File;

use strict;
use vars qw($VERSION @ISA);

require Tie::Hash;
require DynaLoader;

@ISA = qw(Tie::Hash DynaLoader);

$VERSION = "1.00" ;

bootstrap SDBM_File $VERSION;

1;

__END__

=head1 NAME

SDBM_File - Tied access to sdbm files

=head1 SYNOPSIS

 use SDBM_File;

 tie(%h, 'SDBM_File', 'Op.dbmx', O_RDWR|O_CREAT, 0640);

 untie %h;

=head1 DESCRIPTION

See L<perlfunc/tie>

=cut
