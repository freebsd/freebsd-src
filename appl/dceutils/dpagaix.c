/*
 * dpagaix.c
 * On AIX we need to get the kernel extentions
 * with the DFS kafs_syscall in it.
 * We might be running on a system
 * where DFS is not active.
 * So we use this dummy routine which
 * might not load to do the dirty work
 *
 * DCE does this with the /usr/lib/drivers/dfsloadobj
 *
 */

 int dpagaix(parm1, parm2, parm3, parm4, parm5, parm6)
   int parm1;
   int parm2;
   int parm3;
   int parm4;
   int parm5;
   int parm6;
 {
   return(kafs_syscall(parm1, parm2, parm3, parm4, parm5, parm6));
 }
