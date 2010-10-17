# Test parsing of the operands of the cas instruction
	.text
	.globl	foo
foo:	
	cas	%d0,%d1,(%a0)
	cas	%d0,%d1,%a0@
	cas2	%d0:%d2,%d3:%d4,(%a0):(%a1)
	cas2	%d0:%d2,%d3:%d4,(%d0):(%d1)
	cas2	%d0:%d2,%d3:%d4,%a0@:%a1@
	cas2	%d0:%d2,%d3:%d4,@(%a0):@(%a1)
	cas2	%d0:%d2,%d3:%d4,@(%d0):@(%d1)
	cas2	%d0,%d2,%d3,%d4,(%a0),(%a1)
	cas2	%d0,%d2,%d3,%d4,(%d0),(%d1)
	cas2	%d0,%d2,%d3,%d4,%a0@,%a1@
	cas2	%d0,%d2,%d3,%d4,@(%a0),@(%a1)
	cas2	%d0,%d2,%d3,%d4,@(%d0),@(%d1)
