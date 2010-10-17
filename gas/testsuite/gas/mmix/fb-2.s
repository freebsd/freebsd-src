# Test fb-label where the insn or pseudo on line with definition uses an
# argment with a same-number label.
1H IS 5
0H LOC #10
1H BYTE 1B
0H LOC 0B+#20+0F
0H IS 4
1H IS 50
1H GREG 1B+1F
 SWYM
1H LDA $30,1B
1H OCTA 1B,1F
1H SWYM

9H IS 42
 WYDE 9B,9F
9H IS 9B+1
 WYDE 9B,9F
9H IS 9B+1
