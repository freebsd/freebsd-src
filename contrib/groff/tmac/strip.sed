# $FreeBSD$
/%beginstrip%/,$s/[	 ]*\\".*//
/^\.$/d
/%comment%/s/%comment%/.\\"/
