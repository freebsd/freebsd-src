#!/bin/sh
#
#  ascq_tbl.sh - Translate SCSI t10.org's "asc-num.txt" file of
#                SCSI Additional Sense Code & Qualifiers (ASC/ASCQ's)
#                into something useful in C, creating "ascq_tbl.c" file.
#
#*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*#

PREF_INFILE="t10.org/asc-num.txt"	# From SCSI t10.org
PREF_OUTFILE="ascq_tbl.c"

#*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*#

xlate_ascq() {
	cat | awk '
	BEGIN {
		DQ = "\042";
		OUTFILE = "'"${PREF_OUTFILE}"'";
		TRUE = 1;
		FALSE = 0;
		#debug = TRUE;

		#  read and discard all lines up to and including the one that begins
		#  with the "magic token" of "-------  --------------  ---"...
		headers_gone = FALSE;
		while (!headers_gone) {
			if (getline <= 0)
				exit 1;
			header_line[++hdrs] = $0;
			if (debug)
				printf("header_line[%d] = :%s:\n", ++hdrs, $0);
			if ($0 ~ /^-------  --------------  ---/) {
				headers_gone = TRUE;
			}
		}
		outcount = 0;
	}

	(NF > 1) {
		++outcount;
		if (debug)
			printf( "DBG: %s\n", $0 );
		ASC[outcount] = substr($0,1,2);
		ASCQ[outcount] = substr($0,5,2);
		devtypes = substr($0,10,14);
		gsub(/ /, ".", devtypes);
		DESCRIP[outcount] = substr($0,26);

		if (!(devtypes in DevTypesVoodoo)) {
			DevTypesVoodoo[devtypes] = ++voodoo;
			DevTypesIdx[voodoo] = devtypes;
		}
		DEVTYPES[outcount] = DevTypesVoodoo[devtypes];

		#  Handle 0xNN exception stuff...
		if (ASCQ[outcount] == "NN" || ASCQ[outcount] == "nn")
			ASCQ[outcount] = "FF";
	}

	END {
		printf("#ifndef SCSI_ASCQ_TBL_C_INCLUDED\n") > OUTFILE;
		printf("#define SCSI_ASCQ_TBL_C_INCLUDED\n") >> OUTFILE;

		printf("\n/* AuToMaGiCaLlY generated from: %s'"${FIN}"'%s\n", DQ, DQ) >> OUTFILE;
		printf(" *******************************************************************************\n") >> OUTFILE;
		for (i=1; i<=hdrs; i++) {
			printf(" * %s\n", header_line[i]) >> OUTFILE;
		}
		printf(" */\n") >> OUTFILE;

		printf("\n") >> OUTFILE;
		for (i=1; i<=voodoo; i++) {
			printf("static char SenseDevTypes%03d[] = %s%s%s;\n", i, DQ, DevTypesIdx[i], DQ) >> OUTFILE;
		}

		printf("\nstatic ASCQ_Table_t ASCQ_Table[] = {\n") >> OUTFILE;
		for (i=1; i<=outcount; i++) {
			printf("  {\n") >> OUTFILE; 
			printf("    0x%s, 0x%s,\n", ASC[i], ASCQ[i]) >> OUTFILE;
			printf("    SenseDevTypes%03d,\n", DEVTYPES[i]) >> OUTFILE;
			printf("    %s%s%s\n", DQ, DESCRIP[i], DQ) >> OUTFILE;
			printf("  },\n") >> OUTFILE;
		}
		printf( "};\n\n" ) >> OUTFILE;

		printf( "static int ASCQ_TableSize = %d;\n\n", outcount ) >> OUTFILE;
		printf( "Total of %d ASC/ASCQ records generated\n", outcount );
		printf("\n#endif\n") >> OUTFILE;
		close(OUTFILE);
	}'
	return
}

#*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*#

# main()
if [ $# -lt 1 ]; then
	echo "INFO: No input filename supplied - using: $PREF_INFILE" >&2
	FIN=$PREF_INFILE
else
	FIN="$1"
	if [ "$FIN" != "$PREF_INFILE" ]; then
		echo "INFO: Ok, I'll try chewing on '$FIN' for SCSI ASC/ASCQ combos..." >&2
	fi
	shift
fi

cat $FIN | xlate_ascq
exit 0
