        .section  .text
        .global _fun

xc16x_jmpr:

	jmpr cc_uc, xc16x_jmpr
	jmpr cc_z,  xc16x_jmpr
	jmpr cc_nz, xc16x_jmpr
	jmpr cc_v,  xc16x_jmpr
	jmpr cc_nv, xc16x_jmpr
	jmpr cc_n,  xc16x_jmpr
	jmpr cc_nn, xc16x_jmpr
	jmpr cc_c,  xc16x_jmpr
	jmpr cc_nc, xc16x_jmpr
	jmpr cc_eq, xc16x_jmpr
	jmpr cc_ne, xc16x_jmpr
	jmpr cc_ult,xc16x_jmpr
	jmpr cc_ule,xc16x_jmpr
	jmpr cc_uge,xc16x_jmpr
	jmpr cc_ugt,xc16x_jmpr
	jmpr cc_sle,xc16x_jmpr
	jmpr cc_sge,xc16x_jmpr
	jmpr cc_sgt,xc16x_jmpr
	jmpr cc_net,xc16x_jmpr
	jmpr cc_slt,xc16x_jmpr	
