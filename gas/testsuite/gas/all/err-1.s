;# Test .error directive.
;# { dg-do assemble }
 .error "an error message"	;# { dg-error "Error: an error message" }
 .error an error message	;# { dg-error "Error: .error argument must be a string" }
 .error				;# { dg-error "Error: .error directive invoked in source file" }
 .error ".error directive invoked in source file" ;# { dg-error "Error: .error directive invoked in source file" }
 .error ""			;# { dg-error "Error: " }
