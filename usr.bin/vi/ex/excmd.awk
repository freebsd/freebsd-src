#	@(#)excmd.awk	8.1 (Berkeley) 4/17/94
 
/^\/\* C_[0-9A-Z_]* \*\/$/ {
	printf("#define %s %d\n", $2, cnt++);
	next;
}
