#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

eval 'opendir(NOSUCH, "no/such/directory");';
if ($@) { print "1..0\n"; exit; }

print "1..3\n";

for $i (1..2000) {
    local *OP;
    opendir(OP, "op") or die "can't opendir: $!";
    # should auto-closedir() here
}

if (opendir(OP, "op")) { print "ok 1\n"; } else { print "not ok 1\n"; }
@D = grep(/^[^\.].*\.t$/i, readdir(OP));
closedir(OP);

##
## This range will have to adjust as the number of tests expands,
## as it's counting the number of .t files in src/t
##
if (@D > 90 && @D < 110) { print "ok 2\n"; } else { print "not ok 2\n"; }

@R = sort @D;
@G = sort <op/*.t>;
if ($G[0] =~ m#.*\](\w+\.t)#i) {
    # grep is to convert filespecs returned from glob under VMS to format
    # identical to that returned by readdir
    @G = grep(s#.*\](\w+\.t).*#op/$1#i,<op/*.t>);
}
while (@R && @G && "op/".$R[0] eq $G[0]) {
	shift(@R);
	shift(@G);
}
if (@R == 0 && @G == 0) { print "ok 3\n"; } else { print "not ok 3\n"; }
