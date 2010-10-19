.section  .text
.global   _fun

xc16x_jmpi:

	jmpi cc_UC, [r7]
	jmpi cc_z,  [r7]
	jmpi cc_NZ, [r7]
	jmpi cc_V,  [r7]
	jmpi cc_NV, [r7]
	jmpi cc_N,  [r7]
	jmpi cc_NN, [r7]
	jmpi cc_C,  [r7]
	jmpi cc_NC, [r7]
	jmpi cc_EQ, [r7]
	jmpi cc_NE, [r7]
	jmpi cc_ULT,[r7]
	jmpi cc_ULE,[r7]
	jmpi cc_UGE,[r7]
	jmpi cc_UGT,[r7]
	jmpi cc_SLE,[r7]
	jmpi cc_SGE,[r7]
	jmpi cc_SGT,[r7]
	jmpi cc_NET,[r7]
