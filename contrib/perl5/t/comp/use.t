#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..14\n";

my $i = 1;

eval "use 5.000;";
if ($@) {
    print STDERR $@,"\n";
    print "not ";
}
print "ok ",$i++,"\n";

eval sprintf "use %.5f;", $];
if ($@) {
    print STDERR $@,"\n";
    print "not ";
}
print "ok ",$i++,"\n";


eval sprintf "use %.5f;", $] - 0.000001;
if ($@) {
    print STDERR $@,"\n";
    print "not ";
}
print "ok ",$i++,"\n";

eval sprintf("use %.5f;", $] + 1);
unless ($@) {
    print "not ";
}
print "ok ",$i++,"\n";

eval sprintf "use %.5f;", $] + 0.00001;
unless ($@) {
    print "not ";
}
print "ok ",$i++,"\n";



use lib; # I know that this module will be there.


local $lib::VERSION = 1.0;

eval "use lib 0.9";
if ($@) {
    print STDERR $@,"\n";
    print "not ";
}
print "ok ",$i++,"\n";

eval "use lib 1.0";
if ($@) {
    print STDERR $@,"\n";
    print "not ";
}
print "ok ",$i++,"\n";

eval "use lib 1.01";
unless ($@) {
    print "not ";
}
print "ok ",$i++,"\n";


eval "use lib 0.9 qw(fred)";
if ($@) {
    print STDERR $@,"\n";
    print "not ";
}
print "ok ",$i++,"\n";

print "not " unless $INC[0] eq "fred";
print "ok ",$i++,"\n";

eval "use lib 1.0 qw(joe)";
if ($@) {
    print STDERR $@,"\n";
    print "not ";
}
print "ok ",$i++,"\n";

print "not " unless $INC[0] eq "joe";
print "ok ",$i++,"\n";

eval "use lib 1.01 qw(freda)";
unless ($@) {
    print "not ";
}
print "ok ",$i++,"\n";

print "not " if $INC[0] eq "freda";
print "ok ",$i++,"\n";
