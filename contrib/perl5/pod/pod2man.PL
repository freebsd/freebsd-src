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

# pod2man -- Convert POD data to formatted *roff input.
# $Id: pod2man.PL,v 1.4 2000/11/19 05:47:46 eagle Exp $
#
# Copyright 1999, 2000 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

require 5.004;

use Getopt::Long qw(GetOptions);
use Pod::Man ();
use Pod::Usage qw(pod2usage);

use strict;

# Insert -- into @ARGV before any single dash argument to hide it from
# Getopt::Long; we want to interpret it as meaning stdin (which Pod::Parser
# does correctly).
my $stdin;
@ARGV = map { $_ eq '-' && !$stdin++ ? ('--', $_) : $_ } @ARGV;

# Parse our options, trying to retain backwards compatibility with pod2man
# but allowing short forms as well.  --lax is currently ignored.
my %options;
Getopt::Long::config ('bundling_override');
GetOptions (\%options, 'section|s=s', 'release|r=s', 'center|c=s',
            'date|d=s', 'fixed=s', 'fixedbold=s', 'fixeditalic=s',
            'fixedbolditalic=s', 'official|o', 'quotes|q=s', 'lax|l',
            'help|h') or exit 1;
pod2usage (0) if $options{help};

# Official sets --center, but don't override things explicitly set.
if ($options{official} && !defined $options{center}) {
    $options{center} = 'Perl Programmers Reference Guide';
}

# Initialize and run the formatter, pulling a pair of input and output off
# at a time.
my $parser = Pod::Man->new (%options);
my @files;
do {
    @files = splice (@ARGV, 0, 2);
    $parser->parse_from_file (@files);
} while (@ARGV);
  
__END__

=head1 NAME

pod2man - Convert POD data to formatted *roff input

=head1 SYNOPSIS

pod2man [B<--section>=I<manext>] [B<--release>=I<version>]
[B<--center>=I<string>] [B<--date>=I<string>] [B<--fixed>=I<font>]
[B<--fixedbold>=I<font>] [B<--fixeditalic>=I<font>]
[B<--fixedbolditalic>=I<font>] [B<--official>] [B<--lax>]
[B<--quotes>=I<quotes>] [I<input> [I<output>] ...]

pod2man B<--help>

=head1 DESCRIPTION

B<pod2man> is a front-end for Pod::Man, using it to generate *roff input
from POD source.  The resulting *roff code is suitable for display on a
terminal using nroff(1), normally via man(1), or printing using troff(1).

I<input> is the file to read for POD source (the POD can be embedded in
code).  If I<input> isn't given, it defaults to STDIN.  I<output>, if given,
is the file to which to write the formatted output.  If I<output> isn't
given, the formatted output is written to STDOUT.  Several POD files can be
processed in the same B<pod2man> invocation (saving module load and compile
times) by providing multiple pairs of I<input> and I<output> files on the
command line.

B<--section>, B<--release>, B<--center>, B<--date>, and B<--official> can be
used to set the headers and footers to use; if not given, Pod::Man will
assume various defaults.  See below or L<Pod::Man> for details.

B<pod2man> assumes that your *roff formatters have a fixed-width font named
CW.  If yours is called something else (like CR), use B<--fixed> to specify
it.  This generally only matters for troff output for printing.  Similarly,
you can set the fonts used for bold, italic, and bold italic fixed-width
output.

Besides the obvious pod conversions, Pod::Man, and therefore pod2man also
takes care of formatting func(), func(n), and simple variable references
like $foo or @bar so you don't have to use code escapes for them; complex
expressions like C<$fred{'stuff'}> will still need to be escaped, though.
It also translates dashes that aren't used as hyphens into en dashes, makes
long dashes--like this--into proper em dashes, fixes "paired quotes," and
takes care of several other troff-specific tweaks.  See L<Pod::Man> for
complete information.

=head1 OPTIONS

=over 4

=item B<-c> I<string>, B<--center>=I<string>

Sets the centered page header to I<string>.  The default is "User
Contributed Perl Documentation", but also see B<--official> below.

