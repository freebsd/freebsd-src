#ifdef SEQUENCER_C
/*
 * Copyright by Hannu Savolainen 1993
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

  unsigned short finetune_table[128] =
	{
/*   0 */  9439,  9447,  9456,  9464,  9473,  9481,  9490,  9499, 
/*   8 */  9507,  9516,  9524,  9533,  9542,  9550,  9559,  9567, 
/*  16 */  9576,  9585,  9593,  9602,  9611,  9619,  9628,  9637, 
/*  24 */  9645,  9654,  9663,  9672,  9680,  9689,  9698,  9707, 
/*  32 */  9715,  9724,  9733,  9742,  9750,  9759,  9768,  9777, 
/*  40 */  9786,  9795,  9803,  9812,  9821,  9830,  9839,  9848, 
/*  48 */  9857,  9866,  9874,  9883,  9892,  9901,  9910,  9919, 
/*  56 */  9928,  9937,  9946,  9955,  9964,  9973,  9982,  9991, 
/*  64 */ 10000, 10009, 10018, 10027, 10036, 10045, 10054, 10063, 
/*  72 */ 10072, 10082, 10091, 10100, 10109, 10118, 10127, 10136, 
/*  80 */ 10145, 10155, 10164, 10173, 10182, 10191, 10201, 10210, 
/*  88 */ 10219, 10228, 10237, 10247, 10256, 10265, 10274, 10284, 
/*  96 */ 10293, 10302, 10312, 10321, 10330, 10340, 10349, 10358, 
/* 104 */ 10368, 10377, 10386, 10396, 10405, 10415, 10424, 10433, 
/* 112 */ 10443, 10452, 10462, 10471, 10481, 10490, 10499, 10509, 
/* 120 */ 10518, 10528, 10537, 10547, 10556, 10566, 10576, 10585
	};
#else
  extern unsigned short finetune_table[128];
#endif
