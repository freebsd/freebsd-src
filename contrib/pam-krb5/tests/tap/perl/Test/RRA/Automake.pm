# Helper functions for Perl test programs in Automake distributions.
#
# This module provides a collection of helper functions used by test programs
# written in Perl and included in C source distributions that use Automake.
# They embed knowledge of how I lay out my source trees and test suites with
# Autoconf and Automake.  They may be usable by others, but doing so will
# require closely following the conventions implemented by the rra-c-util
# utility collection.
#
# All the functions here assume that C_TAP_BUILD and C_TAP_SOURCE are set in
# the environment.  This is normally done via the C TAP Harness runtests
# wrapper.
#
# SPDX-License-Identifier: MIT

package Test::RRA::Automake;

use 5.010;
use base qw(Exporter);
use strict;
use warnings;

use Exporter;
use File::Find qw(find);
use File::Spec;
use Test::More;
use Test::RRA::Config qw($LIBRARY_PATH);

# Used below for use lib calls.
my ($PERL_BLIB_ARCH, $PERL_BLIB_LIB);

# Determine the path to the build tree of any embedded Perl module package in
# this source package.  We do this in a BEGIN block because we're going to use
# the results in a use lib command below.
BEGIN {
    $PERL_BLIB_ARCH = File::Spec->catdir(qw(perl blib arch));
    $PERL_BLIB_LIB  = File::Spec->catdir(qw(perl blib lib));

    # If C_TAP_BUILD is set, we can come up with better values.
    if (defined($ENV{C_TAP_BUILD})) {
        my ($vol, $dirs) = File::Spec->splitpath($ENV{C_TAP_BUILD}, 1);
        my @dirs = File::Spec->splitdir($dirs);
        pop(@dirs);
        $PERL_BLIB_ARCH = File::Spec->catdir(@dirs, qw(perl blib arch));
        $PERL_BLIB_LIB  = File::Spec->catdir(@dirs, qw(perl blib lib));
    }
}

# Prefer the modules built as part of our source package.  Otherwise, we may
# not find Perl modules while testing, or find the wrong versions.
use lib $PERL_BLIB_ARCH;
use lib $PERL_BLIB_LIB;

# Declare variables that should be set in BEGIN for robustness.
our (@EXPORT_OK, $VERSION);

# Set $VERSION and everything export-related in a BEGIN block for robustness
# against circular module loading (not that we load any modules, but
# consistency is good).
BEGIN {
    @EXPORT_OK = qw(
        all_files automake_setup perl_dirs test_file_path test_tmpdir
    );

    # This version should match the corresponding rra-c-util release, but with
    # two digits for the minor version, including a leading zero if necessary,
    # so that it will sort properly.
    $VERSION = '10.00';
}

# Directories to skip globally when looking for all files, or for directories
# that could contain Perl files.
my @GLOBAL_SKIP = qw(
    .git .pc _build autom4te.cache build-aux perl/_build perl/blib
);

# Additional paths to skip when building a list of all files in the
# distribution.  This primarily skips build artifacts that aren't interesting
# to any of the tests.  These match any path component.
my @FILES_SKIP = qw(
    .deps .dirstamp .libs aclocal.m4 config.h config.h.in config.h.in~
    config.log config.status configure configure~
);

# The temporary directory created by test_tmpdir, if any.  If this is set,
# attempt to remove the directory stored here on program exit (but ignore
# failure to do so).
my $TMPDIR;

# Returns a list of all files in the distribution.
#
# Returns: List of files
sub all_files {
    my @files;

    # Turn the skip lists into hashes for ease of querying.
    my %skip       = map { $_ => 1 } @GLOBAL_SKIP;
    my %files_skip = map { $_ => 1 } @FILES_SKIP;

    # Wanted function for find.  Prune anything matching either of the skip
    # lists, or *.lo files, and then add all regular files to the list.
    my $wanted = sub {
        my $file = $_;
        my $path = $File::Find::name;
        $path =~ s{ \A [.]/ }{}xms;
        if ($skip{$path} || $files_skip{$file} || $file =~ m{ [.]lo\z }xms) {
            $File::Find::prune = 1;
            return;
        }
        if (!-d $file) {
            push(@files, $path);
        }
    };

    # Do the recursive search and return the results.
    find($wanted, q{.});
    return @files;
}

