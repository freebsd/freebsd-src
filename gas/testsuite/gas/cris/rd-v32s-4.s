; Check special registers specified as pN.

 .text
here:
 move $r3,$p0
 move $r5,$p1
 move $r6,$p2
 move $r7,$p3
 move $r8,$p4
 move $r9,$p5
 move $r5,$p6
 move $r6,$p7
 move $r7,$p8
 move $r2,$p9
 move $r4,$p10
 move $r0,$p11
 move $r6,$p12
 move $r10,$p13
 move $r12,$p14
 move $r13,$p15

 move $p0,$r3
 move $p1,$r5
 move $p2,$r6
 move $p3,$r7
 move $p4,$r8
 move $p5,$r9
 move $p6,$r5
 move $p7,$r6
 move $p8,$r7
 move $p9,$r2
 move $p10,$r4
 move $p11,$r0
 move $p12,$r6
 move $p13,$r10
 move $p14,$r12
 move $p15,$r13

 move 3,$p0
 move 5,$p1
 move 6,$p2
 move 7,$p3
 move 8,$p4
 move 9,$p5
 move 10,$p6
 move 101,$p7
 move 120,$p8
 move 13,$p9
 move 4,$p10
 move 0,$p11
 move 6,$p12
 move 10,$p13
 move 12,$p14
 move 13,$p15

 move $p0,[$r3]
 move $p1,[$r5]
 move $p2,[$r6]
 move $p3,[$r7]
 move $p4,[$r8]
 move $p5,[$r9]
 move $p6,[$r5]
 move $p7,[$r6]
 move $p8,[$r7]
 move $p9,[$r2]
 move $p10,[$r4]
 move $p11,[$r0]
 move $p12,[$r6]
 move $p13,[$r10]
 move $p14,[$r12]
 move $p15,[$r13]

 move [$r3],$p0
 move [$r5],$p1
 move [$r6],$p2
 move [$r7],$p3
 move [$r8],$p4
 move [$r9],$p5
 move [$r5],$p6
 move [$r6],$p7
 move [$r7],$p8
 move [$r2],$p9
 move [$r4],$p10
 move [$r0],$p11
 move [$r6],$p12
 move [$r10],$p13
 move [$r12],$p14
 move [$r13],$p15
