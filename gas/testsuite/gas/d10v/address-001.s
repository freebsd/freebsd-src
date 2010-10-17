	;;
	;; address-001.s
	;; Test supported indirect addressing
	;; 

    .text	
    .global main
main:
	;;
	;; Indirect
	;;
        ldb  r0,@r2
        ldub r0,@r2
        ld   r0,@r2
        ld2w r0,@r2
        stb  r0,@r2
        st   r0,@r2
        st2w r0,@r2

	;;
	;; Indirect with post increment
	;; 
        ld   r0,@r2+
        ld2w r0,@r2+
        st   r0,@r2+
        st2w r0,@r2+

	;;	
	;; Indirect with postdecrement
	;; 
        ld   r0,@r2-
        ld2w r0,@r2-
        st   r0,@r2-
        st2w r0,@r2-

	;;
	;; Indirect through stackpointer
	;; 
        ldb  r0,@sp
        ldub r0,@sp
        ld   r0,@sp
        ld2w r0,@sp
        stb  r0,@sp
        st   r0,@sp
        st2w r0,@sp

	;; 
	;; Indirect through stackpointer with postincrement
	;; 
        ld   r0,@sp+
        ld2w r0,@sp+
        st   r0,@sp+
        st2w r0,@sp+

	;;
	;; Indirect through stackpointer with postdecrement
	;; 
        ld   r0,@sp-
        ld2w r0,@sp-

	;;
	;; Indirect through stackpointer with predecrement
	;; 
        st   r0,@-sp
        st2w r0,@-sp

	;;
	;; Indirect with displacement
	;; 
        ldb  r0,@(0x8000,r2)
        ldub r0,@(0x8000,r2)
        ld   r0,@(0x8000,r2)
        ld2w r0,@(0x8000,r2)
        stb  r0,@(0x8000,r2)
        st   r0,@(0x8000,r2)
        st2w r0,@(0x8000,r2)

	jmp r13
