# Helper functions for test programs written in Perl.
#
# This module provides a collection of helper functions used by test programs
# written in Perl.  This is a general collection of functions that can be used
# by both C packages with Automake and by stand-alone Perl modules.  See
# Test::RRA::Automake for additional functions specifically for C Automake
# distributions.
#
# SPDX-License-Identifier: MIT

package Test::RRA;

use 5.010;
use base qw(Exporter);
use strict;
use warnings;

use Carp qw(croak);
use File::Temp;

# Abort if Test::More was loaded before Test::RRA to be sure that we get the
# benefits of the Test::More probing below.
if ($INC{'Test/More.pm'}) {
    croak('Test::More loaded before Test::RRA');
}

# Red Hat's base perl package doesn't include Test::More (one has to install
# the perl-core package in addition).  Try to detect this and skip any Perl
# tests if Test::More is not present.  This relies on Test::RRA being included
# before Test::More.
eval {
    require Test::More;
    Test::More->import();
};
if ($@) {
    print "1..0 # SKIP Test::More required for test\n"
      or croak('Cannot write to stdout');
    exit 0;
}

# Declare variables that should be set in BEGIN for robustness.
our (@EXPORT_OK, $VERSION);

# Set $VERSION and everything export-related in a BEGIN block for robustness
# against circular module loading (not that we load any modules, but
# consistency is good).
BEGIN {
    @EXPORT_OK = qw(
        is_file_contents skip_unless_author skip_unless_automated use_prereq
    );

    # This version should match the corresponding rra-c-util release, but with
    # two digits for the minor version, including a leading zero if necessary,
    # so that it will sort properly.
    $VERSION = '10.00';
}

# Compare a string to the contents of a file, similar to the standard is()
# function, but to show the line-based unified diff between them if they
# differ.
#
# $got      - The output that we received
# $expected - The path to the file containing the expected output
# $message  - The message to use when reporting the test results
#
# Returns: undef
#  Throws: Exception on failure to read or write files or run diff
sub is_file_contents {
    my ($got, $expected, $message) = @_;

    # If they're equal, this is simple.
    open(my $fh, '<', $expected) or BAIL_OUT("Cannot open $expected: $!\n");
    my $data = do { local $/ = undef; <$fh> };
    close($fh) or BAIL_OUT("Cannot close $expected: $!\n");
    if ($got eq $data) {
        is($got, $data, $message);
        return;
    }

    # Otherwise, we show a diff, but only if we have IPC::System::Simple and
    # diff succeeds.  Otherwise, we fall back on showing the full expected and
    # seen output.
    eval {
        require IPC::System::Simple;

        my $tmp     = File::Temp->new();
        my $tmpname = $tmp->filename;
        print {$tmp} $got or BAIL_OUT("Cannot write to $tmpname: $!\n");
        my @command = ('diff', '-u', $expected, $tmpname);
        my $diff    = IPC::System::Simple::capturex([0 .. 1], @command);
        diag($diff);
    };
    if ($@) {
        diag('Expected:');
        diag($expected);
        diag('Seen:');
        diag($data);
    }

    # Report failure.
    ok(0, $message);
    return;
}

# Skip this test unless author tests are requested.  Takes a short description
# of what tests this script would perform, which is used in the skip message.
# Calls plan skip_all, which will terminate the program.
#
# $description - Short description of the tests
#
# Returns: undef
sub skip_unless_author {
    my ($description) = @_;
    if (!$ENV{AUTHOR_TESTING}) {
        plan(skip_all => "$description only run for author");
    }
    return;
}

# Skip this test unless doing automated testing or release testing.  This is
# used for tests that should be run by CPAN smoke testing or during releases,
# but not for manual installs by end users.  Takes a short description of what
# tests this script would perform, which is used in the skip message.  Calls
# plan skip_all, which will terminate the program.
#
# $description - Short description of the tests
#
# Returns: undef
sub skip_unless_automated {
    my ($description) = @_;
    for my $env (qw(AUTOMATED_TESTING RELEASE_TESTING AUTHOR_TESTING)) {
        return if $ENV{$env};
    }
    plan(skip_all => "$description normally skipped");
    return;
}

