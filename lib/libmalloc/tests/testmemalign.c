int
main()
{
    extern char *memalign();
    char *cp;

    if ((cp = memalign(2, 1024)) == 0)
	perror("memalign 2");
    if ((cp = memalign(3, 1024)) == 0)
	perror("memalign 3");
    if ((cp = memalign(4, 1024)) == 0)
	perror("memalign 4");

    return 0;
}

