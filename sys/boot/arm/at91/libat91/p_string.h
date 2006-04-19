/******************************************************************************
 *
 * Filename: p_string.h
 *
 * Definition of basic, private, string operations to prevent inclusion of full
 *
 * Revision information:
 *
 * 20AUG2004	kb_admin	initial creation
 *
 * BEGIN_KBDD_BLOCK
 * No warranty, expressed or implied, is included with this software.  It is
 * provided "AS IS" and no warranty of any kind including statutory or aspects
 * relating to merchantability or fitness for any purpose is provided.  All
 * intellectual property rights of others is maintained with the respective
 * owners.  This software is not copyrighted and is intended for reference
 * only.
 * END_BLOCK
 *
 * $FreeBSD$
 *****************************************************************************/

#ifndef _P_STRING_H_
#define _P_STRING_H_

#define ToASCII(x) ((x > 9) ? (x + 'A' - 0xa) : (x + '0'))

int p_IsWhiteSpace(char cValue);
unsigned p_HexCharValue(char cValue);
void p_memset(char *buffer, char value, int size);
int p_strlen(const char *buffer);
char *p_strcpy(char *to, const char *from);
unsigned p_ASCIIToHex(const char *buf);
unsigned p_ASCIIToDec(const char *buf);
void p_memcpy(char *to, const char *from, unsigned size);
int p_memcmp(const char *to, const char *from, unsigned size);
int p_strcmp(const char *to, const char *from);

#endif /* _P_STRING_H_ */
