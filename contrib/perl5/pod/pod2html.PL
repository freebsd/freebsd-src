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
=pod

=head1 NAME

pod2html - convert .pod files to .html files

=head1 SYNOPSIS

    pod2html --help --htmlroot=<name> --infile=<name> --outfile=<name>
             --podpath=<name>:...:<name> --podroot=<name>
             --libpods=<name>:...:<name> --recurse --norecurse --verbose
             --index --noindex --title=<name>

=head1 DESCRIPTION

Converts files from pod format (see L<perlpod>) to HTML format.

=head1 ARGUMENTS

pod2html takes the following arguments:

=over 4

=item help

  --help

Displays the usage message.

=item htmlroot

  --htmlroot=name

Sets the base URL for the HTML files.  When cross-references are made,
the HTML root is prepended to the URL.

=item infile

  --infile=name

Specify the pod file to convert.  Input is taken from STDIN if no
infile is specified.

=item outfile

  --outfile=name

Specify the HTML file to create.  Output goes to STDOUT if no outfile
is specified.

=item podroot

  --podroot=name

Specify the base directory for finding library pods.

=item podpath

  --podpath=name:...:name

Specify which subdirectories of the podroot contain pod files whose
HTML converted forms can be linked-to in cross-references.

=item libpods

  --libpods=name:...:name

List of page names (eg, "perlfunc") which contain linkable C<=item>s.

=item netscape

  --netscape

Use Netscape HTML directives when applicable.

=item nonetscape

  --nonetscape

Do not use Netscape HTML directives (default).

=item index

  --index

Generate an index at the top of the HTML file (default behaviour).

=item noindex

  --noindex

Do not generate an index at the top of the HTML file.


=item recurse

  --recurse

Recurse into subdirectories specified in podpath (default behaviour).

=item norecurse

  --norecurse

Do not recurse into subdirectories specified in podpath.

=item title

  --title=title

Specify the title of the resulting HTML file.

=item verbose

  --verbose

Display progress messages.

=back

=head1 AUTHOR

Tom Christiansen, E<lt>tchrist@perl.comE<gt>.

=head1 BUGS

See L<Pod::Html> for a list of known bugs in the translator.

=head1 SEE ALSO

L<perlpod>, L<Pod::Html>

=head1 COPYRIGHT

This program is distributed under the Artistic License.

=cut

use Pod::Html;

pod2html @ARGV;
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
