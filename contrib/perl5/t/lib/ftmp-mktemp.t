#!/usr/bin/perl -w

# Test for mktemp family of commands in File::Temp
# Use STANDARD safe level for these tests

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Test; import Test;
	plan(tests => 9);
}

use strict;

use File::Spec;
use File::Path;
use File::Temp qw/ :mktemp unlink0 /;

ok(1);

# MKSTEMP - test

# Create file in temp directory
my $template = File::Spec->catfile(File::Spec->tmpdir, 'wowserXXXX');

(my $fh, $template) = mkstemp($template);

print "# MKSTEMP: FH is $fh File is $template fileno=".fileno($fh)."\n";
# Check if the file exists
ok( (-e $template) );

# Autoflush
$fh->autoflush(1) if $] >= 5.006;

# Try printing something to the file
my $string = "woohoo\n";
print $fh $string;

# rewind the file
ok(seek( $fh, 0, 0));

# Read from the file
my $line = <$fh>;

# compare with previous string
ok($string, $line);

# Tidy up
# This test fails on Windows NT since it seems that the size returned by 
# stat(filehandle) does not always equal the size of the stat(filename)
# This must be due to caching. In particular this test writes 7 bytes
# to the file which are not recognised by stat(filename)
# Simply waiting 3 seconds seems to be enough for the system to update

if ($^O eq 'MSWin32') {
  sleep 3;
}
my $status = unlink0($fh, $template);
if ($status) {
  ok( $status );
} else {
  skip("Skip test failed probably due to \$TMPDIR being on NFS",1);
}

# MKSTEMPS
# File with suffix. This is created in the current directory so
# may be problematic on NFS

$template = "suffixXXXXXX";
my $suffix = ".dat";

($fh, my $fname) = mkstemps($template, $suffix);

print "# MKSTEMPS: File is $template -> $fname fileno=".fileno($fh)."\n";
# Check if the file exists
ok( (-e $fname) );

# This fails if you are running on NFS
# If this test fails simply skip it rather than doing a hard failure
$status = unlink0($fh, $fname);

if ($status) {
  ok($status);
} else {
  skip("Skip test failed probably due to cwd being on NFS",1)
}

# MKDTEMP
# Temp directory

$template = File::Spec->catdir(File::Spec->tmpdir, 'tmpdirXXXXXX');

my $tmpdir = mkdtemp($template);

print "# MKDTEMP: Name is $tmpdir from template $template\n";

ok( (-d $tmpdir ) );

# Need to tidy up after myself
rmtree($tmpdir);

# MKTEMP
# Just a filename, not opened

$template = File::Spec->catfile(File::Spec->tmpdir, 'mytestXXXXXX');

my $tmpfile = mktemp($template);

print "# MKTEMP: Tempfile is $template -> $tmpfile\n";

# Okay if template no longer has XXXXX in


ok( ($tmpfile !~ /XXXXX$/) );
