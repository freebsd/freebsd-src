
package Text::Tabs;

require Exporter;

@ISA = (Exporter);
@EXPORT = qw(expand unexpand $tabstop);

use vars qw($VERSION $tabstop $debug);
$VERSION = 96.121201;

use strict;

BEGIN	{
	$tabstop = 8;
	$debug = 0;
}

sub expand
{
	my @l = @_;
	for $_ (@l) {
		1 while s/(^|\n)([^\t\n]*)(\t+)/
			$1. $2 . (" " x 
				($tabstop * length($3)
				- (length($2) % $tabstop)))
			/sex;
	}
	return @l if wantarray;
	return $l[0];
}

sub unexpand
{
	my @l = @_;
	my @e;
	my $x;
	my $line;
	my @lines;
	my $lastbit;
	for $x (@l) {
		@lines = split("\n", $x, -1);
		for $line (@lines) {
			$line = expand($line);
			@e = split(/(.{$tabstop})/,$line,-1);
			$lastbit = pop(@e);
			$lastbit = '' unless defined $lastbit;
			$lastbit = "\t"
				if $lastbit eq " "x$tabstop;
			for $_ (@e) {
				if ($debug) {
					my $x = $_;
					$x =~ s/\t/^I\t/gs;
					print "sub on '$x'\n";
				}
				s/  +$/\t/;
			}
			$line = join('',@e, $lastbit);
		}
		$x = join("\n", @lines);
	}
	return @l if wantarray;
	return $l[0];
}

1;
__END__


=head1 NAME

Text::Tabs -- expand and unexpand tabs per the unix expand(1) and unexpand(1)

=head1 SYNOPSIS

use Text::Tabs;

$tabstop = 4;
@lines_without_tabs = expand(@lines_with_tabs);
@lines_with_tabs = unexpand(@lines_without_tabs);

=head1 DESCRIPTION

Text::Tabs does about what the unix utilities expand(1) and unexpand(1)
do.  Given a line with tabs in it, expand will replace the tabs with
the appropriate number of spaces.  Given a line with or without tabs in
it, unexpand will add tabs when it can save bytes by doing so.  Invisible
compression with plain ascii!

=head1 BUGS

expand doesn't handle newlines very quickly -- do not feed it an
entire document in one string.  Instead feed it an array of lines.

=head1 AUTHOR

David Muir Sharnoff <muir@idiom.com>
