% { dg-do assemble { target mmix-*-* } }
% Check that error handling for the restrictions on LOCAL works.
 LOCAL 128 % { dg-error "LOCAL must be placed in code or data" "" }
