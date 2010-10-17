! There was a bug with negative pc-relative numbers.
	.mode SHmedia
	.text
start:
	movi (start - 1000000 - end) & 65535,r4
	movi ((start - 1000000 - end) >> 16) & 65535,r5
	movi ((start - 1000000 - end) >> 32) & 65535,r6
	movi ((start - 1000000 - end) >> 48) & 65535,r7
	movi (start - 1000000 - end),r8
end:
