# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42)
# <tobez@tobez.org> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Anton Berezin
# ----------------------------------------------------------------------------
#
# $FreeBSD$
#
package BSDPAN::Config;

use strict;
use BSDPAN;

sub bsdpan_no_override
{
	my $bsdpan_path = BSDPAN->path;
	my @ninc;
	for my $inc_component (@INC) {
		push @ninc, $inc_component
		    unless $inc_component eq $bsdpan_path;
	}
	@INC = (@ninc, $bsdpan_path);
}

BEGIN {
	if ($0 =~ m|/bin/perldoc$|) {
		bsdpan_no_override();

		# Also, add bsdpan_path/.. to @INC, so that perldoc
		# BSDPAN::ExtUtils::MM_Unix and friends will work as
		# expected.

		push @INC, BSDPAN->path() . "/..";
	}
}
use BSDPAN::Override;

1;
=head1 NAME

BSDPAN::Config - disable BSDPAN functionality if needed

=head1 SYNOPSIS

   None

=head1 DESCRIPTION

When perldoc(1) is invoked, this module:

=over 4

=item *

Moves the path to BSDPAN(3) from the beginning of @INC to the end of
@INC.

=item *

Adds the parent directory of the path to BSDPAN(3) to the end of @INC,
so that

    perldoc BSDPAN::Some::Module::BSDPAN::Overrides

does the right thing.

=back

This modules has no other effects.

=head1 AUTHOR

Anton Berezin, tobez@tobez.org

=head1 SEE ALSO

perl(1), L<BSDPAN(3)>, L<BSDPAN::Override(3)>, perldoc(1).

=head1 BUGS

This module is a hack.

=cut
