/* glob.h */

#ifndef _glob_h_
#define _glob_h_ 1

/*  $RCSfile: glob.h,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/05/21 05:45:32 $
 */

char **glob(char *v);
int letter(char c);
int digit(char c);
int any(int c, char *s);
int blklen(char **av);
char **blkcpy(char **oav, char **bv);
void blkfree(char **av0);
char **copyblk(char **v);
int gethdir(char *home_dir);

#endif

