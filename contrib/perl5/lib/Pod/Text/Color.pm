# Pod::Text::Color -- Convert POD data to formatted color ASCII text
# $Id: Color.pm,v 0.6 2000/12/25 12:52:39 eagle Exp $
#
# Copyright 1999 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# This is just a basic proof of concept.  It should later be modified to
# make better use of color, take options changing what colors are used for
# what text, and the like.

############################################################################
# Modules and declarations
############################################################################

package Pod::Text::Color;

require 5.004;

use Pod::Text ();
use Term::ANSIColor qw(colored);

use strict;
use vars qw(@ISA $VERSION);

@ISA = qw(Pod::Text);

# Don't use the CVS revision as the version, since this module is also in
# Perl core and too many things could munge CVS magic revision strings.
# This number should ideally be the same as the CVS revision in podlators,
# however.
$VERSION = 0.06;


############################################################################
# Overrides
############################################################################

# Make level one headings bold.
sub cmd_head1 {
    my $self = shift;
    local $_ = shift;
    s/\s+$//;
    $self->SUPER::cmd_head1 (colored ($_, 'bold'));
}

# Make level two headings bold.
sub cmd_head2 {
    my $self = shift;
    local $_ = shift;
    s/\s+$//;
    $self->SUPER::cmd_head2 (colored ($_, 'bold'));
}

# Fix the various interior sequences.
sub seq_b { return colored ($_[1], 'bold')   }
sub seq_f { return colored ($_[1], 'cyan')   }
sub seq_i { return colored ($_[1], 'yellow') }

# We unfortunately have to override the wrapping code here, since the normal
# wrapping code gets really confused by all the escape sequences.
sub wrap {
    my $self = shift;
    local $_ = shift;
    my $output = '';
    my $spaces = ' ' x $$self{MARGIN};
    my $width = $$self{width} - $$self{MARGIN};
    while (length > $width) {
        if (s/^((?:(?:\e\[[\d;]+m)?[^\n]){0,$width})\s+//
            || s/^((?:(?:\e\[[\d;]+m)?[^\n]){$width})//) {
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

Pod::Text::Color - Convert POD data to formatted color ASCII text

=head1 SYNOPSIS

    use Pod::Text::Color;
    my $parser = Pod::Text::Color->new (sentence => 0, width => 78);

    # Read POD from STDIN and write to STDOUT.
    $parser->parse_from_filehandle;

    # Read POD from file.pod and write to file.txt.
    $parser->parse_from_file ('file.pod', 'file.txt');

=head1 DESCRIPTION

Pod::Text::Color is a simple subclass of Pod::Text that highlights output
text using ANSI color escape sequences.  Apart from the color, it in all
ways functions like Pod::Text.  See L<Pod::Text> for details and available
options.

Term::ANSIColor is used to get colors and therefore must be installed to use
this module.

=head1 BUGS

This is just a basic proof of concept.  It should be seriously expanded to
support configurable coloration via options passed to the constructor, and
B<pod2text> should be taught about those.

=head1 SEE ALSO

L<Pod::Text|Pod::Text>, L<Pod::Parser|Pod::Parser>

=head1 AUTHOR

Russ Allbery E<lt>rra@stanford.eduE<gt>.

=cut