# Perform initial test setup for running a Perl test in an Automake package.
# This verifies that C_TAP_BUILD and C_TAP_SOURCE are set and then changes
# directory to the C_TAP_SOURCE directory by default.  Sets LD_LIBRARY_PATH if
# the $LIBRARY_PATH configuration option is set.  Calls BAIL_OUT if
# C_TAP_BUILD or C_TAP_SOURCE are missing or if anything else fails.
#
# $args_ref - Reference to a hash of arguments to configure behavior:
#   chdir_build - If set to a true value, changes to C_TAP_BUILD instead of
#                 C_TAP_SOURCE
#
# Returns: undef
sub automake_setup {
    my ($args_ref) = @_;

    # Bail if C_TAP_BUILD or C_TAP_SOURCE are not set.
    if (!$ENV{C_TAP_BUILD}) {
        BAIL_OUT('C_TAP_BUILD not defined (run under runtests)');
    }
    if (!$ENV{C_TAP_SOURCE}) {
        BAIL_OUT('C_TAP_SOURCE not defined (run under runtests)');
    }

    # C_TAP_BUILD or C_TAP_SOURCE will be the test directory.  Change to the
    # parent.
    my $start;
    if ($args_ref->{chdir_build}) {
        $start = $ENV{C_TAP_BUILD};
    } else {
        $start = $ENV{C_TAP_SOURCE};
    }
    my ($vol, $dirs) = File::Spec->splitpath($start, 1);
    my @dirs = File::Spec->splitdir($dirs);
    pop(@dirs);

    # Simplify relative paths at the end of the directory.
    my $ups = 0;
    my $i   = $#dirs;
    while ($i > 2 && $dirs[$i] eq File::Spec->updir) {
        $ups++;
        $i--;
    }
    for (1 .. $ups) {
        pop(@dirs);
        pop(@dirs);
    }
    my $root = File::Spec->catpath($vol, File::Spec->catdir(@dirs), q{});
    chdir($root) or BAIL_OUT("cannot chdir to $root: $!");

    # If C_TAP_BUILD is a subdirectory of C_TAP_SOURCE, add it to the global
    # ignore list.
    my ($buildvol, $builddirs) = File::Spec->splitpath($ENV{C_TAP_BUILD}, 1);
    my @builddirs = File::Spec->splitdir($builddirs);
    pop(@builddirs);
    if ($buildvol eq $vol && @builddirs == @dirs + 1) {
        while (@dirs && $builddirs[0] eq $dirs[0]) {
            shift(@builddirs);
            shift(@dirs);
        }
        if (@builddirs == 1) {
            push(@GLOBAL_SKIP, $builddirs[0]);
        }
    }

    # Set LD_LIBRARY_PATH if the $LIBRARY_PATH configuration option is set.
    ## no critic (Variables::RequireLocalizedPunctuationVars)
    if (defined($LIBRARY_PATH)) {
        @builddirs = File::Spec->splitdir($builddirs);
        pop(@builddirs);
        my $libdir = File::Spec->catdir(@builddirs, $LIBRARY_PATH);
        my $path   = File::Spec->catpath($buildvol, $libdir, q{});
        if (-d "$path/.libs") {
            $path .= '/.libs';
        }
        if ($ENV{LD_LIBRARY_PATH}) {
            $ENV{LD_LIBRARY_PATH} .= ":$path";
        } else {
            $ENV{LD_LIBRARY_PATH} = $path;
        }
    }
    return;
}

