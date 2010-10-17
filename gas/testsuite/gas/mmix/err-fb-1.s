% { dg-do assemble { target mmix-*-* } }
% { dg-error "may not appear alone on a line" "" { target mmix-*-* } 5 }
% { dg-error "may not appear alone on a line" "" { target mmix-*-* } 6 }
0H .local 32  % { dg-error "do not mix with dot-pseudos" "" }
1H
2H
3H .set s,32  % { dg-error "do not mix with dot-pseudos" "" }