=item B<-d> I<string>, B<--date>=I<string>

Set the left-hand footer string to this value.  By default, the modification
date of the input file will be used, or the current date if input comes from
STDIN.

=item B<--fixed>=I<font>

The fixed-width font to use for vertabim text and code.  Defaults to CW.
Some systems may want CR instead.  Only matters for troff(1) output.

=item B<--fixedbold>=I<font>

Bold version of the fixed-width font.  Defaults to CB.  Only matters for
troff(1) output.

=item B<--fixeditalic>=I<font>

Italic version of the fixed-width font (actually, something of a misnomer,
since most fixed-width fonts only have an oblique version, not an italic
version).  Defaults to CI.  Only matters for troff(1) output.

=item B<--fixedbolditalic>=I<font>

Bold italic (probably actually oblique) version of the fixed-width font.
Pod::Man doesn't assume you have this, and defaults to CB.  Some systems
(such as Solaris) have this font available as CX.  Only matters for troff(1)
output.

=item B<-h>, B<--help>

Print out usage information.

=item B<-l>, B<--lax>

Don't complain when required sections are missing.  Not currently used, as
POD checking functionality is not yet implemented in Pod::Man.

=item B<-o>, B<--official>

Set the default header to indicate that this page is part of the standard
Perl release, if B<--center> is not also given.

=item B<-q> I<quotes>, B<--quotes>=I<quotes>

Sets the quote marks used to surround CE<lt>> text to I<quotes>.  If
I<quotes> is a single character, it is used as both the left and right
quote; if I<quotes> is two characters, the first character is used as the
left quote and the second as the right quoted; and if I<quotes> is four
characters, the first two are used as the left quote and the second two as
the right quote.

I<quotes> may also be set to the special value C<none>, in which case no
quote marks are added around CE<lt>> text (but the font is still changed for
troff output).

=item B<-r>, B<--release>

Set the centered footer.  By default, this is the version of Perl you run
B<pod2man> under.  Note that some system an macro sets assume that the
centered footer will be a modification date and will prepend something like
"Last modified: "; if this is the case, you may want to set B<--release> to
the last modified date and B<--date> to the version number.

=item B<-s>, B<--section>

Set the section for the C<.TH> macro.  The standard section numbering
convention is to use 1 for user commands, 2 for system calls, 3 for
functions, 4 for devices, 5 for file formats, 6 for games, 7 for
miscellaneous information, and 8 for administrator commands.  There is a lot
of variation here, however; some systems (like Solaris) use 4 for file
formats, 5 for miscellaneous information, and 7 for devices.  Still others
use 1m instead of 8, or some mix of both.  About the only section numbers
that are reliably consistent are 1, 2, and 3.

By default, section 1 will be used unless the file ends in .pm in which case
section 3 will be selected.

=back

=head1 DIAGNOSTICS

If B<pod2man> fails with errors, see L<Pod::Man> and L<Pod::Parser> for
information about what those errors might mean.

=head1 EXAMPLES

    pod2man program > program.1
    pod2man SomeModule.pm /usr/perl/man/man3/SomeModule.3
    pod2man --section=7 note.pod > note.7

If you would like to print out a lot of man page continuously, you probably
want to set the C and D registers to set contiguous page numbering and
even/odd paging, at least on some versions of man(7).

    troff -man -rC1 -rD1 perl.1 perldata.1 perlsyn.1 ...

To get index entries on stderr, turn on the F register, as in:

    troff -man -rF1 perl.1

The indexing merely outputs messages via C<.tm> for each major page,
section, subsection, item, and any C<XE<lt>E<gt>> directives.  See
L<Pod::Man> for more details.

=head1 BUGS

Lots of this documentation is duplicated from L<Pod::Man>.

POD checking and the corresponding B<--lax> option don't work yet.

=head1 NOTES

For those not sure of the proper layout of a man page, here are some notes
on writing a proper man page.

