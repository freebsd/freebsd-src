#!/usr/local/bin/perl -w
# Test for File::Temp - tempfile function

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Test; import Test;
	plan(tests => 20);
}

use strict;
use File::Spec;

# Will need to check that all files were unlinked correctly
# Set up an END block here to do it

# Arrays containing list of dirs/files to test
my (@files, @dirs, @still_there);

# And a test for files that should still be around
# These are tidied up
END {
  foreach (@still_there) {
    ok( -f $_ );
    ok( unlink( $_ ) );
    ok( !(-f $_) );
  }
}

# Loop over an array hoping that the files dont exist
END { foreach (@files) { ok( !(-e $_) )} }

# And a test for directories
END { foreach (@dirs)  { ok( !(-d $_) )} }

# Need to make sure that the END blocks are setup before
# the ones that File::Temp configures since END blocks are evaluated
# in revers order and we need to check the files *after* File::Temp
# removes them
use File::Temp qw/ tempfile tempdir/;

# Now we start the tests properly
ok(1);


# Tempfile
# Open tempfile in some directory, unlink at end
my ($fh, $tempfile) = tempfile(
			       UNLINK => 1,
			       SUFFIX => '.txt',
			      );

ok( (-f $tempfile) );
# Should still be around after closing
ok( close( $fh ) ); 
ok( (-f $tempfile) );
# Check again at exit
push(@files, $tempfile);

# TEMPDIR test
# Create temp directory in current dir
my $template = 'tmpdirXXXXXX';
print "# Template: $template\n";
my $tempdir = tempdir( $template ,
		       DIR => File::Spec->curdir,
		       CLEANUP => 1,
		     );

print "# TEMPDIR: $tempdir\n";

ok( (-d $tempdir) );
push(@dirs, $tempdir);

# Create file in the temp dir
($fh, $tempfile) = tempfile(
			    DIR => $tempdir,
			    UNLINK => 1,
			    SUFFIX => '.dat',
			   );

print "# TEMPFILE: Created $tempfile\n";

ok( (-f $tempfile));
push(@files, $tempfile);

# Test tempfile
# ..and again
($fh, $tempfile) = tempfile(
			    DIR => $tempdir,
			   );


ok( (-f $tempfile ));
push(@files, $tempfile);

print "# TEMPFILE: Created $tempfile\n";

# and another (with template)

($fh, $tempfile) = tempfile( 'helloXXXXXXX',
			    DIR => $tempdir,
			    UNLINK => 1,
			    SUFFIX => '.dat',
			   );

print "# TEMPFILE: Created $tempfile\n";

ok( (-f $tempfile) );
push(@files, $tempfile);


# Create a temporary file that should stay around after
# it has been closed
($fh, $tempfile) = tempfile( 'permXXXXXXX', UNLINK => 0 );
print "# TEMPFILE: Created $tempfile\n";
ok( -f $tempfile );
ok( close( $fh ) );
push( @still_there, $tempfile); # check at END

# Would like to create a temp file and just retrieve the handle
# but the test is problematic since:
#  - We dont know the filename so we cant check that it is tidied
#    correctly
#  - The unlink0 required on unix for tempfile creation will fail
#    on NFS
# Try to do what we can.
# Tempfile croaks on error so we need an eval
$fh = eval { tempfile( 'ftmpXXXXX', DIR => File::Spec->tmpdir ) };

if ($fh) {

  # print something to it to make sure something is there
  ok( print $fh "Test\n" );

  # Close it - can not check it is gone since we dont know the name
  ok( close($fh) );

} else {
  skip "Skip Failed probably due to NFS", 1;
  skip "Skip Failed probably due to NFS", 1;
}

# Now END block will execute to test the removal of directories
print "# End of tests. Execute END blocks\n";

