#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..9\n";

sub t1;
sub t2 : locked;
sub t3 ();
sub t4 ($);
sub t5 {1;}
{
    package P1;
    sub tmc {1;}
    package P2;
    @ISA = 'P1';
}

print "not " unless exists &t1 && not defined &t1;
print "ok 1\n";
print "not " unless exists &t2 && not defined &t2;
print "ok 2\n";
print "not " unless exists &t3 && not defined &t3;
print "ok 3\n";
print "not " unless exists &t4 && not defined &t4;
print "ok 4\n";
print "not " unless exists &t5 && defined &t5;
print "ok 5\n";
P2::->tmc;
print "not " unless not exists &P2::tmc && not defined &P2::tmc;
print "ok 6\n";
my $ref;
$ref->{A}[0] = \&t4;
print "not " unless exists &{$ref->{A}[0]} && not defined &{$ref->{A}[0]};
print "ok 7\n";
undef &P1::tmc;
print "not " unless exists &P1::tmc && not defined &P1::tmc;
print "ok 8\n";
eval 'exists &t5()';
print "not " unless $@;
print "ok 9\n";

exit 0;
