# Pod::Text::Overstrike -- Convert POD data to formatted overstrike text
# $Id: Overstrike.pm,v 1.1 2000/12/25 12:51:23 eagle Exp $
#
# Created by Joe Smith <Joe.Smith@inwap.com> 30-Nov-2000
#   (based on Pod::Text::Color by Russ Allbery <rra@stanford.edu>)
#
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# This was written because the output from:
#
#     pod2text Text.pm > plain.txt; less plain.txt
#
# is not as rich as the output from
#
#     pod2man Text.pm | nroff -man > fancy.txt; less fancy.txt
#
# and because both Pod::Text::Color and Pod::Text::Termcap are not device
# independent.

############################################################################
# Modules and declarations
############################################################################

package Pod::Text::Overstrike;

require 5.004;

use Pod::Text ();

use strict;
use vars qw(@ISA $VERSION);

@ISA = qw(Pod::Text);

# Don't use the CVS revision as the version, since this module is also in
# Perl core and too many things could munge CVS magic revision strings.
# This number should ideally be the same as the CVS revision in podlators,
# however.
$VERSION = 1.01;


############################################################################
# Overrides
############################################################################

# Make level one headings bold, overridding any existing formatting.
sub cmd_head1 {
    my $self = shift;
    local $_ = shift;
    s/\s+$//;
    s/(.)\cH\1//g;
    s/_\cH//g;
    s/(.)/$1\b$1/g;
    $self->SUPER::cmd_head1 ($_);
}

# Make level two headings bold, overriding any existing formatting.
sub cmd_head2 {
    my $self = shift;
    local $_ = shift;
    s/\s+$//;
    s/(.)\cH\1//g;
    s/_\cH//g;
    s/(.)/$1\b$1/g;
    $self->SUPER::cmd_head2 ($_);
}

# Make level three headings underscored, overriding any existing formatting.
sub cmd_head3 {
    my $self = shift;
    local $_ = shift;
    s/\s+$//;
    s/(.)\cH\1//g;
    s/_\cH//g;
    s/(.)/_\b$1/g;
    $self->SUPER::cmd_head3 ($_);
}

# Fix the various interior sequences.
sub seq_b { local $_ = $_[1]; s/(.)\cH\1//g; s/_\cH//g; s/(.)/$1\b$1/g; $_ }
sub seq_f { local $_ = $_[1]; s/(.)\cH\1//g; s/_\cH//g; s/(.)/_\b$1/g; $_ }
sub seq_i { local $_ = $_[1]; s/(.)\cH\1//g; s/_\cH//g; s/(.)/_\b$1/g; $_ }

# We unfortunately have to override the wrapping code here, since the normal
# wrapping code gets really confused by all the escape sequences.
sub wrap {
    my $self = shift;
    local $_ = shift;
    my $output = '';
    my $spaces = ' ' x $$self{MARGIN};
    my $width = $$self{width} - $$self{MARGIN};
    while (length > $width) {
        if (s/^((?:(?:[^\n]\cH)?[^\n]){0,$width})\s+//
            || s/^((?:(?:[^\n]\cH)?[^\n]){$width})//) {
            $output .= $spaces . $1 . "\n";
        } else {
            last;
        }
    }
    $output .= $spaces . $_;
    $output =~ s/\s+$/\n\n/;
    $output;
}

############################################################################
# Module return value and documentation
############################################################################

1;
__END__

=head1 NAME

Pod::Text::Overstrike - Convert POD data to formatted overstrike text

=head1 SYNOPSIS

    use Pod::Text::Overstrike;
    my $parser = Pod::Text::Overstrike->new (sentence => 0, width => 78);

    # Read POD from STDIN and write to STDOUT.
    $parser->parse_from_filehandle;

    # Read POD from file.pod and write to file.txt.
    $parser->parse_from_file ('file.pod', 'file.txt');

=head1 DESCRIPTION

Pod::Text::Overstrike is a simple subclass of Pod::Text that highlights
output text using overstrike sequences, in a manner similar to nroff.
Characters in bold text are overstruck (character, backspace, character) and
characters in underlined text are converted to overstruck underscores
(underscore, backspace, character).  This format was originally designed for
hardcopy terminals and/or lineprinters, yet is readable on softcopy (CRT)
terminals.

Overstruck text is best viewed by page-at-a-time programs that take
advantage of the terminal's B<stand-out> and I<underline> capabilities, such
as the less program on Unix.

Apart from the overstrike, it in all ways functions like Pod::Text.  See
L<Pod::Text> for details and available options.

=head1 BUGS

Currently, the outermost formatting instruction wins, so for example
underlined text inside a region of bold text is displayed as simply bold.
There may be some better approach possible.

=head1 SEE ALSO

L<Pod::Text|Pod::Text>, L<Pod::Parser|Pod::Parser>

=head1 AUTHOR

Joe Smith E<lt>Joe.Smith@inwap.comE<gt>, using the framework created by Russ
Allbery E<lt>rra@stanford.eduE<gt>.

=cut
