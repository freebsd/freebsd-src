/* glob.h */

#ifndef _glob_h_
#define _glob_h_ 1

/*  glob.h,v
 *  1.1.1.1
 *  1994/09/22 23:45:35
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

