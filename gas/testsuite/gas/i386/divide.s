start:
 .long 1,2,3,a,b
/ This comment should still be allowed with --divide,
/ but the divide must remain a divide in the next line
a=(.-start)/4-1 # comment
b=(.-start)/4
