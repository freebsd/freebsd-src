#!./perl

print "1..39\n";

chdir('op') || die "sysio.t: cannot look for myself: $!";

open(I, 'sysio.t') || die "sysio.t: cannot find myself: $!";

$reopen = ($^O eq 'VMS' || $^O eq 'os2' || $^O eq 'MSWin32' || $^O eq 'dos' ||
	   $^O eq 'mpeix');

$x = 'abc';

# should not be able to do negative lengths
eval { sysread(I, $x, -1) };
print 'not ' unless ($@ =~ /^Negative length /);
print "ok 1\n";

# $x should be intact
print 'not ' unless ($x eq 'abc');
print "ok 2\n";

# should not be able to read before the buffer
eval { sysread(I, $x, 1, -4) };
print 'not ' unless ($x eq 'abc');
print "ok 3\n";

# $x should be intact
print 'not ' unless ($x eq 'abc');
print "ok 4\n";

$a ='0123456789';

# default offset 0
print 'not ' unless(sysread(I, $a, 3) == 3);
print "ok 5\n";

# $a should be as follows
print 'not ' unless ($a eq '#!.');
print "ok 6\n";

# reading past the buffer should zero pad
print 'not ' unless(sysread(I, $a, 2, 5) == 2);
print "ok 7\n";

# the zero pad should be seen now
print 'not ' unless ($a eq "#!.\0\0/p");
print "ok 8\n";

# try changing the last two characters of $a
print 'not ' unless(sysread(I, $a, 3, -2) == 3);
print "ok 9\n";

# the last two characters of $a should have changed (into three)
print 'not ' unless ($a eq "#!.\0\0erl");
print "ok 10\n";

$outfile = 'sysio.out';

open(O, ">$outfile") || die "sysio.t: cannot write $outfile: $!";

select(O); $|=1; select(STDOUT);

# cannot write negative lengths
eval { syswrite(O, $x, -1) };
print 'not ' unless ($@ =~ /^Negative length /);
print "ok 11\n";

# $x still intact
print 'not ' unless ($x eq 'abc');
print "ok 12\n";

# $outfile still intact
print 'not ' if (-s $outfile);
print "ok 13\n";

# should not be able to write from after the buffer
eval { syswrite(O, $x, 1, 3) };
print 'not ' unless ($@ =~ /^Offset outside string /);
print "ok 14\n";

# $x still intact
print 'not ' unless ($x eq 'abc');
print "ok 15\n";

# $outfile still intact
if ($reopen) {  # must close file to update EOF marker for stat
  close O; open(O, ">>$outfile") || die "sysio.t: cannot write $outfile: $!";
}
print 'not ' if (-s $outfile);
print "ok 16\n";

# should not be able to write from before the buffer

eval { syswrite(O, $x, 1, -4) };
print 'not ' unless ($@ =~ /^Offset outside string /);
print "ok 17\n";

# $x still intact
print 'not ' unless ($x eq 'abc');
print "ok 18\n";

# $outfile still intact
if ($reopen) {  # must close file to update EOF marker for stat
  close O; open(O, ">>$outfile") || die "sysio.t: cannot write $outfile: $!";
}
print 'not ' if (-s $outfile);
print "ok 19\n";

# default offset 0
print 'not ' unless (syswrite(O, $a, 2) == 2);
print "ok 20\n";

# $a still intact
print 'not ' unless ($a eq "#!.\0\0erl");
print "ok 21\n";

# $outfile should have grown now
if ($reopen) {  # must close file to update EOF marker for stat
  close O; open(O, ">>$outfile") || die "sysio.t: cannot write $outfile: $!";
}
print 'not ' unless (-s $outfile == 2);
print "ok 22\n";

# with offset
print 'not ' unless (syswrite(O, $a, 2, 5) == 2);
print "ok 23\n";

# $a still intact
print 'not ' unless ($a eq "#!.\0\0erl");
print "ok 24\n";

# $outfile should have grown now
if ($reopen) {  # must close file to update EOF marker for stat
  close O; open(O, ">>$outfile") || die "sysio.t: cannot write $outfile: $!";
}
print 'not ' unless (-s $outfile == 4);
print "ok 25\n";

# with negative offset and a bit too much length
print 'not ' unless (syswrite(O, $a, 5, -3) == 3);
print "ok 26\n";

# $a still intact
print 'not ' unless ($a eq "#!.\0\0erl");
print "ok 27\n";

# $outfile should have grown now
if ($reopen) {  # must close file to update EOF marker for stat
  close O; open(O, ">>$outfile") || die "sysio.t: cannot write $outfile: $!";
}
print 'not ' unless (-s $outfile == 7);
print "ok 28\n";

# with implicit length argument
print 'not ' unless (syswrite(O, $x) == 3);
print "ok 29\n";

# $a still intact
print 'not ' unless ($x eq "abc");
print "ok 30\n";

# $outfile should have grown now
if ($reopen) {  # must close file to update EOF marker for stat
  close O; open(O, ">>$outfile") || die "sysio.t: cannot write $outfile: $!";
}
print 'not ' unless (-s $outfile == 10);
print "ok 31\n";

close(O);

open(I, $outfile) || die "sysio.t: cannot read $outfile: $!";

$b = 'xyz';

# reading too much only return as much as available
print 'not ' unless (sysread(I, $b, 100) == 10);
print "ok 32\n";
# this we should have
print 'not ' unless ($b eq '#!ererlabc');
print "ok 33\n";

# test sysseek

print 'not ' unless sysseek(I, 2, 0) == 2;
print "ok 34\n";
sysread(I, $b, 3);
print 'not ' unless $b eq 'ere';
print "ok 35\n";

print 'not ' unless sysseek(I, -2, 1) == 3;
print "ok 36\n";
sysread(I, $b, 4);
print 'not ' unless $b eq 'rerl';
print "ok 37\n";

print 'not ' unless sysseek(I, 0, 0) eq '0 but true';
print "ok 38\n";
print 'not ' if defined sysseek(I, -1, 1);
print "ok 39\n";

close(I);

unlink $outfile;

chdir('..'); 

1;

# eof
