#!./perl
# Test $!

print "1..14\n";

$teststring = "1\n12\n123\n1234\n1234\n12345\n\n123456\n1234567\n";

# Create our test datafile
open TESTFILE, ">./foo" or die "error $! $^E opening";
binmode TESTFILE;
print TESTFILE $teststring;
close TESTFILE;

open TESTFILE, "<./foo";
binmode TESTFILE;

# Check the default $/
$bar = <TESTFILE>;
if ($bar eq "1\n") {print "ok 1\n";} else {print "not ok 1\n";}

# explicitly set to \n
$/ = "\n";
$bar = <TESTFILE>;
if ($bar eq "12\n") {print "ok 2\n";} else {print "not ok 2\n";}

# Try a non line terminator
$/ = "3";
$bar = <TESTFILE>;
if ($bar eq "123") {print "ok 3\n";} else {print "not ok 3\n";}

# Eat the line terminator
$/ = "\n";
$bar = <TESTFILE>;

# How about a larger terminator
$/ = "34";
$bar = <TESTFILE>;
if ($bar eq "1234") {print "ok 4\n";} else {print "not ok 4\n";}

# Eat the line terminator
$/ = "\n";
$bar = <TESTFILE>;

# Does paragraph mode work?
$/ = '';
$bar = <TESTFILE>;
if ($bar eq "1234\n12345\n\n") {print "ok 5\n";} else {print "not ok 5\n";}

# Try slurping the rest of the file
$/ = undef;
$bar = <TESTFILE>;
if ($bar eq "123456\n1234567\n") {print "ok 6\n";} else {print "not ok 6\n";}

# try the record reading tests. New file so we don't have to worry about
# the size of \n.
close TESTFILE;
unlink "./foo";
open TESTFILE, ">./foo";
print TESTFILE "1234567890123456789012345678901234567890";
binmode TESTFILE;
close TESTFILE;
open TESTFILE, "<./foo";
binmode TESTFILE;

# Test straight number
$/ = \2;
$bar = <TESTFILE>;
if ($bar eq "12") {print "ok 7\n";} else {print "not ok 7\n";}

# Test stringified number
$/ = \"2";
$bar = <TESTFILE>;
if ($bar eq "34") {print "ok 8\n";} else {print "not ok 8\n";}

# Integer variable
$foo = 2;
$/ = \$foo;
$bar = <TESTFILE>;
if ($bar eq "56") {print "ok 9\n";} else {print "not ok 9\n";}

# String variable
$foo = "2";
$/ = \$foo;
$bar = <TESTFILE>;
if ($bar eq "78") {print "ok 10\n";} else {print "not ok 10\n";}

# Get rid of the temp file
close TESTFILE;
unlink "./foo";

# Now for the tricky bit--full record reading
if ($^O eq 'VMS') {
  # Create a temp file. We jump through these hoops 'cause CREATE really
  # doesn't like our methods for some reason.
  open FDLFILE, "> ./foo.fdl";
  print FDLFILE "RECORD\n FORMAT VARIABLE\n";
  close FDLFILE;
  open CREATEFILE, "> ./foo.com";
  print CREATEFILE '$ DEFINE/USER SYS$INPUT NL:', "\n";
  print CREATEFILE '$ DEFINE/USER SYS$OUTPUT NL:', "\n";
  print CREATEFILE '$ OPEN YOW []FOO.BAR/WRITE', "\n";
  print CREATEFILE '$ CLOSE YOW', "\n";
  print CREATEFILE "\$EXIT\n";
  close CREATEFILE;
  $throwaway = `\@\[\]foo`, "\n";
  open(TEMPFILE, ">./foo.bar") or print "# open failed $! $^E\n";
  print TEMPFILE "foo\nfoobar\nbaz\n";
  close TEMPFILE;

  open TESTFILE, "<./foo.bar";
  $/ = \10;
  $bar = <TESTFILE>;
  if ($bar eq "foo\n") {print "ok 11\n";} else {print "not ok 11\n";}
  $bar = <TESTFILE>;
  if ($bar eq "foobar\n") {print "ok 12\n";} else {print "not ok 12\n";}
  # can we do a short read?
  $/ = \2;
  $bar = <TESTFILE>;
  if ($bar eq "ba") {print "ok 13\n";} else {print "not ok 13\n";}
  # do we get the rest of the record?
  $bar = <TESTFILE>;
  if ($bar eq "z\n") {print "ok 14\n";} else {print "not ok 14\n";}

  close TESTFILE;
  unlink "./foo.bar";
  unlink "./foo.com";  
} else {
  # Nobody else does this at the moment (well, maybe OS/390, but they can
  # put their own tests in) so we just punt
  foreach $test (11..14) {print "ok $test # skipped on non-VMS system\n"};
}
