#!./perl

print "1..70\n";

#
# @foo, @bar, and @ary are also used from tie-stdarray after tie-ing them
#

@ary = (1,2,3,4,5);
if (join('',@ary) eq '12345') {print "ok 1\n";} else {print "not ok 1\n";}

$tmp = $ary[$#ary]; --$#ary;
if ($tmp == 5) {print "ok 2\n";} else {print "not ok 2\n";}
if ($#ary == 3) {print "ok 3\n";} else {print "not ok 3\n";}
if (join('',@ary) eq '1234') {print "ok 4\n";} else {print "not ok 4\n";}

$[ = 1;
@ary = (1,2,3,4,5);
if (join('',@ary) eq '12345') {print "ok 5\n";} else {print "not ok 5\n";}

$tmp = $ary[$#ary]; --$#ary;
if ($tmp == 5) {print "ok 6\n";} else {print "not ok 6\n";}
if ($#ary == 4) {print "ok 7\n";} else {print "not ok 7\n";}
if (join('',@ary) eq '1234') {print "ok 8\n";} else {print "not ok 8\n";}

if ($ary[5] eq '') {print "ok 9\n";} else {print "not ok 9\n";}

$#ary += 1;	# see if element 5 gone for good
if ($#ary == 5) {print "ok 10\n";} else {print "not ok 10\n";}
if (defined $ary[5]) {print "not ok 11\n";} else {print "ok 11\n";}

$[ = 0;
@foo = ();
$r = join(',', $#foo, @foo);
if ($r eq "-1") {print "ok 12\n";} else {print "not ok 12 $r\n";}
$foo[0] = '0';
$r = join(',', $#foo, @foo);
if ($r eq "0,0") {print "ok 13\n";} else {print "not ok 13 $r\n";}
$foo[2] = '2';
$r = join(',', $#foo, @foo);
if ($r eq "2,0,,2") {print "ok 14\n";} else {print "not ok 14 $r\n";}
@bar = ();
$bar[0] = '0';
$bar[1] = '1';
$r = join(',', $#bar, @bar);
if ($r eq "1,0,1") {print "ok 15\n";} else {print "not ok 15 $r\n";}
@bar = ();
$r = join(',', $#bar, @bar);
if ($r eq "-1") {print "ok 16\n";} else {print "not ok 16 $r\n";}
$bar[0] = '0';
$r = join(',', $#bar, @bar);
if ($r eq "0,0") {print "ok 17\n";} else {print "not ok 17 $r\n";}
$bar[2] = '2';
$r = join(',', $#bar, @bar);
if ($r eq "2,0,,2") {print "ok 18\n";} else {print "not ok 18 $r\n";}
reset 'b';
@bar = ();
$bar[0] = '0';
$r = join(',', $#bar, @bar);
if ($r eq "0,0") {print "ok 19\n";} else {print "not ok 19 $r\n";}
$bar[2] = '2';
$r = join(',', $#bar, @bar);
if ($r eq "2,0,,2") {print "ok 20\n";} else {print "not ok 20 $r\n";}

$foo = 'now is the time';
if (($F1,$F2,$Etc) = ($foo =~ /^(\S+)\s+(\S+)\s*(.*)/)) {
    if ($F1 eq 'now' && $F2 eq 'is' && $Etc eq 'the time') {
	print "ok 21\n";
    }
    else {
	print "not ok 21\n";
    }
}
else {
    print "not ok 21\n";
}

$foo = 'lskjdf';
if ($cnt = (($F1,$F2,$Etc) = ($foo =~ /^(\S+)\s+(\S+)\s*(.*)/))) {
    print "not ok 22 $cnt $F1:$F2:$Etc\n";
}
else {
    print "ok 22\n";
}

%foo = ('blurfl','dyick','foo','bar','etc.','etc.');
%bar = %foo;
print $bar{'foo'} eq 'bar' ? "ok 23\n" : "not ok 23\n";
%bar = ();
print $bar{'foo'} eq '' ? "ok 24\n" : "not ok 24\n";
(%bar,$a,$b) = (%foo,'how','now');
print $bar{'foo'} eq 'bar' ? "ok 25\n" : "not ok 25\n";
print $bar{'how'} eq 'now' ? "ok 26\n" : "not ok 26\n";
@bar{keys %foo} = values %foo;
print $bar{'foo'} eq 'bar' ? "ok 27\n" : "not ok 27\n";
print $bar{'how'} eq 'now' ? "ok 28\n" : "not ok 28\n";

@foo = grep(/e/,split(' ','now is the time for all good men to come to'));
print join(' ',@foo) eq 'the time men come' ? "ok 29\n" : "not ok 29\n";

@foo = grep(!/e/,split(' ','now is the time for all good men to come to'));
print join(' ',@foo) eq 'now is for all good to to' ? "ok 30\n" : "not ok 30\n";

$foo = join('',('a','b','c','d','e','f')[0..5]);
print $foo eq 'abcdef' ? "ok 31\n" : "not ok 31\n";

$foo = join('',('a','b','c','d','e','f')[0..1]);
print $foo eq 'ab' ? "ok 32\n" : "not ok 32\n";

$foo = join('',('a','b','c','d','e','f')[6]);
print $foo eq '' ? "ok 33\n" : "not ok 33\n";

@foo = ('a','b','c','d','e','f')[0,2,4];
@bar = ('a','b','c','d','e','f')[1,3,5];
$foo = join('',(@foo,@bar)[0..5]);
print $foo eq 'acebdf' ? "ok 34\n" : "not ok 34\n";

$foo = ('a','b','c','d','e','f')[0,2,4];
print $foo eq 'e' ? "ok 35\n" : "not ok 35\n";

$foo = ('a','b','c','d','e','f')[1];
print $foo eq 'b' ? "ok 36\n" : "not ok 36\n";

@foo = ( 'foo', 'bar', 'burbl');
push(foo, 'blah');
print $#foo == 3 ? "ok 37\n" : "not ok 37\n";

# various AASSIGN_COMMON checks (see newASSIGNOP() in op.c)

$test = 37;
sub t { ++$test; print "not " unless $_[0]; print "ok $test\n"; }

@foo = @foo;
t("@foo" eq "foo bar burbl blah");				# 38

(undef,@foo) = @foo;
t("@foo" eq "bar burbl blah");					# 39

@foo = ('XXX',@foo, 'YYY');
t("@foo" eq "XXX bar burbl blah YYY");				# 40

@foo = @foo = qw(foo b\a\r bu\\rbl blah);
t("@foo" eq 'foo b\a\r bu\\rbl blah');				# 41

@bar = @foo = qw(foo bar);					# 42
t("@foo" eq "foo bar");
t("@bar" eq "foo bar");						# 43

# try the same with local
# XXX tie-stdarray fails the tests involving local, so we use
# different variable names to escape the 'tie'

@bee = ( 'foo', 'bar', 'burbl', 'blah');
{

    local @bee = @bee;
    t("@bee" eq "foo bar burbl blah");				# 44
    {
	local (undef,@bee) = @bee;
	t("@bee" eq "bar burbl blah");				# 45
	{
	    local @bee = ('XXX',@bee,'YYY');
	    t("@bee" eq "XXX bar burbl blah YYY");		# 46
	    {
		local @bee = local(@bee) = qw(foo bar burbl blah);
		t("@bee" eq "foo bar burbl blah");		# 47
		{
		    local (@bim) = local(@bee) = qw(foo bar);
		    t("@bee" eq "foo bar");			# 48
		    t("@bim" eq "foo bar");			# 49
		}
		t("@bee" eq "foo bar burbl blah");		# 50
	    }
	    t("@bee" eq "XXX bar burbl blah YYY");		# 51
	}
	t("@bee" eq "bar burbl blah");				# 52
    }
    t("@bee" eq "foo bar burbl blah");				# 53
}

# try the same with my
{

    my @bee = @bee;
    t("@bee" eq "foo bar burbl blah");				# 54
    {
	my (undef,@bee) = @bee;
	t("@bee" eq "bar burbl blah");				# 55
	{
	    my @bee = ('XXX',@bee,'YYY');
	    t("@bee" eq "XXX bar burbl blah YYY");		# 56
	    {
		my @bee = my @bee = qw(foo bar burbl blah);
		t("@bee" eq "foo bar burbl blah");		# 57
		{
		    my (@bim) = my(@bee) = qw(foo bar);
		    t("@bee" eq "foo bar");			# 58
		    t("@bim" eq "foo bar");			# 59
		}
		t("@bee" eq "foo bar burbl blah");		# 60
	    }
	    t("@bee" eq "XXX bar burbl blah YYY");		# 61
	}
	t("@bee" eq "bar burbl blah");				# 62
    }
    t("@bee" eq "foo bar burbl blah");				# 63
}

# make sure reification behaves
my $t = 63;
sub reify { $_[1] = ++$t; print "@_\n"; }
reify('ok');
reify('ok');

# qw() is no more a runtime split, it's compiletime.
print "not " unless qw(foo bar snorfle)[2] eq 'snorfle';
print "ok 66\n";

@ary = (12,23,34,45,56);

print "not " unless shift(@ary) == 12;
print "ok 67\n";

print "not " unless pop(@ary) == 56;
print "ok 68\n";

print "not " unless push(@ary,56) == 4;
print "ok 69\n";

print "not " unless unshift(@ary,12) == 5;
print "ok 70\n";
