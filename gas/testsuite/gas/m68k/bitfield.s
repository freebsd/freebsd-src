# Test handling of bitfield instruction operands.
	.text
	.globl	foo
foo:	
	bfexts	(%a0){&1:&2},%d0
	bfexts	(%a0){&1:&(2+4)},%d0
	bfexts	(%a0){&(1+2):&2},%d0
	bfexts	(%a0){&(1+2):&(2+4)},%d0
	bfexts	%a0@,&1,&2,%d0
	bfexts	%a0@,&1,&(2+4),%d0
	bfexts	%a0@,&1+2,&2,%d0
	bfexts	%a0@,&(1+2),&(2+4),%d0
	bfset	(%a0){&1:&2}
	bfset	(%a0){&1:&(2+4)}
	bfset	(%a0){&(1+2):&2}
	bfset	(%a0){&(1+2):&(2+4)}
	bfset	%a0@,&1,&2
	bfset	%a0@,&1,&(2+4)
	bfset	%a0@,&1+2,&2
	bfset	%a0@,&(1+2),&(2+4)
	bfexts	(%a0){%d1:%d2},%d0
	bfexts	%a0@,%d1,%d2,%d0
	bfset	(%a0){%d1:%d2}
	bfset	%a0@,%d1,%d2
