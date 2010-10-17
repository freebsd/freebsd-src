	.text
	.global	xxx
xxx:
	ptabs	r18, tr0
	blink	tr0, r63
	.global	yyy
yyy:
	movi	((xxx@GOTPLT) & 65535), r1
