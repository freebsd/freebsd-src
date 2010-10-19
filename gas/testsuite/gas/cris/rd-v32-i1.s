; Test that addc recognizes constant operands; [pc+]
x:
 addc -1,r10
 addc 0x40,acr
 addc 1,r5
 addc extsym+320,r7
 addc 0,r0
 addc [pc+],r4
 .dword 20021991
 addc 15,acr
