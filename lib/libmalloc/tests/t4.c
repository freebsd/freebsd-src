int
main()
{
    char *cp;
    int i;
    int n = getpagesize();
    extern char *malloc();

    printf("pagesize = %d\n", n);

    for(i = 0; i < 5; i++) {
	cp = malloc(n);
	printf("malloc(%d) returned 0x%x\n", n, cp);
	cp = malloc(2*n);
	printf("malloc(%d) returned 0x%x\n", 2*n, cp);
	cp = malloc(4*n);
	printf("malloc(%d) returned 0x%x\n", 4*n, cp);
    }
    return 0;
}
