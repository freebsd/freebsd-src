:
#
# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
# ----------------------------------------------------------------------------
#
# $FreeBSD$
#
# During installation, we suffer badly of we have to run MAKEDEV.  MAKEDEV
# need sh, ln, chown, mknod, awk, rm, test and probably emacs too when
# we come down to it.  So instead this script will make a C-procedure which
# makes all the B & C nodes of a specified directory.
#
# Poul-Henning

(cd $1; ls -li ) | sed 's/,//' | awk '
BEGIN	{
	while (getline < "/etc/passwd") {
		split($0,a,":")
		uid[a[1]] = a[3]
	}
	while (getline < "/etc/group") {
		split($0,a,":")
		gid[a[1]] = a[3]
	}
	printf("/*\n");
	printf(" * This file is generated from the contents of /dev\n");
	printf(" */\n");
	printf("#define CHK(foo) {i = foo;}\n");
	printf("#include <unistd.h>\n");
	printf("#include <sys/types.h>\n");
	printf("#include <sys/stat.h>\n");
	printf("int makedevs()\n{\n\tint i=0;\n");
	}
	{
	printf ("/* %s */\n",$0)
	$4 = uid[$4]
	$5 = gid[$5]
	if (substr($2,1,1) == "b") {
		k="S_IFBLK"
	} else if (substr($2,1,1) == "c") {
		k="S_IFCHR"
	} else if (substr($2,1,1) == "d") {
		next
	} else if (substr($2,1,1) == "-") {
		next
	} else {
		next
	}
	m = 0;
	if (substr($2,2,1)  == "r") m += 400;
	if (substr($2,3,1)  == "w") m += 200;
	if (substr($2,4,1)  == "x") m += 100;
	if (substr($2,5,1)  == "r") m += 40;
	if (substr($2,6,1)  == "w") m += 20;
	if (substr($2,7,1)  == "x") m += 10;
	if (substr($2,8,1)  == "r") m += 4;
	if (substr($2,9,1)  == "w") m += 2;
	if (substr($2,10,1) == "x") m += 1;
	
	if (a[$1] != 0) {
		printf ("\tCHK(link(\"%s\",\"%s\"));\n", \
			a[$1],$11)
	} else {
		printf ("\tCHK(mknod(\"%s\",%s,makedev(%d,%d)));\n", \
			$11, k, $6, $7)
		printf ("\tCHK(chmod(\"%s\",0%d));\n", \
			$11, m)
		printf ("\tCHK(chown(\"%s\",%d,%d));\n", \
			$11, $4,$5)
		a[$1] = $11
	}
	}
END	{
	printf("\treturn i;\n}\n");
	}
'