The name of the program being documented is conventionally written in bold
(using BE<lt>E<gt>) wherever it occurs, as are all program options.
Arguments should be written in italics (IE<lt>E<gt>).  Functions are
traditionally written in italics; if you write a function as function(),
Pod::Man will take care of this for you.  Literal code or commands should
be in CE<lt>E<gt>.  References to other man pages should be in the form
C<manpage(section)>, and Pod::Man will automatically format those
appropriately.  As an exception, it's traditional not to use this form when
referring to module documentation; use C<LE<lt>Module::NameE<gt>> instead.

References to other programs or functions are normally in the form of man
page references so that cross-referencing tools can provide the user with
links and the like.  It's possible to overdo this, though, so be careful not
to clutter your documentation with too much markup.

The major headers should be set out using a C<=head1> directive, and are
historically written in the rather startling ALL UPPER CASE format, although
this is not mandatory.  Minor headers may be included using C<=head2>, and
are typically in mixed case.

The standard sections of a manual page are:

=over 4

=item NAME

Mandatory section; should be a comma-separated list of programs or functions
documented by this podpage, such as:

    foo, bar - programs to do something

Manual page indexers are often extremely picky about the format of this
section, so don't put anything in it except this line.  A single dash, and
only a single dash, should separate the list of programs or functions from
the description.  Functions should not be qualified with C<()> or the like.
The description should ideally fit on a single line, even if a man program
replaces the dash with a few tabs.

=item SYNOPSIS

A short usage summary for programs and functions.  This section is mandatory
for section 3 pages.

=item DESCRIPTION

Extended description and discussion of the program or functions, or the body
of the documentation for man pages that document something else.  If
particularly long, it's a good idea to break this up into subsections
C<=head2> directives like:

    =head2 Normal Usage

    =head2 Advanced Features

    =head2 Writing Configuration Files

or whatever is appropriate for your documentation.

=item OPTIONS

Detailed description of each of the command-line options taken by the
program.  This should be separate from the description for the use of things
like L<Pod::Usage|Pod::Usage>.  This is normally presented as a list, with
each option as a separate C<=item>.  The specific option string should be
enclosed in BE<lt>E<gt>.  Any values that the option takes should be
enclosed in IE<lt>E<gt>.  For example, the section for the option
B<--section>=I<manext> would be introduced with:

    =item B<--section>=I<manext>

Synonymous options (like both the short and long forms) are separated by a
comma and a space on the same C<=item> line, or optionally listed as their
own item with a reference to the canonical name.  For example, since
B<--section> can also be written as B<-s>, the above would be:

    =item B<-s> I<manext>, B<--section>=I<manext>

(Writing the short option first is arguably easier to read, since the long
option is long enough to draw the eye to it anyway and the short option can
otherwise get lost in visual noise.)

=item RETURN VALUE

What the program or function returns, if successful.  This section can be
omitted for programs whose precise exit codes aren't important, provided
they return 0 on success as is standard.  It should always be present for
functions.

=item ERRORS

Exceptions, error return codes, exit statuses, and errno settings.
Typically used for function documentation; program documentation uses
DIAGNOSTICS instead.  The general rule of thumb is that errors printed to
STDOUT or STDERR and intended for the end user are documented in DIAGNOSTICS
while errors passed internal to the calling program and intended for other
programmers are documented in ERRORS.  When documenting a function that sets
errno, a full list of the possible errno values should be given here.

=item DIAGNOSTICS

All possible messages the program can print out--and what they mean.  You
may wish to follow the same documentation style as the Perl documentation;
see perldiag(1) for more details (and look at the POD source as well).

