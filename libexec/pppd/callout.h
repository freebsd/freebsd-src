/* Note:  This is a copy of /usr/include/sys/callout.h with the c_func */
/* member of struct callout changed from a pointer to a function of type int*/
/* to a pointer to a function of type void (generic pointer) as per */
/* ANSI C */

/* $Id: callout.h,v 1.2 1994/03/30 09:31:22 jkh Exp $ */

#ifndef _ppp_callout_h
#define _ppp_callout_h

struct	callout {
	int	c_time;		/* incremental time */
	caddr_t	c_arg;		/* argument to routine */
	void	(*c_func)();	/* routine (changed to void (*)() */
	struct	callout *c_next;
};

#endif /*!_ppp_callout_h*/
