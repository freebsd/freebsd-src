#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

no utf8;

print "1..78\n";

my $test = 1;

# This table is based on Markus Kuhn's UTF-8 Decode Stress Tester,
# http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt,
# version dated 2000-09-02. 

# We use the \x notation instead of raw binary bytes for \x00-\x1f\x7f-\xff
# because e.g. many patch programs have issues with binary data.

my @MK = split(/\n/, <<__EOMK__);
1	Correct UTF-8
1.1.1 y "\xce\xba\xe1\xbd\xb9\xcf\x83\xce\xbc\xce\xb5"	-		11	ce:ba:e1:bd:b9:cf:83:ce:bc:ce:b5	5
2	Boundary conditions 
2.1	First possible sequence of certain length
2.1.1 y "\x00"			0		1	00	1
2.1.2 y "\xc2\x80"			80		2	c2:80	1
2.1.3 y "\xe0\xa0\x80"		800		3	e0:a0:80	1
2.1.4 y "\xf0\x90\x80\x80"		10000		4	f0:90:80:80	1
2.1.5 y "\xf8\x88\x80\x80\x80"	200000		5	f8:88:80:80:80	1
2.1.6 y "\xfc\x84\x80\x80\x80\x80"	4000000		6	fc:84:80:80:80:80	1
2.2	Last possible sequence of certain length
2.2.1 y "\x7f"			7f		1	7f	1
2.2.2 y "\xdf\xbf"			7ff		2	df:bf	1
# The ffff is illegal unless UTF8_ALLOW_FFFF
2.2.3 n "\xef\xbf\xbf"			ffff		3	ef:bf:bf	1	character 0xffff
2.2.4 y "\xf7\xbf\xbf\xbf"			1fffff		4	f7:bf:bf:bf	1
2.2.5 y "\xfb\xbf\xbf\xbf\xbf"			3ffffff		5	fb:bf:bf:bf:bf	1
2.2.6 y "\xfd\xbf\xbf\xbf\xbf\xbf"		7fffffff	6	fd:bf:bf:bf:bf:bf	1
2.3	Other boundary conditions
2.3.1 y "\xed\x9f\xbf"		d7ff		3	ed:9f:bf	1
2.3.2 y "\xee\x80\x80"		e000		3	ee:80:80	1
2.3.3 y "\xef\xbf\xbd"			fffd		3	ef:bf:bd	1
2.3.4 y "\xf4\x8f\xbf\xbf"		10ffff		4	f4:8f:bf:bf	1
2.3.5 y "\xf4\x90\x80\x80"		110000		4	f4:90:80:80	1
3	Malformed sequences
3.1	Unexpected continuation bytes
3.1.1 n "\x80"			-		1	80	-	unexpected continuation byte 0x80
3.1.2 n "\xbf"			-		1	bf	-	unexpected continuation byte 0xbf
3.1.3 n "\x80\xbf"			-		2	80:bf	-	unexpected continuation byte 0x80
3.1.4 n "\x80\xbf\x80"		-		3	80:bf:80	-	unexpected continuation byte 0x80
3.1.5 n "\x80\xbf\x80\xbf"		-		4	80:bf:80:bf	-	unexpected continuation byte 0x80
3.1.6 n "\x80\xbf\x80\xbf\x80"	-		5	80:bf:80:bf:80	-	unexpected continuation byte 0x80
3.1.7 n "\x80\xbf\x80\xbf\x80\xbf"	-		6	80:bf:80:bf:80:bf	-	unexpected continuation byte 0x80
3.1.8 n "\x80\xbf\x80\xbf\x80\xbf\x80"	-		7	80:bf:80:bf:80:bf:80	-	unexpected continuation byte 0x80
3.1.9 n "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"				-	64	80:81:82:83:84:85:86:87:88:89:8a:8b:8c:8d:8e:8f:90:91:92:93:94:95:96:97:98:99:9a:9b:9c:9d:9e:9f:a0:a1:a2:a3:a4:a5:a6:a7:a8:a9:aa:ab:ac:ad:ae:af:b0:b1:b2:b3:b4:b5:b6:b7:b8:b9:ba:bb:bc:bd:be:bf	-	unexpected continuation byte 0x80
3.2	Lonely start characters
3.2.1 n "\xc0 \xc1 \xc2 \xc3 \xc4 \xc5 \xc6 \xc7 \xc8 \xc9 \xca \xcb \xcc \xcd \xce \xcf \xd0 \xd1 \xd2 \xd3 \xd4 \xd5 \xd6 \xd7 \xd8 \xd9 \xda \xdb \xdc \xdd \xde \xdf "	-	64 	c0:20:c1:20:c2:20:c3:20:c4:20:c5:20:c6:20:c7:20:c8:20:c9:20:ca:20:cb:20:cc:20:cd:20:ce:20:cf:20:d0:20:d1:20:d2:20:d3:20:d4:20:d5:20:d6:20:d7:20:d8:20:d9:20:da:20:db:20:dc:20:dd:20:de:20:df:20	-	unexpected non-continuation byte 0x20 after start byte 0xc0
3.2.2 n "\xe0 \xe1 \xe2 \xe3 \xe4 \xe5 \xe6 \xe7 \xe8 \xe9 \xea \xeb \xec \xed \xee \xef "	-	32	e0:20:e1:20:e2:20:e3:20:e4:20:e5:20:e6:20:e7:20:e8:20:e9:20:ea:20:eb:20:ec:20:ed:20:ee:20:ef:20	-	unexpected non-continuation byte 0x20 after start byte 0xe0
3.2.3 n "\xf0 \xf1 \xf2 \xf3 \xf4 \xf5 \xf6 \xf7 "	-	16	f0:20:f1:20:f2:20:f3:20:f4:20:f5:20:f6:20:f7:20	-	unexpected non-continuation byte 0x20 after start byte 0xf0
3.2.4 n "\xf8 \xf9 \xfa \xfb "		-	8	f8:20:f9:20:fa:20:fb:20	-	unexpected non-continuation byte 0x20 after start byte 0xf8
3.2.5 n "\xfc \xfd "			-	4	fc:20:fd:20	-	unexpected non-continuation byte 0x20 after start byte 0xfc
3.3	Sequences with last continuation byte missing
3.3.1 n "\xc0"			-	1	c0	-	1 byte, need 2
3.3.2 n "\xe0\x80"			-	2	e0:80	-	2 bytes, need 3
3.3.3 n "\xf0\x80\x80"		-	3	f0:80:80	-	3 bytes, need 4
3.3.4 n "\xf8\x80\x80\x80"		-	4	f8:80:80:80	-	4 bytes, need 5
3.3.5 n "\xfc\x80\x80\x80\x80"	-	5	fc:80:80:80:80	-	5 bytes, need 6
3.3.6 n "\xdf"			-	1	df	-	1 byte, need 2
3.3.7 n "\xef\xbf"			-	2	ef:bf	-	2 bytes, need 3
3.3.8 n "\xf7\xbf\xbf"			-	3	f7:bf:bf	-	3 bytes, need 4
3.3.9 n "\xfb\xbf\xbf\xbf"			-	4	fb:bf:bf:bf	-	4 bytes, need 5
3.3.10 n "\xfd\xbf\xbf\xbf\xbf"		-	5	fd:bf:bf:bf:bf	-	5 bytes, need 6
3.4	Concatenation of incomplete sequences
3.4.1 n "\xc0\xe0\x80\xf0\x80\x80\xf8\x80\x80\x80\xfc\x80\x80\x80\x80\xdf\xef\xbf\xf7\xbf\xbf\xfb\xbf\xbf\xbf\xfd\xbf\xbf\xbf\xbf"	-	30	c0:e0:80:f0:80:80:f8:80:80:80:fc:80:80:80:80:df:ef:bf:f7:bf:bf:fb:bf:bf:bf:fd:bf:bf:bf:bf	-	unexpected non-continuation byte 0xe0 after start byte 0xc0
3.5	Impossible bytes
3.5.1 n "\xfe"			-	1	fe	-	byte 0xfe
3.5.2 n "\xff"			-	1	ff	-	byte 0xff
3.5.3 n "\xfe\xfe\xff\xff"			-	4	fe:fe:ff:ff	-	byte 0xfe
4	Overlong sequences
4.1	Examples of an overlong ASCII character
4.1.1 n "\xc0\xaf"			-	2	c0:af	-	2 bytes, need 1
4.1.2 n "\xe0\x80\xaf"		-	3	e0:80:af	-	3 bytes, need 1
4.1.3 n "\xf0\x80\x80\xaf"		-	4	f0:80:80:af	-	4 bytes, need 1
4.1.4 n "\xf8\x80\x80\x80\xaf"	-	5	f8:80:80:80:af	-	5 bytes, need 1
4.1.5 n "\xfc\x80\x80\x80\x80\xaf"	-	6	fc:80:80:80:80:af	-	6 bytes, need 1
4.2	Maximum overlong sequences
4.2.1 n "\xc1\xbf"			-	2	c1:bf	-	2 bytes, need 1
4.2.2 n "\xe0\x9f\xbf"		-	3	e0:9f:bf	-	3 bytes, need 2
4.2.3 n "\xf0\x8f\xbf\xbf"		-	4	f0:8f:bf:bf	-	4 bytes, need 3
4.2.4 n "\xf8\x87\xbf\xbf\xbf"		-	5	f8:87:bf:bf:bf	-	5 bytes, need 4
4.2.5 n "\xfc\x83\xbf\xbf\xbf\xbf"		-	6	fc:83:bf:bf:bf:bf	-	6 bytes, need 5
4.3	Overlong representation of the NUL character
4.3.1 n "\xc0\x80"			-	2	c0:80	-	2 bytes, need 1
4.3.2 n "\xe0\x80\x80"		-	3	e0:80:80	-	3 bytes, need 1
4.3.3 n "\xf0\x80\x80\x80"		-	4	f0:80:80:80	-	4 bytes, need 1
4.3.4 n "\xf8\x80\x80\x80\x80"	-	5	f8:80:80:80:80	-	5 bytes, need 1
4.3.5 n "\xfc\x80\x80\x80\x80\x80"	-	6	fc:80:80:80:80:80	-	6 bytes, need 1
5	Illegal code positions
5.1	Single UTF-16 surrogates
5.1.1 n "\xed\xa0\x80"		-	3	ed:a0:80	-	UTF-16 surrogate 0xd800
5.1.2 n "\xed\xad\xbf"			-	3	ed:ad:bf	-	UTF-16 surrogate 0xdb7f
5.1.3 n "\xed\xae\x80"		-	3	ed:ae:80	-	UTF-16 surrogate 0xdb80
5.1.4 n "\xed\xaf\xbf"			-	3	ed:af:bf	-	UTF-16 surrogate 0xdbff
5.1.5 n "\xed\xb0\x80"		-	3	ed:b0:80	-	UTF-16 surrogate 0xdc00
5.1.6 n "\xed\xbe\x80"		-	3	ed:be:80	-	UTF-16 surrogate 0xdf80
5.1.7 n "\xed\xbf\xbf"			-	3	ed:bf:bf	-	UTF-16 surrogate 0xdfff
5.2	Paired UTF-16 surrogates
5.2.1 n "\xed\xa0\x80\xed\xb0\x80"		-	6	ed:a0:80:ed:b0:80	-	UTF-16 surrogate 0xd800
5.2.2 n "\xed\xa0\x80\xed\xbf\xbf"		-	6	ed:a0:80:ed:bf:bf	-	UTF-16 surrogate 0xd800
5.2.3 n "\xed\xad\xbf\xed\xb0\x80"		-	6	ed:ad:bf:ed:b0:80	-	UTF-16 surrogate 0xdb7f
5.2.4 n "\xed\xad\xbf\xed\xbf\xbf"		-	6	ed:ad:bf:ed:bf:bf	-	UTF-16 surrogate 0xdb7f
5.2.5 n "\xed\xae\x80\xed\xb0\x80"		-	6	ed:ae:80:ed:b0:80	-	UTF-16 surrogate 0xdb80
5.2.6 n "\xed\xae\x80\xed\xbf\xbf"		-	6	ed:ae:80:ed:bf:bf	-	UTF-16 surrogate 0xdb80
5.2.7 n "\xed\xaf\xbf\xed\xb0\x80"		-	6	ed:af:bf:ed:b0:80	-	UTF-16 surrogate 0xdbff
5.2.8 n "\xed\xaf\xbf\xed\xbf\xbf"		-	6	ed:af:bf:ed:bf:bf	-	UTF-16 surrogate 0xdbff
5.3	Other illegal code positions
5.3.1 n "\xef\xbf\xbe"			-	3	ef:bf:be	-	byte order mark 0xfffe
# The ffff is illegal unless UTF8_ALLOW_FFFF
5.3.2 n "\xef\xbf\xbf"			-	3	ef:bf:bf	-	character 0xffff
__EOMK__

