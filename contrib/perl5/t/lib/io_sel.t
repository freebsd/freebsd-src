#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
}

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

print "1..23\n";

use IO::Select 1.09;

my $sel = new IO::Select(\*STDIN);
$sel->add(4, 5) == 2 or print "not ";
print "ok 1\n";

$sel->add([\*STDOUT, 'foo']) == 1 or print "not ";
print "ok 2\n";

@handles = $sel->handles;
print "not " unless $sel->count == 4 && @handles == 4;
print "ok 3\n";
#print $sel->as_string, "\n";

$sel->remove(\*STDIN) == 1 or print "not ";
print "ok 4\n",
;
$sel->remove(\*STDIN, 5, 6) == 1  # two of there are not present
  or print "not ";
print "ok 5\n";

print "not " unless $sel->count == 2;
print "ok 6\n";
#print $sel->as_string, "\n";

$sel->remove(1, 4);
print "not " unless $sel->count == 0 && !defined($sel->bits);
print "ok 7\n";

$sel = new IO::Select;
print "not " unless $sel->count == 0 && !defined($sel->bits);
print "ok 8\n";

$sel->remove([\*STDOUT, 5]);
print "not " unless $sel->count == 0 && !defined($sel->bits);
print "ok 9\n";

if ($^O eq 'MSWin32' || $^O eq 'dos') {  # 4-arg select is only valid on sockets
    print "# skipping tests 10..15\n";
    for (10 .. 15) { print "ok $_\n" }
    $sel->add(\*STDOUT);  # update
    goto POST_SOCKET;
}

@a = $sel->can_read();  # should return imediately
print "not " unless @a == 0;
print "ok 10\n";

# we assume that we can write to STDOUT :-)
$sel->add([\*STDOUT, "ok 12\n"]);

@a = $sel->can_write;
print "not " unless @a == 1;
print "ok 11\n";

my($fd, $msg) = @{shift @a};
print $fd $msg;

$sel->add(\*STDOUT);  # update

@a = IO::Select::select(undef, $sel, undef, 1);
print "not " unless @a == 3;
print "ok 13\n";

($r, $w, $e) = @a;

print "not " unless @$r == 0 && @$w == 1 && @$e == 0;
print "ok 14\n";

$fd = $w->[0];
print $fd "ok 15\n";

POST_SOCKET:
# Test new exists() method
$sel->exists(\*STDIN) and print "not ";
print "ok 16\n";

($sel->exists(0) || $sel->exists([\*STDERR])) and print "not ";
print "ok 17\n";

$fd = $sel->exists(\*STDOUT);
if ($fd) {
    print $fd "ok 18\n";
} else {
    print "not ok 18\n";
}

$fd = $sel->exists([1, 'foo']);
if ($fd) {
    print $fd "ok 19\n";
} else {
    print "not ok 19\n";
}

# Try self clearing
$sel->add(5,6,7,8,9,10);
print "not " unless $sel->count == 7;
print "ok 20\n";

$sel->remove($sel->handles);
print "not " unless $sel->count == 0 && !defined($sel->bits);
print "ok 21\n";

# check warnings
$SIG{__WARN__} = sub { 
    ++ $w 
      if $_[0] =~ /^Call to depreciated method 'has_error', use 'has_exception'/ 
    } ;
$w = 0 ;
IO::Select::has_error();
print "not " unless $w == 0 ;
$w = 0 ;
print "ok 22\n" ;
use warnings 'IO::Select' ;
IO::Select::has_error();
print "not " unless $w == 1 ;
$w = 0 ;
print "ok 23\n" ;
