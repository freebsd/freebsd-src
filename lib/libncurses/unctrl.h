
/*
 * unctrl.h
 *
 * Display a printable version of a control character.
 * Control characters are displayed in caret notation (^x), DELETE is displayed
 * as ^?. Printable characters sre displatyed as is.
 *
 * The returned pointer points to a static buffer which gets overwritten by
 * each call. Therefore, you must copy the resulting string to a safe place
 * before calling unctrl() again.
 *
 */
#ifndef _UNCTRL_H
#define _UNCTRL_H	1

extern char *unctrl(unsigned char);

#endif /* _UNCTRL_H */
