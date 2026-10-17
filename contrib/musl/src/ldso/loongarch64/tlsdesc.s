.text
.global __tlsdesc_static
.hidden __tlsdesc_static
.type __tlsdesc_static,%function
__tlsdesc_static:
	ld.d $a0, $a0, 8
	jr $ra
# size_t __tlsdesc_dynamic(size_t *a)
# {
#      struct {size_t modidx,off;} *p = (void*)a[1];
#      size_t *dtv = *(size_t**)(tp - 8);
#      return dtv[p->modidx] + p->off - tp;
# }
.global __tlsdesc_dynamic
.hidden __tlsdesc_dynamic
.type __tlsdesc_dynamic,%function
__tlsdesc_dynamic:
	addi.d $sp, $sp, -16
	st.d $t1, $sp, 0
	st.d $t2, $sp, 8

	ld.d $t2, $tp, -8 # t2=dtv

	ld.d $a0, $a0, 8  # a0=&{modidx,off}
	ld.d $t1, $a0, 8  # t1=off
	ld.d $a0, $a0, 0  # a0=modidx
	slli.d $a0, $a0, 3  # a0=8*modidx

	add.d $a0, $a0, $t2  # a0=dtv+8*modidx
	ld.d $a0, $a0, 0  # a0=dtv[modidx]
	add.d $a0, $a0, $t1 # a0=dtv[modidx]+off
	sub.d $a0, $a0, $tp # a0=dtv[modidx]+off-tp

	ld.d $t1, $sp, 0
	ld.d $t2, $sp, 8
	addi.d $sp, $sp, 16
	jr $ra
