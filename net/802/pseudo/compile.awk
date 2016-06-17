# usage: cat pseudocode | sed -f act2num | awk -f compile.awk
#
#
BEGIN { "date" | getline
	today = $0
	printf("\n/* this file generated on %s  */\n", today )
	printf("\nstatic char pseudo_code [ ] = { \n" )
	opl = 0			# op codes on the current line

	opc = 0			# opcode counter
	fpi = 0			# fill pointer for idx array
}

/^;/ { } 			# line starting with semicolon is comment 

/^[A-Z]/ {			# start of a new action 
	emit( 0 )
	idx[ ++fpi ] = opc
	name[ fpi ] = $1 
	emit( $2 )
}

/^[\t ]/ { 
        emit( $1 )
}

END {		
	if ( opl > 8 ) {
	    printf("\n")
	}
	printf("\t  0\n};\n\n")
	printf("static short int pseudo_code_idx [ ] ={\n")
	opl = 0
	emit( 0 )
	for( ii = 1; ii <= fpi; ii++ )
	   emit( idx[ ii ] )
	if ( opl > 8 ) {
	    printf("\n")
	}
	printf("\t  0\n};\n\n")

	printf("#define %-10s \t %3d \n", "NOP", 0 )
	for( ii = 1; ii <= fpi; ii++ )
	    printf("#define %-10s \t %3d \n", name[ ii ], ii )
	printf("\n")
}

function emit( opcode ){	# Niclaus Wirth
	if ( opl > 8 ) {
	    printf("\n")
	    opl = 0
	}
	opl = opl +1
	printf("\t%4d,", opcode )
	opc++
}

