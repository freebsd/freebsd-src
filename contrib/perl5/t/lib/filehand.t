#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bIO\b/ && $^O ne 'VMS') {
	print "1..0\n";
	exit 0;
    }
}

use FileHandle;
use strict subs;

autoflush STDOUT 1;

$mystdout = new_from_fd FileHandle 1,"w";
$| = 1;
autoflush $mystdout;
print "1..11\n";

print $mystdout "ok ",fileno($mystdout),"\n";

$fh = (new FileHandle "./TEST", O_RDONLY
       or new FileHandle "TEST", O_RDONLY)
  and print "ok 2\n";


$buffer = <$fh>;
print $buffer eq "#!./perl\n" ? "ok 3\n" : "not ok 3\n";


ungetc $fh ord 'A';
CORE::read($fh, $buf,1);
print $buf eq 'A' ? "ok 4\n" : "not ok 4\n";

close $fh;

$fh = new FileHandle;

print "not " unless ($fh->open("< TEST") && <$fh> eq $buffer);
print "ok 5\n";

$fh->seek(0,0);
print "#possible mixed CRLF/LF in t/TEST\nnot " unless (<$fh> eq $buffer);
print "ok 6\n";

$fh->seek(0,2);
$line = <$fh>;
print "not " if (defined($line) || !$fh->eof);
print "ok 7\n";

print "not " unless ($fh->open("TEST","r") && !$fh->tell && $fh->close);
print "ok 8\n";

autoflush STDOUT 0;

print "not " if ($|);
print "ok 9\n";

autoflush STDOUT 1;

print "not " unless ($|);
print "ok 10\n";

if ($^O eq 'dos')
{
    printf("ok %d\n",11);
    exit(0);
}

($rd,$wr) = FileHandle::pipe;

if ($^O eq 'VMS' || $^O eq 'os2' || $^O eq 'amigaos' || $^O eq 'MSWin32') {
  $wr->autoflush;
  $wr->printf("ok %d\n",11);
  print $rd->getline;
}
else {
  if (fork) {
   $wr->close;
   print $rd->getline;
  }
  else {
   $rd->close;
   $wr->printf("ok %d\n",11);
   exit(0);
  }
}
