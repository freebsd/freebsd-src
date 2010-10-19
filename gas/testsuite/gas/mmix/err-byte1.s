% { dg-do assemble { target mmix-*-* } }
% { dg-error "unterminated string|missing closing" "" { target mmix-*-* } 10 }
% { dg-bogus "end of file" "" { xfail mmix-*-* } 0 }

# Note that the error is detected in the preformatter, before the text
# gets to the assembler.  It also gets confused about the unterminated
# string.  Well, at least we get error messages for it, so no worries.

Main SWYM 0,0,0
 BYTE 2,"no end
 BYTE 0
