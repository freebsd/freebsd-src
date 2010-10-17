# Test that we handle weak symbols with base-plus-offset relocs mixed with
# GREG defs.
 .weak w1
 .weak w3
 GREG w4
 GREG w3
 SWYM
w4:
 LDA $42,w1
w3:
 LDA $43,w3
w1:
 LDA $44,w2
w2:
 LDA $45,w4

