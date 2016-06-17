/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 *  This program may be freely redistributed,
 *  but this entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, 1989, William LeFebvre, Rice University
 *  Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *  Copyright (c) 2016, Randy Westlund
 *
 * $FreeBSD$
 */
#ifndef USERNAME_H
#define USERNAME_H

int	 enter_user(int uid, char *name, int wecare);
int	 get_user(int uid);
void	 init_hash(void);
char 	*username(int uid);
int 	 userid(char *username);

#endif /* USERNAME_H */
