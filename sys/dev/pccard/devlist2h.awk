#! /usr/bin/awk -f
#	$NetBSD: devlist2h.awk,v 1.3 1998/09/05 14:42:06 christos Exp $
# $FreeBSD: src/sys/dev/pccard/devlist2h.awk,v 1.1 1999/10/26 06:52:28 imp Exp $
#
# Copyright (c) 1998 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Christos Zoulas.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#        This product includes software developed by the NetBSD
#        Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# Copyright (c) 1995, 1996 Christopher G. Demetriou
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#      This product includes software developed by Christopher G. Demetriou.
#      This product includes software developed by Christos Zoulas
# 4. The name of the author(s) may not be used to endorse or promote products
#    derived from this software without specific prior written permission
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
function collectline(f, line) {
	oparen = 0
	line = ""
	while (f <= NF) {
		if ($f == "#") {
			line = line "("
			oparen = 1
			f++
			continue
		}
		if (oparen) {
			line = line $f
			if (f < NF)
				line = line " "
			f++
			continue
		}
		line = line $f
		if (f < NF)
			line = line " "
		f++
	}
	if (oparen)
		line = line ")"
	return line
}
BEGIN {
	nproducts = nvendors = 0
	dfile="pccarddevs_data.h"
	hfile="pccarddevs.h"
}
NR == 1 {
	VERSION = $0
	gsub("\\$", "", VERSION)

	printf("/*\t\$FreeBSD\$\t*/\n\n") > dfile
	printf("/*\n") > dfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > dfile
	printf(" *\n") > dfile
	printf(" * generated from:\n") > dfile
	printf(" *\t%s\n", VERSION) > dfile
	printf(" */\n") > dfile

	printf("/*\t\$FreeBSD\$\t*/\n\n") > hfile
	printf("/*\n") > hfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > hfile
	printf(" *\n") > hfile
	printf(" * generated from:\n") > hfile
	printf(" *\t%s\n", VERSION) > hfile
	printf(" */\n") > hfile

	next
}
$1 == "vendor" {
	nvendors++

	vendorindex[$2] = nvendors;		# record index for this name, for later.
	vendors[nvendors, 1] = $2;		# name
	vendors[nvendors, 2] = $3;		# id
	printf("#define\tPCCARD_VENDOR_%s\t%s\t", vendors[nvendors, 1],
	    vendors[nvendors, 2]) > hfile
	vendors[nvendors, 3] = collectline(4, line)
	printf("/* %s */\n", vendors[nvendors, 3]) > hfile
	next
}
$1 == "product" {
	nproducts++

	products[nproducts, 1] = $2;		# vendor name
	products[nproducts, 2] = $3;		# product id
	products[nproducts, 3] = $4;		# id

	f = 5;

	if ($4 == "{") {
		products[nproducts, 3] = -1
		z = "{ "
		for (i = 0; i < 4; i++) {
			if (f <= NF) {
				gsub("&sp", " ", $f)
				gsub("&tab", "\t", $f)
				gsub("&nl", "\n", $f)
				z = z $f " "
				f++
			}
			else {
				if (i == 3)
					z = z "NULL "
				else
					z = z "NULL, "
			}
		}
		products[nproducts, 4] = z $f
		f++
	}
	else {
		products[nproducts, 4] = "{ NULL, NULL, NULL, NULL }"
	}
	printf("#define\tPCCARD_CIS_%s_%s\t%s\n",
	    products[nproducts, 1], products[nproducts, 2],
	    products[nproducts, 4]) > hfile
	printf("#define\tPCCARD_PRODUCT_%s_%s\t%s\n", products[nproducts, 1],
	    products[nproducts, 2], products[nproducts, 3]) > hfile

	products[nproducts, 5] = collectline(f, line)

	printf("#define\tPCCARD_STR_%s_%s\t\"%s\"\n",
	    products[nproducts, 1], products[nproducts, 2],
	    products[nproducts, 5]) > hfile

	next
}
{
	if ($0 == "")
		blanklines++
	print $0 > hfile
	if (blanklines < 2)
		print $0 > dfile
}
END {
	# print out the match tables

	printf("\n") > dfile

	printf("struct pccard_knowndev pccard_knowndevs[] = {\n") > dfile
	for (i = 1; i <= nproducts; i++) {
		printf("\t{\n") > dfile
		if (products[i, 3] == -1) {
			printf("\t    PCCARD_VENDOR_UNKNOWN, PCCARD_PRODUCT_%s_%s,\n",
			    products[i, 1], products[i, 2]) > dfile
		} else {
			printf("\t    PCCARD_VENDOR_%s, PCCARD_PRODUCT_%s_%s,\n",
			    products[i, 1], products[i, 1], products[i, 2]) > dfile
		}
		printf("\t    PCCARD_CIS_%s_%s,\n", 
		    products[i, 1], products[i, 2]) > dfile
		printf("\t    ") > dfile
		printf("0") > dfile
		printf(",\n") > dfile

		vendi = vendorindex[products[i, 1]];
		printf("\t    \"%s\",\n", vendors[vendi, 3]) > dfile
		printf("\t    \"%s\"\t},\n", products[i, 5]) > dfile
		printf("\t},\n") > dfile
	}
	for (i = 1; i <= nvendors; i++) {
		printf("\t{\n") > dfile
		printf("\t    PCCARD_VENDOR_%s, 0,\n", vendors[i, 1]) > dfile
		printf("\t    PCCARD_KNOWNDEV_NOPROD,\n") > dfile
		printf("\t    PCCARD_CIS_INVALID,\n") > dfile
		printf("\t    \"%s\",\n", vendors[i, 3]) > dfile
		printf("\t    NULL,\n") > dfile
		printf("\t},\n") > dfile
	}
	printf("\t{ 0, 0, { NULL, NULL, NULL, NULL }, 0, NULL, NULL, }\n") > dfile
	printf("};\n") > dfile
}
