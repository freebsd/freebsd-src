% { dg-do assemble { target mmix-*-* } }
% { dg-options "-no-predefined-syms" }
% When disallowing built-in names, we have to treat GET and PUT
% specially, so when parsing the special register operand we do
% not use the symbol table.  Make sure an error is emitted for
% invalid registers despite there being a valid user label and
% the construct being valid without the -no-builtin-syms option.
% FIXME: Another option?  Or is this just the consequence?
RJ IS 4
other IS 20
Main GET $5,RJ % { dg-error "invalid operands" "" }
 PUT other,$7 % { dg-error "invalid operands" "" }
 GET garbage % { dg-error "invalid operands" "" }
 PUT garbage % { dg-error "invalid operands" "" }
