package NDBM_File;

BEGIN {
    if ($] >= 5.002) {
	use strict;
    }
}
use vars qw($VERSION @ISA); 

require Tie::Hash;
require DynaLoader;

@ISA = qw(Tie::Hash DynaLoader);

$VERSION = "1.01";

bootstrap NDBM_File $VERSION;

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

See L<perlfunc/tie>

=cut
