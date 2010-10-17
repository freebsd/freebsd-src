% Check that lowercase pseudos with mmixal syntax (no dot prefix) aren't
% recognized.  Since local is handled as an insn, it's actually
% misrecognized in lower case.
% { dg-do assemble { target mmix-*-* } }
Main SWYM 0,0,0
X is 42 % { dg-error "unknown opcode: \`is\'" "" }
 local 56 % { dg-error "unknown opcode: \`fatal\'" "" { xfail *-*-* } }
a greg 94 % { dg-error "unknown opcode: \`greg\'" "" }
