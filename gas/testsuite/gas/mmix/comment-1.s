# Check that "naked" comments are accepted and ignored on all different
# mnemonic types and pseudos.  The goal is to use all combinations of
# operands where varying number of operands are allowed.  If any
# combinations are missing, for simplicity, add them to another file.
Main TRAP 123 ignore; x y z
 TRAP 1,23 all; x y z
 TRAP 1,2,3 these; x y z
 FCMP $3,$2,$1 comments; x y z
 FLOT $5,6 and; x y z
 FLOT $7,ROUND_UP,8 do; x y z
 FIX $9,$10 nothing; x y z
 FIX $11,ROUND_DOWN,$12 that; x y z
 ADDU $15,$16,17 would make; x y z
 LDA $18,$19 a; x y z
 LDA $20,$21,22 difference; x y z
 NEG $23,$24 in; x y z
 NEG $25,26,$27 the; x y z
 bn $28,target + 44 generated; x y z
 SYNCD 29,$30,31 code; x y z
 PUSHGO 32,$33,34 so; x y z
 SET $35,$36 it; x y z
 SETH $37,target2 + 48 is; x y z
 JMP target3 + 56 as; x y z
 POP 38,39 if; x y z
 RESUME 40 it; x y z
 PUSHJ $41,target3 had; x y z
 SAVE $42,0 never; x y z
 UNSAVE 0,$43 been; x y z
 PUT rJ,$44 there; x y z
 GET $45,rJ at all.; x y z

 LOC @+4 likewise; x y z
 PREFIX : with; x y z
 BYTE 3,2,1,0+4 the; x y z
 WYDE 7,4+8 different; x y z
 TETRA 8+12 pseudo; x y z
 OCTA 12+16 ops,; x y z
 LOCAL 48 they; x y z
 BSPEC 49 too; x y z
# Specifying an operand field (although ignored) is necessary for a comment
# with a ';' to be ignorable and not interpreted as eoln, both for GAS and
# mmixal.
 ESPEC 0 ignore; x y z
 GREG 50 + 1 naked; x y z
z IS 9 + 8 + 7 comments; x y z
x SWYM 34,21,56
