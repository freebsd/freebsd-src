# usage: awk -f actionnm.awk pseudocode.h
#
BEGIN { "date" | getline
	today = $0
	printf("\n/* this file generated on %s  */\n", today )
	printf("\nstatic char *action_names[] = { \n    " )
	opl = 0
}

/^#define/ {			
	if ( opl > 3 ) {
	    printf("\n    ")
	    opl = 0
	}
	opl = opl +1
	t = sprintf("\"%s\"", $2 )
	printf("%-15s ,", t )
#	printf("%-10s", $2 )
}

END {		
	if ( opl > 3 ) {
	    printf("\n    ")
	}
	printf("\t  0\n};\n\n")
}