# Returns a list of directories that may contain Perl scripts and that should
# be passed to Perl test infrastructure that expects a list of directories to
# recursively check.  The list will be all eligible top-level directories in
# the package except for the tests directory, which is broken out to one
# additional level.  Calls BAIL_OUT on any problems
#
# $args_ref - Reference to a hash of arguments to configure behavior:
#   skip - A reference to an array of directories to skip
#
# Returns: List of directories possibly containing Perl scripts to test
sub perl_dirs {
    my ($args_ref) = @_;

    # Add the global skip list.  We also ignore the perl directory if it
    # exists since, in my packages, it is treated as a Perl module
    # distribution and has its own standalone test suite.
    my @skip = $args_ref->{skip} ? @{ $args_ref->{skip} } : ();
    push(@skip, @GLOBAL_SKIP, 'perl');

    # Separate directories to skip under tests from top-level directories.
    my @skip_tests = grep { m{ \A tests/ }xms } @skip;
    @skip = grep { !m{ \A tests }xms } @skip;
    for my $skip_dir (@skip_tests) {
        $skip_dir =~ s{ \A tests/ }{}xms;
    }

    # Convert the skip lists into hashes for convenience.
    my %skip       = map { $_ => 1 } @skip, 'tests';
    my %skip_tests = map { $_ => 1 } @skip_tests;

    # Build the list of top-level directories to test.
    opendir(my $rootdir, q{.}) or BAIL_OUT("cannot open .: $!");
    my @dirs = grep { -d && !$skip{$_} } readdir($rootdir);
    closedir($rootdir);
    @dirs = File::Spec->no_upwards(@dirs);

    # Add the list of subdirectories of the tests directory.
    if (-d 'tests') {
        opendir(my $testsdir, q{tests}) or BAIL_OUT("cannot open tests: $!");

        # Skip if found in %skip_tests or if not a directory.
        my $is_skipped = sub {
            my ($dir) = @_;
            return 1 if $skip_tests{$dir};
            $dir = File::Spec->catdir('tests', $dir);
            return -d $dir ? 0 : 1;
        };

        # Build the filtered list of subdirectories of tests.
        my @test_dirs = grep { !$is_skipped->($_) } readdir($testsdir);
        closedir($testsdir);
        @test_dirs = File::Spec->no_upwards(@test_dirs);

        # Add the tests directory to the start of the directory name.
        push(@dirs, map { File::Spec->catdir('tests', $_) } @test_dirs);
    }
    return @dirs;
}

# Find a configuration file for the test suite.  Searches relative to
# C_TAP_BUILD first and then C_TAP_SOURCE and returns whichever is found
# first.  Calls BAIL_OUT if the file could not be found.
#
# $file - Partial path to the file
#
# Returns: Full path to the file
sub test_file_path {
    my ($file) = @_;
  BASE:
    for my $base ($ENV{C_TAP_BUILD}, $ENV{C_TAP_SOURCE}) {
        next if !defined($base);
        if (-e "$base/$file") {
            return "$base/$file";
        }
    }
    BAIL_OUT("cannot find $file");
    return;
}

# Create a temporary directory for tests to use for transient files and return
# the path to that directory.  The directory is automatically removed on
# program exit.  The directory permissions use the current umask.  Calls
# BAIL_OUT if the directory could not be created.
#
# Returns: Path to a writable temporary directory
sub test_tmpdir {
    my $path;

    # If we already figured out what directory to use, reuse the same path.
    # Otherwise, create a directory relative to C_TAP_BUILD if set.
    if (defined($TMPDIR)) {
        $path = $TMPDIR;
    } else {
        my $base;
        if (defined($ENV{C_TAP_BUILD})) {
            $base = $ENV{C_TAP_BUILD};
        } else {
            $base = File::Spec->curdir;
        }
        $path = File::Spec->catdir($base, 'tmp');
    }

    # Create the directory if it doesn't exist.
    if (!-d $path) {
        if (!mkdir($path, 0777)) {
            BAIL_OUT("cannot create directory $path: $!");
        }
    }

    # Store the directory name for cleanup and return it.
    $TMPDIR = $path;
    return $path;
}

# On program exit, remove $TMPDIR if set and if possible.  Report errors with
# diag but otherwise ignore them.
END {
    if (defined($TMPDIR) && -d $TMPDIR) {
        local $! = undef;
        if (!rmdir($TMPDIR)) {
            diag("cannot remove temporary directory $TMPDIR: $!");
        }
    }
}

1;
__END__

=for stopwords
Allbery Automake Automake-aware Automake-based rra-c-util ARGS subdirectories
sublicense MERCHANTABILITY NONINFRINGEMENT umask

=head1 NAME

Test::RRA::Automake - Automake-aware support functions for Perl tests

