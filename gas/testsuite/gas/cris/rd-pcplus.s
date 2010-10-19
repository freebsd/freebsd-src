 .text
a:
 move [$pc=$r7+$r1.b],$srp
 move [$pc=$pc+65536],$p0
 move.d [$pc=$r3+$r8.b],$r5
 move.d [$pc=$pc+65536],$r11
 move [$pc=$r5+655360],$p1
 move.d [$pc=$r7+655],$r13
 move.d [$pc=$r6+65],$r10
 nop
