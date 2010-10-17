# System and privileged instructions
# ld.c, st.c, flush, lock, unlock, intovr, trap

	.text

	lock
	unlock
	intovr

	trap	%r0,%r0,%r0
	trap	%r31,%r31,%r31
	trap	%r1,%r5,%r18
	trap	%r31,%r20,%r6

	ld.c	%fir,%r1
	ld.c	%fir,%r31
	ld.c	%psr,%r5
	ld.c	%psr,%r30
	ld.c	%dirbase,%r10
	ld.c	%dirbase,%r2
	ld.c	%db,%r21
	ld.c	%db,%r0
	ld.c	%fsr,%r28
	ld.c	%fsr,%r12
	ld.c	%epsr,%r31
	ld.c	%epsr,%r6

	st.c	%r0,%fir
	st.c	%r30,%fir
	st.c	%r7,%psr
	st.c	%r31,%psr
	st.c	%r11,%dirbase
	st.c	%r3,%dirbase
	st.c	%r22,%db
	st.c	%r15,%db
	st.c	%r29,%fsr
	st.c	%r13,%fsr
	st.c	%r4,%epsr
	st.c	%r6,%epsr

	# Flush, no auto-increment.
	flush	0(%r0)
	flush	128(%r1)
	flush	256(%r2)
	flush	512(%r3)
	flush	1024(%r4)
	flush	4096(%r5)
	flush	8192(%r6)
	flush	16384(%r7)
	flush	-16384(%r8)
	flush	-8192(%r9)
	flush	-4096(%r10)
	flush	-1024(%r11)
	flush	-512(%r12)
	flush	-248(%r13)
	flush	-32(%r14)
	flush	-16(%r14)

	# Flush, auto-increment.	
	flush	0(%r0)++
	flush	128(%r1)++
	flush	256(%r2)++
	flush	512(%r3)++
	flush	1024(%r4)++
	flush	4096(%r22)++
	flush	8192(%r23)++
	flush	16384(%r24)++
	flush	-16384(%r25)++
	flush	-8192(%r26)++
	flush	-4096(%r27)++
	flush	-1024(%r28)++
	flush	-512(%r29)++
	flush	-248(%r30)++
	flush	32(%r31)++
	flush	16(%r31)++

