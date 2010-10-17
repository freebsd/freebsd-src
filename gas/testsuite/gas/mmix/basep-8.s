# Test that we handle COMM-type symbols with base-plus-offset relocs.
 .comm comm_symbol1,4,4
 .lcomm comm_symbol3,4
 LDA $42,comm_symbol1
 LDA $43,comm_symbol2
 LDA $44,comm_symbol3
 LDA $45,comm_symbol4
 .comm comm_symbol2,4,4
 .lcomm comm_symbol4,4
