# $FreeBSD$
/^#/	{ next }
NF == 1 { next }
$2 == "??" { next }
$2 == "lkm" { next }
	{
	a[$1] = $1;
	}
END	{
	print "unsigned char reserved_majors[256] = {"
	for (i = 0; i < 256; i += 16) {
		for (j = 0; j < 16; j++) {
			printf("%3d", a[i + j]);
			if (i + j != 255)
				printf(",");
		}
		print ""
	}
	print "};"
	}

