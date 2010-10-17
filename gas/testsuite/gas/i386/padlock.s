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

	.p2align 4,0
