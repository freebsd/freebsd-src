#
# Copyright (C) 2006 Daniel M. Eischen.  All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#
# Make a list of all the library versions listed in the master file.
#
#   versions[] - array indexed by version name, contains number
#                of symbols (+ 1) found for each version.
#   successors[] - array index by version name, contains successor
#                  version name.
#   symbols[][] - array index by [version name, symbol index], contains
#                 names of symbols defined for each version.
#
BEGIN {
	brackets = 0;
	errors = 0;
	version_count = 0;
	current_version = "";
	stderr = "/dev/stderr";
	while (getline < vfile) {
		# Strip comments.
		sub("#.*$", "", $0);

		# Strip trailing spaces.
		sub(" *$", "", $0);

		if (/^[ \t]*[a-zA-Z0-9._]+ *{/) {
			brackets++;
			symver = $1;
			versions[symver] = 1;
			successors[symver] = "";
			generated[symver] = 0;
			version_count++;
		}
		else if (/^[ \t]*} *[a-zA-Z0-9._]+ *;/) {
			# Strip semicolon.
			gsub(";", "", $2);
			if (symver == "")
				printf("Unmatched bracket.\n");
			else if (versions[$2] != 1)
				printf("File %s: %s has unknown " \
				    "successor %s\n", vfile, symver, $2);
			else
				successors[symver] = $2;
			brackets--;
		}
		else if (/^[ \t]*};/) {
			if (symver == "")
				printf("File %s: Unmatched bracket.\n",
				    vfile) > stderr;
			# No successor
			brackets--;
		}
		else if (/^[ \t]*}/) {
			printf("File %s: Missing ending semi-colon.\n",
			    vfile) > stderr;
		}
		else if (/^$/)
			;  # Ignore blank lines.
		else
			printf("File %s: Unknown directive: %s\n",
			    vfile, $0) > stderr;
	}
	brackets = 0;
}

/.*/ {
	# Delete comments, preceding and trailing whitespace, then
	# consume blank lines.
	sub("#.*$", "", $0);
	sub("^[ \t]+", "", $0);
	sub("[ \t]+$", "", $0);
	if ($0 == "")
		next;
}

/^[a-zA-Z0-9._]+ +{$/ {
	# Strip bracket from version name.
	sub("{", "", $1);
	if (current_version != "")
		printf("File %s, line %d: Illegal nesting detected.\n",
		    FILENAME, FNR) > stderr;
	else if (versions[$1] == 0) {
		printf("File %s, line %d: Undefined " \
		    "library version %s\n", FILENAME, FNR, $1) > stderr;
		# Remove this entry from the versions.
		delete versions[$1];
	}
	else
		current_version = $1;
	brackets++;
	next;
}

/^[a-zA-Z0-9._]+ *;$/ {
	if (current_version != "") {
		count = versions[current_version];
		versions[current_version]++;
		symbols[current_version, count] = $1;
	}
	next;
}

/^} *;$/ {
	brackets--;
	if (brackets < 0) {
		printf("File %s, line %d: Unmatched bracket.\n",
		    FILENAME, FNR, $1) > stderr;
		brackets = 0;	# Reset
	}
	current_version = "";
	next;
}


/.*/ {
	printf("File %s, line %d: Unknown directive: '%s'\n",
	    FILENAME, FNR, $0) > stderr;
}

function print_version(v)
{
	# This function is recursive, so return if this version
	# has already been printed.  Otherwise, if there is an
	# ancestral version, recursively print its symbols before
	# printing the symbols for this version.
	#
	if (generated[v] == 1)
		return;
	if (successors[v] != "")
		print_version(successors[v]);

	printf("%s {\n", v);

	# The version count is always one more that actual,
	# so the loop ranges from 1 to n-1.
	#
	for (i = 1; i < versions[v]; i++) {
		if (i == 1)
			printf("global:\n");
		printf("\t%s\n", symbols[v, i]);
	}
	if (successors[v] == "") {
		# This version succeeds no other version.
		printf("local:\n");
		printf("\t*;\n");
		printf("};\n");
	}
	else
		printf("} %s;\n", successors[v]);
	printf("\n");

	generated[v] = 1;
    }
END {
	for (v in versions) {
		print_version(v);
	}
}
