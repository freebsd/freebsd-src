
/***********************************************************************\
* Per controller structure.						*
\***********************************************************************/
struct fdc_data
{
	int	fdcu;		/* our unit number */
	int	baseport;
	int	dmachan;
	int	flags;
#define FDC_ATTACHED	0x01
#define FDC_HASFTAPE	0x02
#define FDC_TAPE_BUSY	0x04
	struct	fd_data *fd;
	int fdu;		/* the active drive	*/
	struct buf head;	/* Head of buf chain      */
	struct buf rhead;	/* Raw head of buf chain  */
	int state;
	int retry;
	int status[7];		/* copy of the registers */
};

/***********************************************************************\
* Throughout this file the following conventions will be used:		*
* fd is a pointer to the fd_data struct for the drive in question	*
* fdc is a pointer to the fdc_data struct for the controller		*
* fdu is the floppy drive unit number					*
* fdcu is the floppy controller unit number				*
* fdsu is the floppy drive unit number on that controller. (sub-unit)	*
\***********************************************************************/
typedef int	fdu_t;
typedef int	fdcu_t;
typedef int	fdsu_t;
typedef	struct fd_data *fd_p;
typedef struct fdc_data *fdc_p;

#define FDUNIT(s)       (((s)>>6)&03)
#define FDTYPE(s)       ((s)&077)
