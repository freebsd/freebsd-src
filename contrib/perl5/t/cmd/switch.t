#!./perl

# $RCSfile: switch.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:14 $

print "1..18\n";

sub foo1 {
    $_ = shift(@_);
    $a = 0;
    until ($a++) {
	next if $_ eq 1;
	next if $_ eq 2;
	next if $_ eq 3;
	next if $_ eq 4;
	return 20;
    }
    continue {
	return $_;
    }
}

print do foo1(0) == 20 ? "ok 1\n" : "not ok 1\n";
print do foo1(1) == 1 ? "ok 2\n" : "not ok 2\n";
print do foo1(2) == 2 ? "ok 3\n" : "not ok 3\n";
print do foo1(3) == 3 ? "ok 4\n" : "not ok 4\n";
print do foo1(4) == 4 ? "ok 5\n" : "not ok 5\n";
print do foo1(5) == 20 ? "ok 6\n" : "not ok 6\n";

sub foo2 {
    $_ = shift(@_);
    {
	last if $_ == 1;
	last if $_ == 2;
	last if $_ == 3;
	last if $_ == 4;
    }
    continue {
	return 20;
    }
    return $_;
}

print do foo2(0) == 20 ? "ok 7\n" : "not ok 7\n";
print do foo2(1) == 1 ? "ok 8\n" : "not ok 8\n";
print do foo2(2) == 2 ? "ok 9\n" : "not ok 9\n";
print do foo2(3) == 3 ? "ok 10\n" : "not ok 10\n";
print do foo2(4) == 4 ? "ok 11\n" : "not ok 11\n";
print do foo2(5) == 20 ? "ok 12\n" : "not ok 12\n";

sub foo3 {
    $_ = shift(@_);
    if (/^1/) {
	return 1;
    }
    elsif (/^2/) {
	return 2;
    }
    elsif (/^3/) {
	return 3;
    }
    elsif (/^4/) {
	return 4;
    }
    else {
	return 20;
    }
    return 40;
}

print do foo3(0) == 20 ? "ok 13\n" : "not ok 13\n";
print do foo3(1) == 1 ? "ok 14\n" : "not ok 14\n";
print do foo3(2) == 2 ? "ok 15\n" : "not ok 15\n";
print do foo3(3) == 3 ? "ok 16\n" : "not ok 16\n";
print do foo3(4) == 4 ? "ok 17\n" : "not ok 17\n";
print do foo3(5) == 20 ? "ok 18\n" : "not ok 18\n";
