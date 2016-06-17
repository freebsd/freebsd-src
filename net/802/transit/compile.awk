# to run: awk -f transit.awk transit.p0
#
BEGIN { "date" | getline
	enable_index = 1
	today = $0
	printf("\n/* this file was generated on %s  */\n", today )
	not_firstone = 0	# flag to avoid empty entry in 1st table
	fpe = 0			# entry tbl array fill pointer
	fpeo = 0		# entry tbl offset list fill pointer
	fpdef = 0 		# define list fill pointer
}

### /^;/ { }			# line starting with a semicolon is comment 

/^[A-Z]/ {			# table name
	if ( $1 == "TABLE" ) {
	   tbl = $2		# get table name
	   newtbl( tbl )
	}
	else if ( $1 == "COMPILE" ) {
	   array_name = $2
	   if ( $3 == "NOINDEX" ) { enable_index = 0 }
	}
	else {			# table entry
	   ec = ec +1
	   n = split( $0, fld, " " )
	   action = fld[ n-1 ]
	   newstate = fld[ n ]
	   store( action, newstate )
	   ecct = ecct +1
	}
}

END {	store( action, newstate )

	if ( enable_index ) {
	    printf( "\n/* index name #defines: */\n\n",
		 ec, ecct )

	    for( ii = 1; ii <= fpeo; ii++ ){
	       printf( "#define %-12s %3d\n", define[ ii ], ii -1 )
	    }
	}

	printf( "\n\n/* size of transition table is %d bytes */\n",
		 fpe )

	if ( enable_index ) {
	    printf( "\nstatic short int %s_offset [ ] ={", array_name )
	    for( ii = 1; ii <= fpeo; ii++ ){
	        if ( (ii % 10) == 1 ) printf("\n  ")
	        printf( " %4d", entry_offset[ ii ] )
	        if ( ii < fpeo ) printf( "," )
	    }
	    printf(" };\n")
	}

	printf( "\nstatic char %s_entry [ ] = {", array_name )
	for( ii = 1; ii <= fpe; ii++ ){
	    if ( (ii % 6) == 1 ) printf("\n  ")
	    printf( " %-14s", entry[ ii ] )
	    if ( ii < fpe ) printf( "," )
	}
	printf(" };\n")

}

function store(  act, ns ){
#	printf( "%s %s\n",  act, ns )
	entry[ ++fpe ] = act
	entry[ ++fpe ] = ns 
}

function newtbl( tbl ){
	   if ( not_firstone ) {
		store( action, newstate )
	   }
	   not_firstone = 1
	   entry_offset[ ++fpeo ] = fpe		# entry tbl offset list
	   define[ ++fpdef ] = tbl	# state name to define
}
