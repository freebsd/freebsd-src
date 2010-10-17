% { dg-do assemble { target mmix-*-* } }

% SAVE, UNSAVE are not valid with base-plus-offset

 .data
buffer OCTA 0,0,0

 .text
 GREG buffer
Main SWYM 0
 SAVE buffer,0		% { dg-error "operands" "" }
 UNSAVE 0,buffer	% { dg-error "operands" "" }
