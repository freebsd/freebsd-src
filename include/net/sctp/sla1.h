/* SCTP reference Implementation 
 * Copyright (C) 1999 Cisco, Inc. 
 * Copyright (C) 1999 Motorola, Inc.
 *
 * This file originates from Randy Stewart's SCTP reference Implementation.
 * 
 * The SCTP reference implementation is distributed in the hope that it 
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 * 
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 * 
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by: 
 *    Randy Stewart <rstewar1@email.mot.com>
 *    Ken Morneau   <kmorneau@cisco.com> 
 *    Qiaobing Xie  <qxie1@email.mot.com>
 */

#ifndef __SLA1_h__
#define __SLA1_h__

struct SLA_1_Context {
	unsigned int A;
	unsigned int B;
	unsigned int C;
	unsigned int D;
	unsigned int E;
	unsigned int H0;
	unsigned int H1;
	unsigned int H2;
	unsigned int H3;
	unsigned int H4;
	unsigned int words[80];
	unsigned int TEMP;

	/* block I am collecting to process */
	char SLAblock[64];

	/* collected so far */
	int howManyInBlock;
	unsigned int runningTotal;
};


#define F1(B,C,D) (((B & C) | ((~B) & D)))       /* 0  <= t <= 19 */
#define F2(B,C,D) (B ^ C ^ D)                   /* 20 <= t <= 39 */
#define F3(B,C,D) ((B & C) | (B & D) | (C & D)) /* 40 <= t <= 59 */
#define F4(B,C,D) (B ^ C ^ D)                   /*600 <= t <= 79 */
/* circular shift */

#define CSHIFT(A,B) ((B << A) | (B >> (32-A)))

#define K1 0x5a827999       /* 0  <= t <= 19 */
#define K2 0x6ed9eba1       /* 20 <= t <= 39 */
#define K3 0x8f1bbcdc       /* 40 <= t <= 59 */
#define K4 0xca62c1d6       /* 60 <= t <= 79 */

#define H0INIT 0x67452301
#define H1INIT 0xefcdab89
#define H2INIT 0x98badcfe
#define H3INIT 0x10325476
#define H4INIT 0xc3d2e1f0

extern void SLA1_Init(struct SLA_1_Context *);
extern void SLA1_Process(struct SLA_1_Context *, const unsigned char *, int);
extern void SLA1_Final(struct SLA_1_Context *, unsigned char *);

#endif
