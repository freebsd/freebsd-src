#!./perl

print "1..6\n";

open(try, '>Io.argv.tmp') || (die "Can't open temp file: $!");
print try "a line\n";
close try;

if ($^O eq 'MSWin32') {
  $x = `.\\perl -e "while (<>) {print \$.,\$_;}" Io.argv.tmp Io.argv.tmp`;
}
else {
  $x = `./perl -e 'while (<>) {print \$.,\$_;}' Io.argv.tmp Io.argv.tmp`;
}
if ($x eq "1a line\n2a line\n") {print "ok 1\n";} else {print "not ok 1\n";}

if ($^O eq 'MSWin32') {
  $x = `.\\perl -le "print 'foo'" | .\\perl -e "while (<>) {print \$_;}" Io.argv.tmp -`;
}
else {
  $x = `echo foo|./perl -e 'while (<>) {print $_;}' Io.argv.tmp -`;
}
if ($x eq "a line\nfoo\n") {print "ok 2\n";} else {print "not ok 2\n";}

if ($^O eq 'MSWin32') {
  $x = `.\\perl -le "print 'foo'" |.\\perl -e "while (<>) {print \$_;}"`;
}
else {
  $x = `echo foo|./perl -e 'while (<>) {print $_;}'`;
}
if ($x eq "foo\n") {print "ok 3\n";} else {print "not ok 3 :$x:\n";}

@ARGV = ('Io.argv.tmp', 'Io.argv.tmp', '/dev/null', 'Io.argv.tmp');
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

open(try, '>Io.argv.tmp') or die "Can't open temp file: $!";
close try;
@ARGV = 'Io.argv.tmp';
$^I = '.bak';
$/ = undef;
while (<>) {
    s/^/ok 6\n/;
    print;
}
open(try, '<Io.argv.tmp') or die "Can't open temp file: $!";
print while <try>;
close try;

END { unlink 'Io.argv.tmp', 'Io.argv.tmp.bak' }
