#!./perl

eval 'opendir(NOSUCH, "no/such/directory");';
if ($@) { print "1..0\n"; exit; }

print "1..3\n";

if (opendir(OP, "op")) { print "ok 1\n"; } else { print "not ok 1\n"; }
@D = grep(/^[^\.].*\.t$/, readdir(OP));
closedir(OP);

if (@D > 20 && @D < 100) { print "ok 2\n"; } else { print "not ok 2\n"; }

@R = sort @D;
@G = <op/*.t>;
while (@R && @G && "op/".$R[0] eq $G[0]) {
	shift(@R);
	shift(@G);
}
if (@R == 0 && @G == 0) { print "ok 3\n"; } else { print "not ok 3\n"; }
