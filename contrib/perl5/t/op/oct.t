#!./perl

print "1..50\n";

print +(oct('0b1_0101') ==        0b101_01) ? "ok" : "not ok", " 1\n";
print +(oct('0b10_101') ==           0_2_5) ? "ok" : "not ok", " 2\n";
print +(oct('0b101_01') ==             2_1) ? "ok" : "not ok", " 3\n";
print +(oct('0b1010_1') ==           0x1_5) ? "ok" : "not ok", " 4\n";

print +(oct('b1_0101') ==          0b10101) ? "ok" : "not ok", " 5\n";
print +(oct('b10_101') ==              025) ? "ok" : "not ok", " 6\n";
print +(oct('b101_01') ==               21) ? "ok" : "not ok", " 7\n";
print +(oct('b1010_1') ==             0x15) ? "ok" : "not ok", " 8\n";

print +(oct('01_234')  ==   0b10_1001_1100) ? "ok" : "not ok", " 9\n";
print +(oct('012_34')  ==            01234) ? "ok" : "not ok", " 10\n";
print +(oct('0123_4')  ==              668) ? "ok" : "not ok", " 11\n";
print +(oct('01234')   ==            0x29c) ? "ok" : "not ok", " 12\n";

print +(oct('0x1_234') == 0b10010_00110100) ? "ok" : "not ok", " 13\n";
print +(oct('0x12_34') ==          01_1064) ? "ok" : "not ok", " 14\n";
print +(oct('0x123_4') ==             4660) ? "ok" : "not ok", " 15\n";
print +(oct('0x1234')  ==          0x12_34) ? "ok" : "not ok", " 16\n";

print +(oct('x1_234')  == 0b100100011010_0) ? "ok" : "not ok", " 17\n";
print +(oct('x12_34')  ==          0_11064) ? "ok" : "not ok", " 18\n";
print +(oct('x123_4')  ==             4660) ? "ok" : "not ok", " 19\n";
print +(oct('x1234')   ==          0x_1234) ? "ok" : "not ok", " 20\n";

print +(hex('01_234')  == 0b_1001000110100) ? "ok" : "not ok", " 21\n";
print +(hex('012_34')  ==           011064) ? "ok" : "not ok", " 22\n";
print +(hex('0123_4')  ==             4660) ? "ok" : "not ok", " 23\n";
print +(hex('01234_')  ==           0x1234) ? "ok" : "not ok", " 24\n";

print +(hex('0x_1234') ==  0b1001000110100) ? "ok" : "not ok", " 25\n";
print +(hex('0x1_234') ==           011064) ? "ok" : "not ok", " 26\n";
print +(hex('0x12_34') ==             4660) ? "ok" : "not ok", " 27\n";
print +(hex('0x1234_') ==           0x1234) ? "ok" : "not ok", " 28\n";

print +(hex('x_1234')  ==  0b1001000110100) ? "ok" : "not ok", " 29\n";
print +(hex('x12_34')  ==           011064) ? "ok" : "not ok", " 30\n";
print +(hex('x123_4')  ==             4660) ? "ok" : "not ok", " 31\n";
print +(hex('x1234_')  ==           0x1234) ? "ok" : "not ok", " 32\n";

print +(oct('0b1111_1111_1111_1111_1111_1111_1111_1111') == 4294967295) ?
    "ok" : "not ok", " 33\n";
print +(oct('037_777_777_777')                       == 4294967295) ?
    "ok" : "not ok", " 34\n";
print +(oct('0xffff_ffff')                         == 4294967295) ?
    "ok" : "not ok", " 35\n";

print +(hex('0xff_ff_ff_ff')                         == 4294967295) ?
    "ok" : "not ok", " 36\n";

$_ = "\0_7_7";
print length eq 5                      ? "ok" : "not ok", " 37\n";
print $_ eq "\0"."_"."7"."_"."7"       ? "ok" : "not ok", " 38\n";
chop, chop, chop, chop;
print $_ eq "\0"                       ? "ok" : "not ok", " 39\n";
if (ord("\t") != 9) {
    # question mark is 111 in 1047, 037, && POSIX-BC
    print "\157_" eq "?_"                  ? "ok" : "not ok", " 40\n";
}
else {
    print "\077_" eq "?_"                  ? "ok" : "not ok", " 40\n";
}

$_ = "\x_7_7";
print length eq 5                      ? "ok" : "not ok", " 41\n";
print $_ eq "\0"."_"."7"."_"."7"       ? "ok" : "not ok", " 42\n";
chop, chop, chop, chop;
print $_ eq "\0"                       ? "ok" : "not ok", " 43\n";
if (ord("\t") != 9) {
    # / is 97 in 1047, 037, && POSIX-BC
    print "\x61_" eq "/_"                  ? "ok" : "not ok", " 44\n";
}
else {
    print "\x2F_" eq "/_"                  ? "ok" : "not ok", " 44\n";
}

print +(oct('0b'.(  '0'x10).'1_0101') ==  0b101_01) ? "ok" : "not ok", " 45\n";
print +(oct('0b'.( '0'x100).'1_0101') ==  0b101_01) ? "ok" : "not ok", " 46\n";
print +(oct('0b'.('0'x1000).'1_0101') ==  0b101_01) ? "ok" : "not ok", " 47\n";

print +(hex((  '0'x10).'01234') ==  0x1234) ? "ok" : "not ok", " 48\n";
print +(hex(( '0'x100).'01234') ==  0x1234) ? "ok" : "not ok", " 49\n";
print +(hex(('0'x1000).'01234') ==  0x1234) ? "ok" : "not ok", " 50\n";

