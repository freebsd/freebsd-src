	;; test the parsing of strings and character constants
	section .data
laf:	
	defb "single:'"
	defb 'double:"',laf
	defb 'escape:\\'

	ex af,af'
af0:	
	cp '9'+1