If applicable, please include details on what the user should do to correct
the error; documenting an error as indicating "the input buffer is too
small" without telling the user how to increase the size of the input buffer
(or at least telling them that it isn't possible) aren't very useful.

=item EXAMPLES

Give some example uses of the program or function.  Don't skimp; users often
find this the most useful part of the documentation.  The examples are
generally given as verbatim paragraphs.

Don't just present an example without explaining what it does.  Adding a
short paragraph saying what the example will do can increase the value of
the example immensely.

=item ENVIRONMENT

Environment variables that the program cares about, normally presented as a
list using C<=over>, C<=item>, and C<=back>.  For example:

    =over 6

    =item HOME

    Used to determine the user's home directory.  F<.foorc> in this
    directory is read for configuration details, if it exists.

    =back

Since environment variables are normally in all uppercase, no additional
special formatting is generally needed; they're glaring enough as it is.

=item FILES

All files used by the program or function, normally presented as a list, and
what it uses them for.  File names should be enclosed in FE<lt>E<gt>.  It's
particularly important to document files that will be potentially modified.

=item CAVEATS

Things to take special care with, sometimes called WARNINGS.

=item BUGS

Things that are broken or just don't work quite right.

=item RESTRICTIONS

Bugs you don't plan to fix.  :-)

=item NOTES

Miscellaneous commentary.

=item SEE ALSO

Other man pages to check out, like man(1), man(7), makewhatis(8), or
catman(8).  Normally a simple list of man pages separated by commas, or a
paragraph giving the name of a reference work.  Man page references, if they
use the standard C<name(section)> form, don't have to be enclosed in
LE<lt>E<gt>, but other things in this section probably should be when
appropriate.  You may need to use the C<LE<lt>...|...E<gt>> syntax to keep
B<pod2man> and B<pod2text> from being too verbose; see perlpod(1).

If the package has a web site, include a URL here.

=item AUTHOR

Who wrote it (use AUTHORS for multiple people).  Including your current
e-mail address (or some e-mail address to which bug reports should be sent)
so that users have a way of contacting you is a good idea.  Remember that
program documentation tends to roam the wild for far longer than you expect
and pick an e-mail address that's likely to last if possible.

=item HISTORY

Programs derived from other sources sometimes have this, or you might keep a
modification log here.

=back

In addition, some systems use CONFORMING TO to note conformance to relevant
standards and MT-LEVEL to note safeness for use in threaded programs or
signal handlers.  These headings are primarily useful when documenting parts
of a C library.  Documentation of object-oriented libraries or modules may
use CONSTRUCTORS and METHODS sections for detailed documentation of the
parts of the library and save the DESCRIPTION section for an overview; other
large modules may use FUNCTIONS for similar reasons.  Some people use
OVERVIEW to summarize the description if it's quite long.  Sometimes there's
an additional COPYRIGHT section at the bottom, for licensing terms.
AVAILABILITY is sometimes added, giving the canonical download site for the
software or a URL for updates.

Section ordering varies, although NAME should I<always> be the first section
(you'll break some man page systems otherwise), and NAME, SYNOPSIS,
DESCRIPTION, and OPTIONS generally always occur first and in that order if
present.  In general, SEE ALSO, AUTHOR, and similar material should be left
for last.  Some systems also move WARNINGS and NOTES to last.  The order
given above should be reasonable for most purposes.

Finally, as a general note, try not to use an excessive amount of markup.
As documented here and in L<Pod::Man>, you can safely leave Perl variables,
function names, man page references, and the like unadorned by markup and
the POD translators will figure it out for you.  This makes it much easier
to later edit the documentation.  Note that many existing translators
(including this one currently) will do the wrong thing with e-mail addresses
or URLs when wrapped in LE<lt>E<gt>, so don't do that.

For additional information that may be more accurate for your specific
system, see either man(5) or man(7) depending on your system manual section
numbering conventions.

=head1 SEE ALSO

L<Pod::Man|Pod::Man>, L<Pod::Parser|Pod::Parser>, man(1), nroff(1),
troff(1), man(7)

The man page documenting the an macro set may be man(5) instead of man(7) on
your system.

=head1 AUTHOR

Russ Allbery E<lt>rra@stanford.eduE<gt>, based I<very> heavily on the
original B<pod2man> by Larry Wall and Tom Christiansen.  Large portions of
this documentation, particularly the sections on the anatomy of a proper man
page, are taken from the B<pod2man> documentation by Tom.

=cut
!NO!SUBS!
#'# (cperl-mode)

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
