#!/usr/bin/perl -w
# Test for File::Temp - POSIX functions

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Test; import Test;
	plan(tests => 7);
}

use strict;

use File::Temp qw/ :POSIX unlink0 /;
ok(1);

# TMPNAM - scalar

print "# TMPNAM: in a scalar context: \n";
my $tmpnam = tmpnam();

# simply check that the file does not exist
# Not a 100% water tight test though if another program 
# has managed to create one in the meantime.
ok( !(-e $tmpnam ));

print "# TMPNAM file name: $tmpnam\n";

# TMPNAM list context
# Not strict posix behaviour
(my $fh, $tmpnam) = tmpnam();

print "# TMPNAM: in list context: $fh $tmpnam\n";

# File is opened - make sure it exists
ok( (-e $tmpnam ));

# Unlink it - a possible NFS issue again if TMPDIR is not a local disk
my $status = unlink0($fh, $tmpnam);
if ($status) {
  ok( $status );
} else {
  skip("Skip test failed probably due to \$TMPDIR being on NFS",1);
}

# TMPFILE

$fh = tmpfile();

if (defined $fh) {
  ok( $fh );
  print "# TMPFILE: tmpfile got FH $fh\n";

  $fh->autoflush(1) if $] >= 5.006;

  # print something to it
  my $original = "Hello a test\n";
  print "# TMPFILE: Wrote line: $original";
  print $fh $original
    or die "Error printing to tempfile\n";

  # rewind it
  ok( seek($fh,0,0) );

  # Read from it
  my $line = <$fh>;

  print "# TMPFILE: Read line: $line";
  ok( $original, $line);

  close($fh);

} else {
  # Skip all the remaining tests
  foreach (1..3) {
    skip("Skip test failed probably due to \$TMPDIR being on NFS",1);
  }
}




