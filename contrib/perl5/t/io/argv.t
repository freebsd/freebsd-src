#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..21\n";

use File::Spec;

my $devnull = File::Spec->devnull;

open(try, '>Io_argv1.tmp') || (die "Can't open temp file: $!");
print try "a line\n";
close try;

if ($^O eq 'MSWin32') {
  $x = `.\\perl -e "while (<>) {print \$.,\$_;}" Io_argv1.tmp Io_argv1.tmp`;
}
else {
  $x = `./perl -e 'while (<>) {print \$.,\$_;}' Io_argv1.tmp Io_argv1.tmp`;
}
if ($x eq "1a line\n2a line\n") {print "ok 1\n";} else {print "not ok 1\n";}

if ($^O eq 'MSWin32') {
  $x = `.\\perl -le "print 'foo'" | .\\perl -e "while (<>) {print \$_;}" Io_argv1.tmp -`;
}
else {
  $x = `echo foo|./perl -e 'while (<>) {print $_;}' Io_argv1.tmp -`;
}
if ($x eq "a line\nfoo\n") {print "ok 2\n";} else {print "not ok 2\n";}

if ($^O eq 'MSWin32') {
  $x = `.\\perl -le "print 'foo'" |.\\perl -e "while (<>) {print \$_;}"`;
}
else {
  $x = `echo foo|./perl -e 'while (<>) {print $_;}'`;
}
if ($x eq "foo\n") {print "ok 3\n";} else {print "not ok 3 :$x:\n";}

@ARGV = ('Io_argv1.tmp', 'Io_argv1.tmp', $devnull, 'Io_argv1.tmp');
while (<>) {
    $y .= $. . $_;
    if (eof()) {
	if ($. == 3) {print "ok 4\n";} else {print "not ok 4\n";}
    }
}

if ($y eq "1a line\n2a line\n3a line\n")
    {print "ok 5\n";}
else
    {print "not ok 5\n";}

open(try, '>Io_argv1.tmp') or die "Can't open temp file: $!";
close try;
open(try, '>Io_argv2.tmp') or die "Can't open temp file: $!";
close try;
@ARGV = ('Io_argv1.tmp', 'Io_argv2.tmp');
$^I = '.bak';
$/ = undef;
my $i = 6;
while (<>) {
    s/^/ok $i\n/;
    ++$i;
    print;
}
open(try, '<Io_argv1.tmp') or die "Can't open temp file: $!";
print while <try>;
open(try, '<Io_argv2.tmp') or die "Can't open temp file: $!";
print while <try>;
close try;
undef $^I;

eof try or print 'not ';
print "ok 8\n";

eof NEVEROPENED or print 'not ';
print "ok 9\n";

open STDIN, 'Io_argv1.tmp' or die $!;
@ARGV = ();
!eof() or print 'not ';
print "ok 10\n";

<> eq "ok 6\n" or print 'not ';
print "ok 11\n";

open STDIN, $devnull or die $!;
@ARGV = ();
eof() or print 'not ';
print "ok 12\n";

@ARGV = ('Io_argv1.tmp');
!eof() or print 'not ';
print "ok 13\n";

@ARGV = ($devnull, $devnull);
!eof() or print 'not ';
print "ok 14\n";

close ARGV or die $!;
eof() or print 'not ';
print "ok 15\n";

{
    local $/;
    open F, 'Io_argv1.tmp' or die;
    <F>;	# set $. = 1
    print "not " if defined(<F>); # should hit eof
    print "ok 16\n";
    open F, $devnull or die;
    print "not " unless defined(<F>);
    print "ok 17\n";
    print "not " if defined(<F>);
    print "ok 18\n";
    print "not " if defined(<F>);
    print "ok 19\n";
    open F, $devnull or die;	# restart cycle again
    print "not " unless defined(<F>);
    print "ok 20\n";
    print "not " if defined(<F>);
    print "ok 21\n";
    close F;
}

END { unlink 'Io_argv1.tmp', 'Io_argv1.tmp.bak', 'Io_argv2.tmp', 'Io_argv2.tmp.bak' }
