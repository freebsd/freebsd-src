#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);
use Cwd;

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
$origdir = cwd;
chdir dirname($0);
$file = basename($0, '.PL');
$file .= '.com' if $^O eq 'VMS';

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{startperl}
    eval 'exec $Config{perlpath} -S \$0 \${1+"\$@"}'
        if \$running_under_some_shell;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

# pod2text -- Convert POD data to formatted ASCII text.
#
# Copyright 1999, 2000 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# The driver script for Pod::Text, Pod::Text::Termcap, and Pod::Text::Color,
# invoked by perldoc -t among other things.

require 5.004;

use Getopt::Long qw(GetOptions);
use Pod::Text ();
use Pod::Usage qw(pod2usage);

use strict;

# Take an initial pass through our options, looking for one of the form
# -<number>.  We turn that into -w <number> for compatibility with the
# original pod2text script.
for (my $i = 0; $i < @ARGV; $i++) {
    last if $ARGV[$i] =~ /^--$/;
    if ($ARGV[$i] =~ /^-(\d+)$/) {
        splice (@ARGV, $i++, 1, '-w', $1);
    }
}

# Insert -- into @ARGV before any single dash argument to hide it from
# Getopt::Long; we want to interpret it as meaning stdin (which Pod::Parser
# does correctly).
my $stdin;
@ARGV = map { $_ eq '-' && !$stdin++ ? ('--', $_) : $_ } @ARGV;

# Parse our options.  Use the same names as Pod::Text for simplicity, and
# default to sentence boundaries turned off for compatibility.
my %options;
$options{sentence} = 0;
Getopt::Long::config ('bundling');
GetOptions (\%options, 'alt|a', 'color|c', 'help|h', 'indent|i=i',
            'loose|l', 'overstrike|o', 'quotes|q=s', 'sentence|s',
            'termcap|t', 'width|w=i') or exit 1;
pod2usage (1) if $options{help};

# Figure out what formatter we're going to use.  -c overrides -t.
my $formatter = 'Pod::Text';
if ($options{color}) {
    $formatter = 'Pod::Text::Color';
    eval { require Term::ANSIColor };
    if ($@) { die "-c (--color) requires Term::ANSIColor be installed\n" }
    require Pod::Text::Color;
} elsif ($options{termcap}) {
    $formatter = 'Pod::Text::Termcap';
    require Pod::Text::Termcap;
} elsif ($options{overstrike}) {
    $formatter = 'Pod::Text::Overstrike';
    require Pod::Text::Overstrike;
}
delete @options{'color', 'termcap', 'overstrike'};

# Initialize and run the formatter.
my $parser = $formatter->new (%options);
$parser->parse_from_file (@ARGV);

__END__

=head1 NAME

pod2text - Convert POD data to formatted ASCII text

=head1 SYNOPSIS

pod2text [B<-aclost>] [B<-i> I<indent>] [B<-q> I<quotes>] [B<-w> I<width>]
[I<input> [I<output>]]

pod2text B<-h>

=head1 DESCRIPTION

B<pod2text> is a front-end for Pod::Text and its subclasses.  It uses them
to generate formatted ASCII text from POD source.  It can optionally use
either termcap sequences or ANSI color escape sequences to format the text.

I<input> is the file to read for POD source (the POD can be embedded in
code).  If I<input> isn't given, it defaults to STDIN.  I<output>, if given,
is the file to which to write the formatted output.  If I<output> isn't
given, the formatted output is written to STDOUT.

=head1 OPTIONS

=over 4

=item B<-a>, B<--alt>

Use an alternate output format that, among other things, uses a different
heading style and marks C<=item> entries with a colon in the left margin.

=item B<-c>, B<--color>

Format the output with ANSI color escape sequences.  Using this option
requires that Term::ANSIColor be installed on your system.

=item B<-i> I<indent>, B<--indent=>I<indent>

Set the number of spaces to indent regular text, and the default indentation
for C<=over> blocks.  Defaults to 4 spaces if this option isn't given.

=item B<-h>, B<--help>

Print out usage information and exit.

=item B<-l>, B<--loose>

Print a blank line after a C<=head1> heading.  Normally, no blank line is
printed after C<=head1>, although one is still printed after C<=head2>,
because this is the expected formatting for manual pages; if you're
formatting arbitrary text documents, using this option is recommended.

=item B<-o>, B<--overstrike>

Format the output with overstruck printing.  Bold text is rendered as
character, backspace, character.  Italics and file names are rendered as
underscore, backspace, character.  Many pagers, such as B<less>, know how
to convert this to bold or underlined text.

=item B<-q> I<quotes>, B<--quotes>=I<quotes>

Sets the quote marks used to surround CE<lt>> text to I<quotes>.  If
I<quotes> is a single character, it is used as both the left and right
quote; if I<quotes> is two characters, the first character is used as the
left quote and the second as the right quoted; and if I<quotes> is four
characters, the first two are used as the left quote and the second two as
the right quote.

I<quotes> may also be set to the special value C<none>, in which case no
quote marks are added around CE<lt>> text.

=item B<-s>, B<--sentence>

Assume each sentence ends with two spaces and try to preserve that spacing.
Without this option, all consecutive whitespace in non-verbatim paragraphs
is compressed into a single space.

=item B<-t>, B<--termcap>

Try to determine the width of the screen and the bold and underline
sequences for the terminal from termcap, and use that information in
formatting the output.  Output will be wrapped at two columns less than the
width of your terminal device.  Using this option requires that your system
have a termcap file somewhere where Term::Cap can find it and requires that
your system support termios.  With this option, the output of B<pod2text>
will contain terminal control sequences for your current terminal type.

=item B<-w>, B<--width=>I<width>, B<->I<width>

The column at which to wrap text on the right-hand side.  Defaults to 76,
unless B<-t> is given, in which case it's two columns less than the width of
your terminal device.

=back

=head1 DIAGNOSTICS

If B<pod2text> fails with errors, see L<Pod::Text> and L<Pod::Parser> for
information about what those errors might mean.  Internally, it can also
produce the following diagnostics:

=over 4

=item -c (--color) requires Term::ANSIColor be installed

(F) B<-c> or B<--color> were given, but Term::ANSIColor could not be
loaded.

=item Unknown option: %s

(F) An unknown command line option was given.

=back

In addition, other L<Getopt::Long|Getopt::Long> error messages may result
from invalid command-line options.

=head1 ENVIRONMENT

=over 4

=item COLUMNS

If B<-t> is given, B<pod2text> will take the current width of your screen
from this environment variable, if available.  It overrides terminal width
information in TERMCAP.

=item TERMCAP

If B<-t> is given, B<pod2text> will use the contents of this environment
variable if available to determine the correct formatting sequences for your
current terminal device.

=back

=head1 SEE ALSO

L<Pod::Text|Pod::Text>, L<Pod::Text::Color|Pod::Text::Color>,
L<Pod::Text::Termcap|Pod::Text::Termcap>, L<Pod::Parser|Pod::Parser>

=head1 AUTHOR

Russ Allbery E<lt>rra@stanford.eduE<gt>.

=cut
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
