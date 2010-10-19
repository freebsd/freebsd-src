;# Test .warning directive.
;# { dg-do assemble }
 .warning "a warning message"	;# { dg-warning "Warning: a warning message" }
 .warning a warning message	;# { dg-error "Error: .warning argument must be a string" }
 .warning			;# { dg-warning "Warning: .warning directive invoked in source file" }
 .warning ".warning directive invoked in source file"	;# { dg-warning "Warning: .warning directive invoked in source file" }
 .warning ""			;# { dg-warning "Warning: " }
