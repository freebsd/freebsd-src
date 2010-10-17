% { dg-do assemble { target mmix-*-* } }

# Test that we handle COMM-type symbols with base-plus-offset relocs, but
# that we don't merge ones that may be separately merged with other
# symbols at link-time.  Likewise for weak symbols.
 .comm comm_symbol1,4,4
 .lcomm comm_symbol3,4
 GREG comm_symbol1
 GREG comm_symbol3
 GREG xx
 .weak xx
xx:
 LDA $47,yy		% { dg-error "no suitable GREG definition" "" }
 LDA $46,xx
 LDA $42,comm_symbol1
 LDA $43,comm_symbol2	% { dg-error "no suitable GREG definition" "" }
 LDA $44,comm_symbol3
 LDA $45,comm_symbol4
yy:
 .comm comm_symbol2,4,4
 .lcomm comm_symbol4,4
