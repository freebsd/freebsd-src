@ VFP/Neon overlapping instructions

	.arm
	.text
	.syntax unified

	fmdrr d0,r0,r1
        vmov d0,r0,r1
        fmrrd r0,r1,d0
        vmov r0,r1,d0

	@ the 'x' versions should disassemble as VFP instructions, because
        @ they can't be represented in Neon syntax.

	fldmiax r0,{d0-d3}
        fldmdbx r0!,{d0-d3}
        fstmiax r0,{d0-d3}
        fstmdbx r0!,{d0-d3}

	fldd d0,[r0]
        vldr d0,[r0]
        fstd d0,[r0]
        vstr d0,[r0]

	fldmiad r0,{d0-d3}
        vldmia r0,{d0-d3}
        fldmdbd r0!,{d0-d3}
        vldmdb r0!,{d0-d3}
        fstmiad r0,{d0-d3}
        vstmia r0,{d0-d3}
        fstmdbd r0!,{d0-d3}
        vstmdb r0!,{d0-d3}

	fmrdh r0,d0
        vmov.32 r0,d0[1]
        fmrdl r0,d0
        vmov.32 r0,d0[0]
	fmdhr d0,r0
        vmov.32 d0[1],r0
        fmdlr d0,r0
        vmov.32 d0[0],r0
