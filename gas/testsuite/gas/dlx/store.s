.text
2:  sw      L1(r0),%3
    sw      %hi(L1)(r0),%3
1:  sh      (1b - 2b) + 8 - ((5f - 4f)<<4)[r2],r3
4:  sb	    4b+'0',$3
4:  sb	    L1+'0'-L1,$3
5:  sw      %hi((L1 - 2b) + 8 + ((5b - 4b)<<4))(r2),%3
    nop
L1: nop
