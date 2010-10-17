        .section s1, "ax"
foo:
        add r1,r11

        .section s2, "ax"
bar:
	add r2,r11 || add r3,r11
	add r2,r11 -> add r3,r11
	add r2,r11 <- add r3,r11

