% JMP with a large constant must not fail
i1	IS #ffff0000ffff0000
Main JMP  i1
 JMP  i2
i2	IS #ffff0000ffff0000
