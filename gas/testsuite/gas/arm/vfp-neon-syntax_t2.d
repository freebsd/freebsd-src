# name: VFP Neon-style syntax, Thumb mode
# as: -mfpu=vfp3 -I$srcdir/$subdir
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section \.text:
0[0-9a-f]+ <[^>]+> eeb0 0a60 	fcpys	s0, s1
0[0-9a-f]+ <[^>]+> eeb0 0b41 	fcpyd	d0, d1
0[0-9a-f]+ <[^>]+> eeb5 0a00 	fconsts	s0, #80
0[0-9a-f]+ <[^>]+> eeb7 0b00 	fconstd	d0, #112
0[0-9a-f]+ <[^>]+> ee10 0a90 	fmrs	r0, s1
0[0-9a-f]+ <[^>]+> ee00 1a10 	fmsr	s0, r1
0[0-9a-f]+ <[^>]+> ec51 0a11 	fmrrs	r0, r1, {s2, s3}
0[0-9a-f]+ <[^>]+> ec44 2a10 	fmsrr	{s0, s1}, r2, r4
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> eeb0 0a60 	fcpyseq	s0, s1
0[0-9a-f]+ <[^>]+> eeb0 0b41 	fcpydeq	d0, d1
0[0-9a-f]+ <[^>]+> eeb5 0a00 	fconstseq	s0, #80
0[0-9a-f]+ <[^>]+> eeb7 0b00 	fconstdeq	d0, #112
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> ee10 0a90 	fmrseq	r0, s1
0[0-9a-f]+ <[^>]+> ee00 1a10 	fmsreq	s0, r1
0[0-9a-f]+ <[^>]+> ec51 0a11 	fmrrseq	r0, r1, {s2, s3}
0[0-9a-f]+ <[^>]+> ec44 2a10 	fmsrreq	{s0, s1}, r2, r4
0[0-9a-f]+ <[^>]+> eeb1 0ae0 	fsqrts	s0, s1
0[0-9a-f]+ <[^>]+> eeb1 0bc1 	fsqrtd	d0, d1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> eeb1 0ae0 	fsqrtseq	s0, s1
0[0-9a-f]+ <[^>]+> eeb1 0bc1 	fsqrtdeq	d0, d1
0[0-9a-f]+ <[^>]+> eeb0 0ae0 	fabss	s0, s1
0[0-9a-f]+ <[^>]+> eeb0 0bc1 	fabsd	d0, d1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> eeb0 0ae0 	fabsseq	s0, s1
0[0-9a-f]+ <[^>]+> eeb0 0bc1 	fabsdeq	d0, d1
0[0-9a-f]+ <[^>]+> eeb1 0a60 	fnegs	s0, s1
0[0-9a-f]+ <[^>]+> eeb1 0b41 	fnegd	d0, d1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> eeb1 0a60 	fnegseq	s0, s1
0[0-9a-f]+ <[^>]+> eeb1 0b41 	fnegdeq	d0, d1
0[0-9a-f]+ <[^>]+> eeb4 0a60 	fcmps	s0, s1
0[0-9a-f]+ <[^>]+> eeb4 0b41 	fcmpd	d0, d1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> eeb4 0a60 	fcmpseq	s0, s1
0[0-9a-f]+ <[^>]+> eeb4 0b41 	fcmpdeq	d0, d1
0[0-9a-f]+ <[^>]+> eeb4 0ae0 	fcmpes	s0, s1
0[0-9a-f]+ <[^>]+> eeb4 0bc1 	fcmped	d0, d1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> eeb4 0ae0 	fcmpeseq	s0, s1
0[0-9a-f]+ <[^>]+> eeb4 0bc1 	fcmpedeq	d0, d1
0[0-9a-f]+ <[^>]+> ee20 0ac1 	fnmuls	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee21 0b42 	fnmuld	d0, d1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ee20 0ac1 	fnmulseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee21 0b42 	fnmuldeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee00 0ac1 	fnmacs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee01 0b42 	fnmacd	d0, d1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ee00 0ac1 	fnmacseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee01 0b42 	fnmacdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee10 0ac1 	fnmscs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee11 0b42 	fnmscd	d0, d1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ee10 0ac1 	fnmscseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee11 0b42 	fnmscdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee20 0a81 	fmuls	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee21 0b02 	fmuld	d0, d1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ee20 0a81 	fmulseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee21 0b02 	fmuldeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee00 0a81 	fmacs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee01 0b02 	fmacd	d0, d1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ee00 0a81 	fmacseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee01 0b02 	fmacdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee10 0a81 	fmscs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee11 0b02 	fmscd	d0, d1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ee10 0a81 	fmscseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee11 0b02 	fmscdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee30 0a81 	fadds	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee31 0b02 	faddd	d0, d1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ee30 0a81 	faddseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee31 0b02 	fadddeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee30 0ac1 	fsubs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee31 0b42 	fsubd	d0, d1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ee30 0ac1 	fsubseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee31 0b42 	fsubdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> ee80 0a81 	fdivs	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee81 0b02 	fdivd	d0, d1, d2
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ee80 0a81 	fdivseq	s0, s1, s2
0[0-9a-f]+ <[^>]+> ee81 0b02 	fdivdeq	d0, d1, d2
0[0-9a-f]+ <[^>]+> eeb5 0a40 	fcmpzs	s0
0[0-9a-f]+ <[^>]+> eeb5 0b40 	fcmpzd	d0
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> eeb5 0a40 	fcmpzseq	s0
0[0-9a-f]+ <[^>]+> eeb5 0b40 	fcmpzdeq	d0
0[0-9a-f]+ <[^>]+> eeb5 0ac0 	fcmpezs	s0
0[0-9a-f]+ <[^>]+> eeb5 0bc0 	fcmpezd	d0
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> eeb5 0ac0 	fcmpezseq	s0
0[0-9a-f]+ <[^>]+> eeb5 0bc0 	fcmpezdeq	d0
0[0-9a-f]+ <[^>]+> eebd 0ae0 	ftosizs	s0, s1
0[0-9a-f]+ <[^>]+> eebc 0ae0 	ftouizs	s0, s1
0[0-9a-f]+ <[^>]+> eebd 0bc1 	ftosizd	s0, d1
0[0-9a-f]+ <[^>]+> eebc 0bc1 	ftouizd	s0, d1
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> eebd 0ae0 	ftosizseq	s0, s1
0[0-9a-f]+ <[^>]+> eebc 0ae0 	ftouizseq	s0, s1
0[0-9a-f]+ <[^>]+> eebd 0bc1 	ftosizdeq	s0, d1
0[0-9a-f]+ <[^>]+> eebc 0bc1 	ftouizdeq	s0, d1
0[0-9a-f]+ <[^>]+> eebd 0a60 	ftosis	s0, s1
0[0-9a-f]+ <[^>]+> eebc 0a60 	ftouis	s0, s1
0[0-9a-f]+ <[^>]+> eeb8 0ae0 	fsitos	s0, s1
0[0-9a-f]+ <[^>]+> eeb8 0a60 	fuitos	s0, s1
0[0-9a-f]+ <[^>]+> eeb7 0bc1 	fcvtsd	s0, d1
0[0-9a-f]+ <[^>]+> eeb7 0ae0 	fcvtds	d0, s1
0[0-9a-f]+ <[^>]+> eebd 0b41 	ftosid	s0, d1
0[0-9a-f]+ <[^>]+> eebc 0b41 	ftouid	s0, d1
0[0-9a-f]+ <[^>]+> eeb8 0be0 	fsitod	d0, s1
0[0-9a-f]+ <[^>]+> eeb8 0b60 	fuitod	d0, s1
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> eebd 0a60 	ftosiseq	s0, s1
0[0-9a-f]+ <[^>]+> eebc 0a60 	ftouiseq	s0, s1
0[0-9a-f]+ <[^>]+> eeb8 0ae0 	fsitoseq	s0, s1
0[0-9a-f]+ <[^>]+> eeb8 0a60 	fuitoseq	s0, s1
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> eeb7 0bc1 	fcvtsdeq	s0, d1
0[0-9a-f]+ <[^>]+> eeb7 0ae0 	fcvtdseq	d0, s1
0[0-9a-f]+ <[^>]+> eebd 0b41 	ftosideq	s0, d1
0[0-9a-f]+ <[^>]+> eebc 0b41 	ftouideq	s0, d1
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> eeb8 0be0 	fsitodeq	d0, s1
0[0-9a-f]+ <[^>]+> eeb8 0b60 	fuitodeq	d0, s1
0[0-9a-f]+ <[^>]+> eebe 0aef 	ftosls	s0, #1
0[0-9a-f]+ <[^>]+> eebf 0aef 	ftouls	s0, #1
0[0-9a-f]+ <[^>]+> eeba 0aef 	fsltos	s0, #1
0[0-9a-f]+ <[^>]+> eebb 0aef 	fultos	s0, #1
0[0-9a-f]+ <[^>]+> eebe 0bef 	ftosld	d0, #1
0[0-9a-f]+ <[^>]+> eebf 0bef 	ftould	d0, #1
0[0-9a-f]+ <[^>]+> eeba 0bef 	fsltod	d0, #1
0[0-9a-f]+ <[^>]+> eebb 0bef 	fultod	d0, #1
0[0-9a-f]+ <[^>]+> eeba 0a67 	fshtos	s0, #1
0[0-9a-f]+ <[^>]+> eebb 0a67 	fuhtos	s0, #1
0[0-9a-f]+ <[^>]+> eeba 0b67 	fshtod	d0, #1
0[0-9a-f]+ <[^>]+> eebb 0b67 	fuhtod	d0, #1
0[0-9a-f]+ <[^>]+> eebe 0a67 	ftoshs	s0, #1
0[0-9a-f]+ <[^>]+> eebf 0a67 	ftouhs	s0, #1
0[0-9a-f]+ <[^>]+> eebe 0b67 	ftoshd	d0, #1
0[0-9a-f]+ <[^>]+> eebf 0b67 	ftouhd	d0, #1
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> eebe 0aef 	ftoslseq	s0, #1
0[0-9a-f]+ <[^>]+> eebf 0aef 	ftoulseq	s0, #1
0[0-9a-f]+ <[^>]+> eeba 0aef 	fsltoseq	s0, #1
0[0-9a-f]+ <[^>]+> eebb 0aef 	fultoseq	s0, #1
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> eebe 0bef 	ftosldeq	d0, #1
0[0-9a-f]+ <[^>]+> eebf 0bef 	ftouldeq	d0, #1
0[0-9a-f]+ <[^>]+> eeba 0bef 	fsltodeq	d0, #1
0[0-9a-f]+ <[^>]+> eebb 0bef 	fultodeq	d0, #1
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> eeba 0a67 	fshtoseq	s0, #1
0[0-9a-f]+ <[^>]+> eebb 0a67 	fuhtoseq	s0, #1
0[0-9a-f]+ <[^>]+> eeba 0b67 	fshtodeq	d0, #1
0[0-9a-f]+ <[^>]+> eebb 0b67 	fuhtodeq	d0, #1
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> eebe 0a67 	ftoshseq	s0, #1
0[0-9a-f]+ <[^>]+> eebf 0a67 	ftouhseq	s0, #1
0[0-9a-f]+ <[^>]+> eebe 0b67 	ftoshdeq	d0, #1
0[0-9a-f]+ <[^>]+> eebf 0b67 	ftouhdeq	d0, #1
0[0-9a-f]+ <[^>]+> ecd0 1a04 	fldmias	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ecd0 1a04 	fldmias	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ecf0 1a04 	fldmias	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ed70 1a04 	fldmdbs	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ec90 3b08 	vldmia	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> ec90 3b08 	vldmia	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> ecb0 3b08 	vldmia	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ed30 3b08 	vldmdb	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> ecd0 1a04 	fldmiaseq	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ecd0 1a04 	fldmiaseq	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ecf0 1a04 	fldmiaseq	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ed70 1a04 	fldmdbseq	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> ec90 3b08 	vldmiaeq	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> ec90 3b08 	vldmiaeq	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> ecb0 3b08 	vldmiaeq	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ed30 3b08 	vldmdbeq	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ecc0 1a04 	fstmias	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ecc0 1a04 	fstmias	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ece0 1a04 	fstmias	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ed60 1a04 	fstmdbs	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ec80 3b08 	vstmia	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> ec80 3b08 	vstmia	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> eca0 3b08 	vstmia	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ed20 3b08 	vstmdb	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> ecc0 1a04 	fstmiaseq	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ecc0 1a04 	fstmiaseq	r0, {s3-s6}
0[0-9a-f]+ <[^>]+> ece0 1a04 	fstmiaseq	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> ed60 1a04 	fstmdbseq	r0!, {s3-s6}
0[0-9a-f]+ <[^>]+> bf01      	itttt	eq
0[0-9a-f]+ <[^>]+> ec80 3b08 	vstmiaeq	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> ec80 3b08 	vstmiaeq	r0, {d3-d6}
0[0-9a-f]+ <[^>]+> eca0 3b08 	vstmiaeq	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ed20 3b08 	vstmdbeq	r0!, {d3-d6}
0[0-9a-f]+ <[^>]+> ed90 0a01 	flds	s0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> ed90 0b01 	vldr	d0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ed90 0a01 	fldseq	s0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> ed90 0b01 	vldreq	d0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> ed80 0a01 	fsts	s0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> ed80 0b01 	vstr	d0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> bf04      	itt	eq
0[0-9a-f]+ <[^>]+> ed80 0a01 	fstseq	s0, \[r0, #4\]
0[0-9a-f]+ <[^>]+> ed80 0b01 	vstreq	d0, \[r0, #4\]
