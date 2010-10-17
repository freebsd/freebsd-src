# Test that we handle weak symbols with base-plus-offset relocs.
 .weak w1
 .weak w3
 SWYM
w4:
 LDA $42,w1
w3:
 LDA $43,w3
w1:
 LDA $44,w2
w2:
 LDA $45,w4

