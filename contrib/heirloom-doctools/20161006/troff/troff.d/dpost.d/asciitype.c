/*
 * Copyright (c) 2003 Gunnar Ritter
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */
/*	Sccsid @(#)asciitype.c	1.4 (gritter) 4/17/03	*/

#include "asciitype.h"

const unsigned char class_char[] = {
/*	000 nul	001 soh	002 stx	003 etx	004 eot	005 enq	006 ack	007 bel	*/
	C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,
/*	010 bs 	011 ht 	012 nl 	013 vt 	014 np 	015 cr 	016 so 	017 si 	*/
	C_CNTRL,C_BLANK,C_WHITE,C_SPACE,C_SPACE,C_SPACE,C_CNTRL,C_CNTRL,
/*	020 dle	021 dc1	022 dc2	023 dc3	024 dc4	025 nak	026 syn	027 etb	*/
	C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,
/*	030 can	031 em 	032 sub	033 esc	034 fs 	035 gs 	036 rs 	037 us 	*/
	C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,C_CNTRL,
/*	040 sp 	041  ! 	042  " 	043  # 	044  $ 	045  % 	046  & 	047  ' 	*/
	C_BLANK,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,
/*	050  ( 	051  ) 	052  * 	053  + 	054  , 	055  - 	056  . 	057  / 	*/
	C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,
/*	060  0 	061  1 	062  2 	063  3 	064  4 	065  5 	066  6 	067  7 	*/
	C_OCTAL,C_OCTAL,C_OCTAL,C_OCTAL,C_OCTAL,C_OCTAL,C_OCTAL,C_OCTAL,
/*	070  8 	071  9 	072  : 	073  ; 	074  < 	075  = 	076  > 	077  ? 	*/
	C_DIGIT,C_DIGIT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,
/*	100  @ 	101  A 	102  B 	103  C 	104  D 	105  E 	106  F 	107  G 	*/
	C_PUNCT,C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,
/*	110  H 	111  I 	112  J 	113  K 	114  L 	115  M 	116  N 	117  O 	*/
	C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,
/*	120  P 	121  Q 	122  R 	123  S 	124  T 	125  U 	126  V 	127  W 	*/
	C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,C_UPPER,
/*	130  X 	131  Y 	132  Z 	133  [ 	134  \ 	135  ] 	136  ^ 	137  _ 	*/
	C_UPPER,C_UPPER,C_UPPER,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,
/*	140  ` 	141  a 	142  b 	143  c 	144  d 	145  e 	146  f 	147  g 	*/
	C_PUNCT,C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,
/*	150  h 	151  i 	152  j 	153  k 	154  l 	155  m 	156  n 	157  o 	*/
	C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,
/*	160  p 	161  q 	162  r 	163  s 	164  t 	165  u 	166  v 	167  w 	*/
	C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,C_LOWER,
/*	170  x 	171  y 	172  z 	173  { 	174  | 	175  } 	176  ~ 	177 del	*/
	C_LOWER,C_LOWER,C_LOWER,C_PUNCT,C_PUNCT,C_PUNCT,C_PUNCT,C_CNTRL
};
