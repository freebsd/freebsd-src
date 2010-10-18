# VIA Nehemiah PadLock instructions

	.text
foo:
	xstorerng
	rep xstorerng
	xcryptecb
	rep xcryptecb
	xcryptcbc
	rep xcryptcbc
	xcryptcfb
	rep xcryptcfb
	xcryptofb
	rep xcryptofb
	xstore
	rep xstore
	montmul
	rep montmul
	xsha1
	rep xsha1
	xsha256
	rep xsha256

	.p2align 4,0
