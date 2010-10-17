# Test that we handle COMM-type symbols with base-plus-offset relocs.
 .comm comm_symbol1,4,4
 .lcomm comm_symbol3,4
 GREG comm_symbol1
 GREG comm_symbol3
 LDA $42,comm_symbol1
 LDA $44,comm_symbol3
 LDA $45,comm_symbol4
 .lcomm comm_symbol4,4