# 104..181
{
    my $WARNCNT;
    my $id;

    local $SIG{__WARN__} =
	sub {
	    print "# $id: @_";
	    $WARNCNT++;
	    $WARNMSG = "@_";
	};

    sub moan {
	print "$id: @_";
    }
    
    sub test_unpack_U {
	$WARNCNT = 0;
	$WARNMSG = "";
	unpack('U*', $_[0]);
    }

    for (@MK) {
	if (/^(?:\d+(?:\.\d+)?)\s/ || /^#/) {
	    # print "# $_\n";
	} elsif (/^(\d+\.\d+\.\d+[bu]?)\s+([yn])\s+"(.+)"\s+([0-9a-f]{1,8}|-)\s+(\d+)\s+([0-9a-f]{2}(?::[0-9a-f]{2})*)(?:\s+((?:\d+|-)(?:\s+(.+))?))?$/) {
	    $id = $1;
	    my ($okay, $bytes, $Unicode, $byteslen, $hex, $charslen, $error) =
		($2, $3, $4, $5, $6, $7, $8);
	    my @hex = split(/:/, $hex);
	    unless (@hex == $byteslen) {
		my $nhex = @hex;
		moan "amount of hex ($nhex) not equal to byteslen ($byteslen)\n";
	    }
	    {
		use bytes;
		my $bytesbyteslen = length($bytes);
		unless ($bytesbyteslen == $byteslen) {
		    moan "bytes length() ($bytesbyteslen) not equal to $byteslen\n";
		}
	    }
	    if ($okay eq 'y') {
		test_unpack_U($bytes);
		if ($WARNCNT) {
		    moan "unpack('U*') false negative\n";
		    print "not ";
		}
	    } elsif ($okay eq 'n') {
		test_unpack_U($bytes);
		if ($WARNCNT == 0 || ($error ne '' && $WARNMSG !~ /$error/)) {
		    moan "unpack('U*') false positive\n";
		    print "not ";
		}
	    }
	    print "ok $test\n";
	    $test++;
 	} else {
	    moan "unknown format\n";
	}
    }
}
