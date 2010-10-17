# Check that we allow zero operands for some insns.
# Naked comments aren't supported when no operands are supplied; this
# matches mmixal behavior.
Main TRAP
 SWYM
 TRIP
 JMP
 POP 123456
 POP
