/*
 *  Copyright (c) 1993, 1994 Steve Gerakines
 *
 *  This is freely redistributable software.  You may do anything you
 *  wish with it, so long as the above notice stays intact.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 *  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  ftreg.h - QIC-40/80 floppy tape driver header
 *  06/03/94 v0.9
 *  Changed seek load point to QC_SEEKLP, added reqseg to SegReq structure.
 *
 *  10/30/93 v0.3
 *  More things will end up here.  QC_VENDORID and QC_VERSION now used.
 *
 *  08/07/93 v0.2 release
 *  Things that should've been here in the first place were moved.
 *  Tape geometry and segment request types were added.
 *
 *  06/03/93 v0.1 Alpha release
 *  Initial revision.  Many more things should be moved here.
 */

/* QIC-117 command set. */
#define QC_RESET			1	/* reset */
#define QC_NEXTBIT			2	/* report next bit */
#define QC_PAUSE			3	/* pause */
#define QC_STPAUSE			4	/* step pause */
#define QC_TIMEOUT			5	/* alt timeout */
#define QC_STATUS			6	/* report status */
#define QC_ERRCODE			7	/* report error code */
#define QC_CONFIG			8	/* report config */
#define QC_VERSION			9	/* report version */
#define QC_FORWARD			10	/* logical forward */
#define QC_SEEKSTART			11	/* seek to track start */
#define QC_SEEKEND			12	/* seek to track end */
#define QC_SEEKTRACK			13	/* seek head to track */
#define QC_SEEKLP			14	/* seek load point */
#define QC_FORMAT			15	/* format mode */
#define QC_WRITEREF			16	/* write reference */
#define QC_VERIFY			17	/* verify mode */
#define QC_STOP				18	/* stop tape */
#define QC_STEPUP			21	/* step head up */
#define QC_STEPDOWN			22	/* step head down */
#define QC_SEEKREV			25	/* seek reverse */
#define QC_SEEKFWD			26	/* seek forward */
#define QC_RATE				27	/* select data rate */
#define QC_DIAG1			28	/* diagnostic mode 1 */
#define QC_DIAG2			29	/* diagnostic mode 2 */
#define QC_PRIMARY			30	/* primary mode */
#define QC_VENDORID			32	/* vendor id */
#define QC_TSTATUS			33	/* report tape status */
#define QC_EXTREV			34	/* extended skip reverse */
#define QC_EXTFWD			35	/* extended skip forward */

/* Colorado enable/disable. */
#define QC_COL_ENABLE1			46	/* enable */
#define QC_COL_ENABLE2			2	/* unit+2 */
#define QC_COL_DISABLE			47	/* disable */

/* Mountain enable/disable. */
#define QC_MTN_ENABLE1			23	/* enable 1 */
#define QC_MTN_ENABLE2			20	/* enable 2 */
#define QC_MTN_DISABLE			24	/* disable */

/* Segment I/O request. */
typedef struct segq {
	unsigned char buff[QCV_SEGSIZE];/* Segment data; first for alignment */
	int reqtype;			/* Request type */
	long reqcrc;			/* CRC Errors found */
	long reqbad;			/* Bad sector map */
	long reqblk;			/* Block request starts at */
	long reqseg;			/* Segment request is at */
	int reqcan;			/* Cancel read-ahead */
	struct segq *next;		/* Next request */
} SegReq;

typedef int	ftu_t;
typedef int	ftsu_t;
typedef	struct ft_data *ft_p;
