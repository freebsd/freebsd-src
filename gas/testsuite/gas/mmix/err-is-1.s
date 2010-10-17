% { dg-do assemble { target mmix-*-* } }
 IS 42 % { dg-error "empty label field for IS" "" }
2H IS 1
 IS 2B % { dg-error "empty label field for IS" "" }
