/* $Id: assert.h,v 1.1 1994/05/19 14:34:09 nate Exp $ */
#ifndef __ASSERT_H__
#define __ASSERT_H__
#ifdef DEBUG
#define ASSERT(p, s) ((void) (!(p) ? __m_botch(s, __FILE__, __LINE__) : 0))
extern int __m_botch proto((const char *, const char *, int));
#else
#define ASSERT(p, s)
#endif
#endif /* __ASSERT_H__ */ /* Do not add anything after this line */