=head1 SYNOPSIS

    use Test::RRA::Automake qw(automake_setup perl_dirs test_file_path);
    automake_setup({ chdir_build => 1 });

    # Paths to directories that may contain Perl scripts.
    my @dirs = perl_dirs({ skip => [qw(lib)] });

    # Configuration for Kerberos tests.
    my $keytab = test_file_path('config/keytab');

=head1 DESCRIPTION

This module collects utility functions that are useful for test scripts
written in Perl and included in a C Automake-based package.  They assume the
layout of a package that uses rra-c-util and C TAP Harness for the test
structure.

Loading this module will also add the directories C<perl/blib/arch> and
C<perl/blib/lib> to the Perl library search path, relative to C_TAP_BUILD if
that environment variable is set.  This is harmless for C Automake projects
that don't contain an embedded Perl module, and for those projects that do,
this will allow subsequent C<use> calls to find modules that are built as part
of the package build process.

The automake_setup() function should be called before calling any other
functions provided by this module.

=head1 FUNCTIONS

None of these functions are imported by default.  The ones used by a script
should be explicitly imported.  On failure, all of these functions call
BAIL_OUT (from Test::More).

=over 4

=item all_files()

Returns a list of all "interesting" files in the distribution that a test
suite may want to look at.  This excludes various products of the build system,
the build directory if it's under the source directory, and a few other
uninteresting directories like F<.git>.  The returned paths will be paths
relative to the root of the package.

=item automake_setup([ARGS])

Verifies that the C_TAP_BUILD and C_TAP_SOURCE environment variables are set
and then changes directory to the top of the source tree (which is one
directory up from the C_TAP_SOURCE path, since C_TAP_SOURCE points to the top
of the tests directory).

If ARGS is given, it should be a reference to a hash of configuration options.
Only one option is supported: C<chdir_build>.  If it is set to a true value,
automake_setup() changes directories to the top of the build tree instead.

=item perl_dirs([ARGS])

Returns a list of directories that may contain Perl scripts that should be
tested by test scripts that test all Perl in the source tree (such as syntax
or coding style checks).  The paths will be simple directory names relative to
the current directory or two-part directory names under the F<tests>
directory.  (Directories under F<tests> are broken out separately since it's
common to want to apply different policies to different subdirectories of
F<tests>.)

If ARGS is given, it should be a reference to a hash of configuration options.
Only one option is supported: C<skip>, whose value should be a reference to an
array of additional top-level directories or directories starting with
C<tests/> that should be skipped.

=item test_file_path(FILE)

Given FILE, which should be a relative path, locates that file relative to the
test directory in either the source or build tree.  FILE will be checked for
relative to the environment variable C_TAP_BUILD first, and then relative to
C_TAP_SOURCE.  test_file_path() returns the full path to FILE or calls
BAIL_OUT if FILE could not be found.

=item test_tmpdir()

Create a temporary directory for tests to use for transient files and return
the path to that directory.  The directory is created relative to the
C_TAP_BUILD environment variable, which must be set.  Permissions on the
directory are set using the current umask.  test_tmpdir() returns the full
path to the temporary directory or calls BAIL_OUT if it could not be created.

The directory is automatically removed if possible on program exit.  Failure
to remove the directory on exit is reported with diag() and otherwise ignored.

=back

=head1 ENVIRONMENT

=over 4

=item C_TAP_BUILD

The root of the tests directory in Automake build directory for this package,
used to find files as documented above.

=item C_TAP_SOURCE

The root of the tests directory in the source tree for this package, used to
find files as documented above.

=back

=head1 AUTHOR

Russ Allbery <eagle@eyrie.org>

=head1 COPYRIGHT AND LICENSE

Copyright 2014-2015, 2018-2021 Russ Allbery <eagle@eyrie.org>

Copyright 2013 The Board of Trustees of the Leland Stanford Junior University

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

Test::More(3), Test::RRA(3), Test::RRA::Config(3)

This module is maintained in the rra-c-util package.  The current version is
available from L<https://www.eyrie.org/~eagle/software/rra-c-util/>.

The C TAP Harness test driver and libraries for TAP-based C testing are
available from L<https://www.eyrie.org/~eagle/software/c-tap-harness/>.

=cut

# Local Variables:
# copyright-at-end-flag: t
# End:
