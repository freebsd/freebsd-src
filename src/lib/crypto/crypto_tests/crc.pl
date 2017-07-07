# Copyright 2002 by the Massachusetts Institute of Technology.
# All Rights Reserved.
#
# Export of this software from the United States of America may
#   require a specific license from the United States Government.
#   It is the responsibility of any person or organization contemplating
#   export to obtain such a license before exporting.
# 
# WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
# distribute this software and its documentation for any purpose and
# without fee is hereby granted, provided that the above copyright
# notice appear in all copies and that both that copyright notice and
# this permission notice appear in supporting documentation, and that
# the name of M.I.T. not be used in advertising or publicity pertaining
# to distribution of the software without specific, written prior
# permission.  Furthermore if you modify this software you must label
# your software as modified software and not distribute it in such a
# fashion that it might be confused with the original M.I.T. software.
# M.I.T. makes no representations about the suitability of
# this software for any purpose.  It is provided "as is" without express
# or implied warranty.

use CRC;

print "*** crudely testing polynomial functions ***\n";

$x = Poly->new(1,1,1,1);
$y = Poly->new(1,1);
print "x = @{[$x->pretty]}\ny = @{[$y->pretty]}\n";
$q = $x / $y;
$r = $x % $y;
print $x->pretty, " = (", $y->pretty , ") * (", $q->pretty,
    ") + ", $r->pretty, "\n";
$q = $y / $x;
$r = $y % $x;
print "y / x = @{[$q->pretty]}\ny % x = @{[$r->pretty]}\n";

# ISO 3309 32-bit FCS polynomial
$fcs32 = Poly->powers2poly(32,26,23,22,16,12,11,10,8,7,5,4,2,1,0);
print "fcs32 = ", $fcs32->pretty, "\n";

$crc = CRC->new(Poly => $fcs32, bitsendian => "little");

print "\n";

print "*** little endian, no complementation ***\n";
for ($i = 0; $i < 256; $i++) {
    $r = $crc->crcstring(pack "C", $i);
    printf ("%02x: ", $i) if !($i % 8);
    print ($r->revhex, ($i % 8 == 7) ? "\n" : " ");
}

print "\n";

print "*** little endian, 4 bits, no complementation ***\n";
for ($i = 0; $i < 16; $i++) {
    @m = (split //, unpack "b*", pack "C", $i)[0..3];
    $r = $crc->crc(@m);
    printf ("%02x: ", $i) if !($i % 8);
    print ($r->revhex, ($i % 8 == 7) ? "\n" : " ");
}

print "\n";

print "*** test vectors for t_crc.c, little endian ***\n";
for ($i = 1; $i <= 4; $i *=2) {
    for ($j = 0; $j < $i * 8; $j++) {
	@m = split //, unpack "b*", pack "V", 1 << $j;
	splice @m, $i * 8;
	$r = $crc->crc(@m);
	$m = unpack "H*", pack "b*", join("", @m);
	print "{HEX, \"$m\", 0x", $r->revhex, "},\n";
    }
}
@m = ("foo", "test0123456789",
      "MASSACHVSETTS INSTITVTE OF TECHNOLOGY");
foreach $m (@m) {
    $r = $crc->crcstring($m);
    print "{STR, \"$m\", 0x", $r->revhex, "},\n";
}
__END__

print "*** big endian, no complementation ***\n";
for ($i = 0; $i < 256; $i++) {
    $r = $crc->crcstring(pack "C", $i);
    printf ("%02x: ", $i) if !($i % 8);
    print ($r->hex, ($i % 8 == 7) ? "\n" : " ");
}

# all ones polynomial of order 31
$ones = Poly->new((1) x 32);

print "*** big endian, ISO-3309 style\n";
$crc = CRC->new(Poly => $fcs32,
		bitsendian => "little",
		precomp => $ones,
		postcomp => $ones);
for ($i = 0; $i < 256; $i++) {
    $r = $crc->crcstring(pack "C", $i);
    print ($r->hex, ($i % 8 == 7) ? "\n" : " ");
}

for ($i = 0; $i < 0; $i++) {
    $x = Poly->new((1) x 32, (0) x $i);
    $y = Poly->new((1) x 32);
    $f = ($x % $fcs32) + $y;
    $r = (($f + $x) * Poly->powers2poly(32)) % $fcs32;
    @out = @$r;
    unshift @out, 0 while @out < 32;
    print @out, "\n";
}