# Attempt to load a module and skip the test if the module could not be
# loaded.  If the module could be loaded, call its import function manually.
# If the module could not be loaded, calls plan skip_all, which will terminate
# the program.
#
# The special logic here is based on Test::More and is required to get the
# imports to happen in the caller's namespace.
#
# $module  - Name of the module to load
# @imports - Any arguments to import, possibly including a version
#
# Returns: undef
sub use_prereq {
    my ($module, @imports) = @_;

    # If the first import looks like a version, pass it as a bare string.
    my $version = q{};
    if (@imports >= 1 && $imports[0] =~ m{ \A \d+ (?: [.][\d_]+ )* \z }xms) {
        $version = shift(@imports);
    }

    # Get caller information to put imports in the correct package.
    my ($package) = caller;

    # Do the import with eval, and try to isolate it from the surrounding
    # context as much as possible.  Based heavily on Test::More::_eval.
    ## no critic (BuiltinFunctions::ProhibitStringyEval)
    ## no critic (ValuesAndExpressions::ProhibitImplicitNewlines)
    my ($result, $error, $sigdie);
    {
        local $@            = undef;
        local $!            = undef;
        local $SIG{__DIE__} = undef;
        $result = eval qq{
            package $package;
            use $module $version \@imports;
            1;
        };
        $error  = $@;
        $sigdie = $SIG{__DIE__} || undef;
    }

    # If the use failed for any reason, skip the test.
    if (!$result || $error) {
        my $name = length($version) > 0 ? "$module $version" : $module;
        plan(skip_all => "$name required for test");
    }

    # If the module set $SIG{__DIE__}, we cleared that via local.  Restore it.
    ## no critic (Variables::RequireLocalizedPunctuationVars)
    if (defined($sigdie)) {
        $SIG{__DIE__} = $sigdie;
    }
    return;
}

1;
__END__

=for stopwords
Allbery Allbery's DESC bareword sublicense MERCHANTABILITY NONINFRINGEMENT
rra-c-util CPAN diff

=head1 NAME

Test::RRA - Support functions for Perl tests

=head1 SYNOPSIS

    use Test::RRA
      qw(skip_unless_author skip_unless_automated use_prereq);

    # Skip this test unless author tests are requested.
    skip_unless_author('Coding style tests');

    # Skip this test unless doing automated or release testing.
    skip_unless_automated('POD syntax tests');

    # Load modules, skipping the test if they're not available.
    use_prereq('Perl6::Slurp', 'slurp');
    use_prereq('Test::Script::Run', '0.04');

=head1 DESCRIPTION

This module collects utility functions that are useful for Perl test scripts.
It assumes Russ Allbery's Perl module layout and test conventions and will
only be useful for other people if they use the same conventions.

This module B<must> be loaded before Test::More or it will abort during
import.  It will skip the test (by printing a skip message to standard output
and exiting with status 0, equivalent to C<plan skip_all>) during import if
Test::More is not available.  This allows tests written in Perl using this
module to be skipped if run on a system with Perl but not Test::More, such as
Red Hat systems with the C<perl> package but not the C<perl-core> package
installed.

=head1 FUNCTIONS

None of these functions are imported by default.  The ones used by a script
should be explicitly imported.

=over 4

=item is_file_contents(GOT, EXPECTED, MESSAGE)

Check a string against the contents of a file, showing the differences if any
using diff (if IPC::System::Simple and diff are available).  GOT is the output
the test received.  EXPECTED is the path to a file containing the expected
output (not the output itself).  MESSAGE is a message to display alongside the
test results.

=item skip_unless_author(DESC)

Checks whether AUTHOR_TESTING is set in the environment and skips the whole
test (by calling C<plan skip_all> from Test::More) if it is not.  DESC is a
description of the tests being skipped.  A space and C<only run for author>
will be appended to it and used as the skip reason.

=item skip_unless_automated(DESC)

Checks whether AUTHOR_TESTING, AUTOMATED_TESTING, or RELEASE_TESTING are set
in the environment and skips the whole test (by calling C<plan skip_all> from
Test::More) if they are not.  This should be used by tests that should not run
during end-user installs of the module, but which should run as part of CPAN
smoke testing and release testing.

DESC is a description of the tests being skipped.  A space and C<normally
skipped> will be appended to it and used as the skip reason.

=item use_prereq(MODULE[, VERSION][, IMPORT ...])

Attempts to load MODULE with the given VERSION and import arguments.  If this
fails for any reason, the test will be skipped (by calling C<plan skip_all>
from Test::More) with a skip reason saying that MODULE is required for the
test.

VERSION will be passed to C<use> as a version bareword if it looks like a
version number.  The remaining IMPORT arguments will be passed as the value of
an array.

=back

=head1 AUTHOR

Russ Allbery <eagle@eyrie.org>

=head1 COPYRIGHT AND LICENSE

Copyright 2016, 2018-2019, 2021 Russ Allbery <eagle@eyrie.org>

Copyright 2013-2014 The Board of Trustees of the Leland Stanford Junior
University

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

=head1 SEE ALSO

Test::More(3), Test::RRA::Automake(3), Test::RRA::Config(3)

This module is maintained in the rra-c-util package.  The current version is
available from L<https://www.eyrie.org/~eagle/software/rra-c-util/>.

The functions to control when tests are run use environment variables defined
by the L<Lancaster
Consensus|https://github.com/Perl-Toolchain-Gang/toolchain-site/blob/master/lancaster-consensus.md>.

=cut

# Local Variables:
# copyright-at-end-flag: t
# End:
