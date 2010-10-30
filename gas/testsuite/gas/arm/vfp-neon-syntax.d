# name: VFP Neon-style syntax, ARM mode
# as: -mfpu=vfp3 -I$srcdir/$subdir
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> eeb00a60 	fcpys	s0, s1
0[0-9a-f]+ <[^>]+> eeb00b41 	fcpyd	d0, d1
0[0-9a-f]+ <[^>]+> eeb50a00 	fconsts	s0, #80
0[0-9a-f]+ <[^>]+> eeb70b00 	fconstd	d0, #112
0[0-9a-f]+ <[^>]+> ee100a90 	fmrs	r0, s1
0[0-9a-f]+ <[^>]+> ee001a10 	fmsr	s0, r1
0[0-9a-f]+ <[^>]+> ec510a11 	fmrrs	r0, r1, {s2, s3}
0[0-9a-f]+ <[^>]+> ec442a10 	fmsrr	{s0, s1}, r2, r4
0[0-9a-f]+ <[^>]+> 0eb00a60 	fcpyseq	s0, s1
0[0-9a-f]+ <[^>]+> 0eb00b41 	fcpydeq	d0, d1
0[0-9a-f]+ <[^>]+> 0eb50a00 	fconstseq	s0, #80
0[0-9a-f]+ <[^>]+> 0eb70b00 	fconstdeq	d0, #112
0[0-9a-f]+ <[^>]+> 0e100a90 	fmrseq	r0, s1
0[0-9a-f]+ <[^>]+> 0e001a10 	fmsreq	s0, r1
0[0-9a-f]+ <[^>]+> 0c510a11 	fmrrseq	r0, r1, {s2, s3}
0[0-9a-f]+ <[^>]+> 0c442a10 	fmsrreq	{s0, s1}, r2, r4
0[0-9a-f]+ <[^>]+> eeb10ae0 	fsqrts	s0, s1
0[0-9a-f]+ <[^>]+> eeb10bc1 	fsqrtd	d0, d1
0[0-9a-f]+ <[^>]+> 0eb10ae0 	fsqrtseq	s0, s1
0[0-9a-f]+ <[^>]+> 0eb10bc1 	fsqrtdeq	d0, d1
0[0-9a-f]+ <[^>]+> eeb00ae0 	fabss	s0, s1
0[0-9a-f]+ <[^>]+> eeb00bc1 	fabsd	d0, d1
0[0-9a-f]+ <[^>]+> 0eb00ae0 	fabsseq	s0, s1
0[0-9a-f]+ <[^>]+> 0eb00bc1 	fabsdeq	d0, d1
0[0-9a-f]+ <[^>]+> eeb10a60 	fnegs	s0, s1
0[0-9a-f]+ <[^>]+> eeb10b41 	fnegd	d0, d1
0[0-9a-f]+ <[^>]+> 0eb10a60 	fnegseq	s0, s1
0[0-9a-f]+ <[^>]+> 0eb10b41 	fnegdeq	d0, d1
0[0-9a-f]+ <[^>]+> eeb40a60 	fcmps	s0, s1
0[0-9a-f]+ <[^>]+> eeb40b41 	fcmpd	d0, d1
0[0-9a-f]+ <[^>]+> 0eb40a60 	fcmpseq	s0, s1
0[0-9a-f]+ <[^>]+> 0eb40b41 	fcmpdeq	d0, d1
0[0-9a-f]+ <[^>]+> eeb40ae0 	fcmpes	s0, s1
0[0-9a-f]+ <[^>]+> eeb40bc1 	fcmped	d0, d1
0[0-9a-f]+ <[^>]+> 0eb40ae0 	fcmpeseq	s0, s1
0[0-9a-f]+ <[^>]+> 0eb40bc1 	fcmpedeq	d0, d1
0[0-9a-f]+ <[^>]+> ee200ac1 	fnmuls	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee210b42 	fnmuld	d0, d1, d2
0[0-9a-f]+ <[^>]+> 0e200ac1 	fnmulseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> 0e210b42 	fnmuldeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee000ac1 	fnmacs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee010b42 	fnmacd	d0, d1, d2
0[0-9a-f]+ <[^>]+> 0e000ac1 	fnmacseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> 0e010b42 	fnmacdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee100ac1 	fnmscs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee110b42 	fnmscd	d0, d1, d2
0[0-9a-f]+ <[^>]+> 0e100ac1 	fnmscseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> 0e110b42 	fnmscdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee200a81 	fmuls	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee210b02 	fmuld	d0, d1, d2
0[0-9a-f]+ <[^>]+> 0e200a81 	fmulseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> 0e210b02 	fmuldeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee000a81 	fmacs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee010b02 	fmacd	d0, d1, d2
0[0-9a-f]+ <[^>]+> 0e000a81 	fmacseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> 0e010b02 	fmacdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee100a81 	fmscs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee110b02 	fmscd	d0, d1, d2
0[0-9a-f]+ <[^>]+> 0e100a81 	fmscseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> 0e110b02 	fmscdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee300a81 	fadds	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee310b02 	faddd	d0, d1, d2
0[0-9a-f]+ <[^>]+> 0e300a81 	faddseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> 0e310b02 	fadddeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee300ac1 	fsubs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee310b42 	fsubd	d0, d1, d2
0[0-9a-f]+ <[^>]+> 0e300ac1 	fsubseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> 0e310b42 	fsubdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee800a81 	fdivs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee810b02 	fdivd	d0, d1, d2
0[0-9a-f]+ <[^>]+> 0e800a81 	fdivseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> 0e810b02 	fdivdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> eeb50a40 	fcmpzs	s0
0[0-9a-f]+ <[^>]+> eeb50b40 	fcmpzd	d0
0[0-9a-f]+ <[^>]+> 0eb50a40 	fcmpzseq	s0
0[0-9a-f]+ <[^>]+> 0eb50b40 	fcmpzdeq	d0
0[0-9a-f]+ <[^>]+> eeb50ac0 	fcmpezs	s0
0[0-9a-f]+ <[^>]+> eeb50bc0 	fcmpezd	d0
0[0-9a-f]+ <[^>]+> 0eb50ac0 	fcmpezseq	s0
0[0-9a-f]+ <[^>]+> 0eb50bc0 	fcmpezdeq	d0
0[0-9a-f]+ <[^>]+> eebd0ae0 	ftosizs	s0, s1
0[0-9a-f]+ <[^>]+> eebc0ae0 	ftouizs	s0, s1
0[0-9a-f]+ <[^>]+> eebd0bc1 	ftosizd	s0, d1
0[0-9a-f]+ <[^>]+> eebc0bc1 	ftouizd	s0, d1
0[0-9a-f]+ <[^>]+> 0ebd0ae0 	ftosizseq	s0, s1
0[0-9a-f]+ <[^>]+> 0ebc0ae0 	ftouizseq	s0, s1
0[0-9a-f]+ <[^>]+> 0ebd0bc1 	ftosizdeq	s0, d1
0[0-9a-f]+ <[^>]+> 0ebc0bc1 	ftouizdeq	s0, d1
0[0-9a-f]+ <[^>]+> eebd0a60 	ftosis	s0, s1
0[0-9a-f]+ <[^>]+> eebc0a60 	ftouis	s0, s1
0[0-9a-f]+ <[^>]+> eeb80ae0 	fsitos	s0, s1
0[0-9a-f]+ <[^>]+> eeb80a60 	fuitos	s0, s1
0[0-9a-f]+ <[^>]+> eeb70bc1 	fcvtsd	s0, d1
0[0-9a-f]+ <[^>]+> eeb70ae0 	fcvtds	d0, s1
0[0-9a-f]+ <[^>]+> eebd0b41 	ftosid	s0, d1
0[0-9a-f]+ <[^>]+> eebc0b41 	ftouid	s0, d1
0[0-9a-f]+ <[^>]+> eeb80be0 	fsitod	d0, s1
0[0-9a-f]+ <[^>]+> eeb80b60 	fuitod	d0, s1
0[0-9a-f]+ <[^>]+> 0ebd0a60 	ftosiseq	s0, s1
0[0-9a-f]+ <[^>]+> 0ebc0a60 	ftouiseq	s0, s1
0[0-9a-f]+ <[^>]+> 0eb80ae0 	fsitoseq	s0, s1
0[0-9a-f]+ <[^>]+> 0eb80a60 	fuitoseq	s0, s1
0[0-9a-f]+ <[^>]+> 0eb70bc1 	fcvtsdeq	s0, d1
0[0-9a-f]+ <[^>]+> 0eb70ae0 	fcvtdseq	d0, s1
0[0-9a-f]+ <[^>]+> 0ebd0b41 	ftosideq	s0, d1
0[0-9a-f]+ <[^>]+> 0ebc0b41 	ftouideq	s0, d1
0[0-9a-f]+ <[^>]+> 0eb80be0 	fsitodeq	d0, s1
0[0-9a-f]+ <[^>]+> 0eb80b60 	fuitodeq	d0, s1
0[0-9a-f]+ <[^>]+> eebe0aef 	ftosls	s0, #1
0[0-9a-f]+ <[^>]+> eebf0aef 	ftouls	s0, #1
0[0-9a-f]+ <[^>]+> eeba0aef 	fsltos	s0, #1
0[0-9a-f]+ <[^>]+> eebb0aef 	fultos	s0, #1
0[0-9a-f]+ <[^>]+> eebe0bef 	ftosld	d0, #1
0[0-9a-f]+ <[^>]+> eebf0bef 	ftould	d0, #1
0[0-9a-f]+ <[^>]+> eeba0bef 	fsltod	d0, #1
0[0-9a-f]+ <[^>]+> eebb0bef 	fultod	d0, #1
0[0-9a-f]+ <[^>]+> eeba0a67 	fshtos	s0, #1
0[0-9a-f]+ <[^>]+> eebb0a67 	fuhtos	s0, #1
0[0-9a-f]+ <[^>]+> eeba0b67 	fshtod	d0, #1
0[0-9a-f]+ <[^>]+> eebb0b67 	fuhtod	d0, #1
0[0-9a-f]+ <[^>]+> eebe0a67 	ftoshs	s0, #1
0[0-9a-f]+ <[^>]+> eebf0a67 	ftouhs	s0, #1
0[0-9a-f]+ <[^>]+> eebe0b67 	ftoshd	d0, #1
0[0-9a-f]+ <[^>]+> eebf0b67 	ftouhd	d0, #1
0[0-9a-f]+ <[^>]+> 0ebe0aef 	ftoslseq	s0, #1
0[0-9a-f]+ <[^>]+> 0ebf0aef 	ftoulseq	s0, #1
0[0-9a-f]+ <[^>]+> 0eba0aef 	fsltoseq	s0, #1
0[0-9a-f]+ <[^>]+> 0ebb0aef 	fultoseq	s0, #1
0[0-9a-f]+ <[^>]+> 0ebe0bef 	ftosldeq	d0, #1
0[0-9a-f]+ <[^>]+> 0ebf0bef 	ftouldeq	d0, #1
0[0-9a-f]+ <[^>]+> 0eba0bef 	fsltodeq	d0, #1
0[0-9a-f]+ <[^>]+> 0ebb0bef 	fultodeq	d0, #1
0[0-9a-f]+ <[^>]+> 0eba0a67 	fshtoseq	s0, #1
0[0-9a-f]+ <[^>]+> 0ebb0a67 	fuhtoseq	s0, #1
0[0-9a-f]+ <[^>]+> 0eba0b67 	fshtodeq	d0, #1
0[0-9a-f]+ <[^>]+> 0ebb0b67 	fuhtodeq	d0, #1
0[0-9a-f]+ <[^>]+> 0ebe0a67 	ftoshseq	s0, #1
0[0-9a-f]+ <[^>]+> 0ebf0a67 	ftouhseq	s0, #1
0[0-9a-f]+ <[^>]+> 0ebe0b67 	ftoshdeq	d0, #1
0[0-9a-f]+ <[^>]+> 0ebf0b67 	ftouhdeq	d0, #1
0[0-9a-f]+ <[^>]+> ecd01a04 	fldmias	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ecd01a04 	fldmias	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ecf01a04 	fldmias	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ed701a04 	fldmdbs	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ec903b08 	vldmia	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> ec903b08 	vldmia	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> ecb03b08 	vldmia	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ed303b08 	vldmdb	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> 0cd01a04 	fldmiaseq	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> 0cd01a04 	fldmiaseq	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> 0cf01a04 	fldmiaseq	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> 0d701a04 	fldmdbseq	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> 0c903b08 	vldmiaeq	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> 0c903b08 	vldmiaeq	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> 0cb03b08 	vldmiaeq	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> 0d303b08 	vldmdbeq	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ecc01a04 	fstmias	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ecc01a04 	fstmias	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ece01a04 	fstmias	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ed601a04 	fstmdbs	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ec803b08 	vstmia	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> ec803b08 	vstmia	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> eca03b08 	vstmia	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ed203b08 	vstmdb	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> 0cc01a04 	fstmiaseq	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> 0cc01a04 	fstmiaseq	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> 0ce01a04 	fstmiaseq	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> 0d601a04 	fstmdbseq	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> 0c803b08 	vstmiaeq	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> 0c803b08 	vstmiaeq	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> 0ca03b08 	vstmiaeq	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> 0d203b08 	vstmdbeq	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ed900a01 	flds	s0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> ed900b01 	vldr	d0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> 0d900a01 	fldseq	s0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> 0d900b01 	vldreq	d0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> ed800a01 	fsts	s0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> ed800b01 	vstr	d0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> 0d800a01 	fstseq	s0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> 0d800b01 	vstreq	d0, \[r0, #4\]
