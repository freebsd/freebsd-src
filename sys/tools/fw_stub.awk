#!/usr/bin/awk -f

#-
# Copyright (c) 2006 Max Laier.
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
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

#
# Script to generate module .c file from a list of firmware images
#

function usage ()
{
	print "usage: fw_stub <firmware:name>* [-m modname] [-c outfile]";
	exit 1;
}

#   These are just for convenience ...
function printc(s)
{
	if (opt_c)
		print s > ctmpfilename;
	else
		print s > "/dev/stdout";
}

BEGIN {

#
#   Process the command line.
#

num_files = 0;

for (i = 1; i < ARGC; i++) {
	if (ARGV[i] ~ /^-/) {
		#
		#   awk doesn't have getopt(), so we have to do it ourselves.
		#   This is a bit clumsy, but it works.
		#
		for (j = 2; j <= length(ARGV[i]); j++) {
			o = substr(ARGV[i], j, 1);
			if (o == "c") {
				if (length(ARGV[i]) > j) {
					opt_c = substr(ARGV[i], j + 1);
					break;
				}
				else {
					if (++i < ARGC)
						opt_c = ARGV[i];
					else
						usage();
				}
			} else if (o == "m") {
				if (length(ARGV[i]) > j) {
					opt_m = substr(ARGV[i], j + 1);
					break;
				}
				else {
					if (++i < ARGC)
						opt_m = ARGV[i];
					else
						usage();
				}
			} else
				usage();
		}
	} else {
		split(ARGV[i], curr, ":");
		filenames[num_files] = curr[1];
		if (length(curr[2]) > 0)
			shortnames[num_files] = curr[2];
		else
			shortnames[num_files] = curr[2];
		if (length(curr[3]) > 0)
			versions[num_files] = int(curr[3]);
		else
			versions[num_files] = 0;
		num_files++;
	}
}

if (!num_files || !opt_m)
	usage();

cfilename = opt_c;
ctmpfilename = cfilename ".tmp";

printc("#include <sys/param.h>\
#include <sys/errno.h>\
#include <sys/kernel.h>\
#include <sys/module.h>\
#include <sys/linker.h>\
#include <sys/firmware.h>\n");

for (file_i = 0; file_i < num_files; file_i++) {
	symb = filenames[file_i];
	# '-', '.' and '/' are converted to '_' by ld/objcopy
	gsub(/-|\.|\//, "_", symb);
	printc("extern char _binary_" symb "_start[], _binary_" symb "_end[];");
}

printc("\nstatic int\n"\
opt_m "_fw_modevent(module_t mod, int type, void *unused)\
{\
	struct firmware *fp, *parent;\
	int error;\
	switch (type) {\
	case MOD_LOAD:");

for (file_i = 0; file_i < num_files; file_i++) {
	short = shortnames[file_i];
	symb = filenames[file_i];
	version = versions[file_i];
	# '-', '.' and '/' are converted to '_' by ld/objcopy
	gsub(/-|\.|\//, "_", symb);

	reg = "\t\tfp = ";
	reg = reg "firmware_register(\"" short "\", _binary_" symb "_start , ";
	reg = reg "(size_t)(_binary_" symb "_end - _binary_" symb "_start), ";
	reg = reg version ", ";

	if (file_i == 0)
		reg = reg "NULL);";
	else
		reg = reg "parent);";

	printc(reg);

	printc("\t\tif (fp == NULL)");
	printc("\t\t\tgoto fail_" file_i ";");
	if (file_i == 0)
		printc("\t\tparent = fp;");
}

printc("\t\treturn (0);");

for (file_i = num_files - 1; file_i > 0; file_i--) {
	printc("\tfail_" file_i ":")
	printc("\t\t(void)firmware_unregister(\"" shortnames[file_i - 1] "\");");
}

printc("\tfail_0:");
printc("\t\treturn (ENXIO);");

printc("\tcase MOD_UNLOAD:");

for (file_i = 1; file_i < num_files; file_i++) {
	printc("\t\terror = firmware_unregister(\"" shortnames[file_i] "\");");
	printc("\t\tif (error)");
	printc("\t\t\treturn (error);");
}

printc("\t\terror = firmware_unregister(\"" shortnames[0] "\");");

printc("\t\treturn (error);\
	}\
	return (EINVAL);\
}\
\
static moduledata_t " opt_m "_fw_mod = {\
        \"" opt_m "_fw\",\
        " opt_m "_fw_modevent,\
        0\
};\
DECLARE_MODULE(" opt_m "_fw, " opt_m "_fw_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);\
MODULE_VERSION(" opt_m "_fw, 1);\
MODULE_DEPEND(" opt_m "_fw, firmware, 1, 1, 1);\
");

if (opt_c)
	if ((rc = system("mv -f " ctmpfilename " " cfilename))) {
		print "'mv -f " ctmpfilename " " cfilename "' failed: " rc \
		    > "/dev/stderr";
		exit 1;
	}

exit 0;

}
