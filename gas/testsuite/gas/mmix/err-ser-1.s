% { dg-do assemble { target mmix-*-* } }
% { dg-bogus "bad expression" "" { xfail mmix-*-* } 9 }
% { dg-bogus "bad expression" "" { xfail mmix-*-* } 10 }

% Make sure we correctly diagnose the serial-number operator.
% We can't stop the "bad expression" error, though; hence the "bogus" errors.

a IS 42
Main TETRA &a<<8 { dg-error "serial number operator is not supported" "" }
  TETRA 3+&a<<8 { dg-error "serial number operator is not supported" "" }
