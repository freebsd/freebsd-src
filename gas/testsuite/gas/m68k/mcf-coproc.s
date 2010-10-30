
start:
	cp0bcbusy  zero
	cp0ld %d0,%d2,#1,#0x123
	cp0ldl %a0,%a2,#2,#0x1
	cp0ldw (%a0),%a2,#3,#0x1
	cp0ldb (%a0)+,%a2,#6,#0x1
	cp0ldl -(%a0),%a2,#7,#0x1
	cp0ldl 16(%a0),%a2,#8,#0x1
	
	cp0st %d2,%d0,#1,#0x123
	cp0stl %a2,%a0,#2,#0x1
	cp0stw %a2,(%a0),#3,#0x1
	cp0stb %a2,(%a0)+,#6,#0x1
	cp0stl %a2,-(%a0),#7,#0x1
	cp0stl %a2,16(%a0),#8,#0x1
	
	cp0nop #8
	cp0ld %d0,%d0,#3,#0
	cp0ld %d0,%d1,#3,#0
	cp0ld %a0,%d0,#3,#0
	cp0ld (%a0),%d0,#3,#0
	cp0ld 16(%a0),%d0,#3,#0
zero:	nop
	
	cp1bcbusy  one
	cp1ld %d0,%d2,#1,#0x123
	cp1ldl %a0,%a2,#2,#0x1
	cp1ldw (%a0),%a2,#3,#0x1
	cp1ldb (%a0)+,%a2,#6,#0x1
	cp1ldl -(%a0),%a2,#7,#0x1
	cp1ldl 16(%a0),%a2,#8,#0x1
	
	cp1st %d2,%d0,#1,#0x123
	cp1stl %a2,%a0,#2,#0x1
	cp1stw %a2,(%a0),#3,#0x1
	cp1stb %a2,(%a0)+,#6,#0x1
	cp1stl %a2,-(%a0),#7,#0x1
	cp1stl %a2,16(%a0),#8,#0x1
	
	cp1nop #8
	cp1ld %d0,%d0,#3,#0
	cp1ld %d0,%d1,#3,#0
	cp1ld %a0,%d0,#3,#0
	cp1ld (%a0),%d0,#3,#0
	cp1ld 16(%a0),%d0,#3,#0
one:	nop
