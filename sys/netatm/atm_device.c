/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * ATM device support functions
 *
 */

#include <netatm/kern_include.h>
#include <net/bpf.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Private structures for managing allocated kernel memory resources
 *
 * For each allocation of kernel memory, one Mem_ent will be used.  
 * The Mem_ent structures will be allocated in blocks inside of a 
 * Mem_blk structure.
 */
#define MEM_NMEMENT	10		/* How many Mem_ent's in a Mem_blk */

struct mem_ent {
	void		*me_kaddr;	/* Allocated memory address */
	u_int		me_ksize;	/* Allocated memory length */
	void		*me_uaddr;	/* Memory address returned to caller */
	u_int		me_flags;	/* Flags (see below) */
};
typedef struct mem_ent	Mem_ent;

/*
 * Memory entry flags
 */
#define	MEF_NONCACHE	1		/* Memory is noncacheable */


struct mem_blk {
	struct mem_blk	*mb_next;	/* Next block in chain */
	Mem_ent		mb_mement[MEM_NMEMENT]; /* Allocated memory entries */
};
typedef struct mem_blk	Mem_blk;

static Mem_blk		*atm_mem_head = NULL;

static struct t_atm_cause	atm_dev_cause = {
	T_ATM_ITU_CODING,
	T_ATM_LOC_USER,
	T_ATM_CAUSE_VPCI_VCI_ASSIGNMENT_FAILURE,
	{0, 0, 0, 0}
};


/*
 * ATM Device Stack Instantiation
 *
 * Called at splnet.
 *
 * Arguments
 *	ssp		pointer to array of stack definition pointers
 *			for connection
 *			ssp[0] points to upper layer's stack definition
 *			ssp[1] points to this layer's stack definition
 *			ssp[2] points to lower layer's stack definition
 *	cvcp		pointer to connection vcc for this stack
 *
 * Returns
 *	0		instantiation successful
 *	err		instantiation failed - reason indicated
 *
 */
int
atm_dev_inst(ssp, cvcp)
	struct stack_defn	**ssp;
	Atm_connvc		*cvcp;
{
	Cmn_unit	*cup = (Cmn_unit *)cvcp->cvc_attr.nif->nif_pif;
	Cmn_vcc		*cvp;
	int		err;

	/*
	 * Check to see if device has been initialized
	 */
	if ((cup->cu_flags & CUF_INITED) == 0)
		return ( EIO );

	/*
	 * Validate lower SAP
	 */
	/*
	 * Device driver is the lowest layer - no need to validate
	 */

	/*
	 * Validate PVC vpi.vci
	 */
	if (cvcp->cvc_attr.called.addr.address_format == T_ATM_PVC_ADDR) {
		/*
		 * Look through existing circuits - return error if found
		 */
		Atm_addr_pvc	*pp;

		pp = (Atm_addr_pvc *)cvcp->cvc_attr.called.addr.address;
		if (atm_dev_vcc_find(cup, ATM_PVC_GET_VPI(pp),
				ATM_PVC_GET_VCI(pp), 0))
			return ( EADDRINUSE );
	}

	/*
	 * Validate our SAP type
	 */
	switch ((*(ssp+1))->sd_sap) {
	case SAP_CPCS_AAL3_4:
	case SAP_CPCS_AAL5:
	case SAP_ATM:
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Allocate a VCC control block
	 */
	if ( ( cvp = (Cmn_vcc *)atm_allocate(cup->cu_vcc_pool) ) == NULL )
		return ( ENOMEM );
	
	cvp->cv_state = CVS_INST;
	cvp->cv_toku = (*ssp)->sd_toku;
	cvp->cv_upper = (*ssp)->sd_upper;
	cvp->cv_connvc = cvcp;

	/*
	 * Let device have a look at the connection request
	 */
	err = (*cup->cu_instvcc)(cup, cvp);
	if (err) {
		atm_free((caddr_t)cvp);
		return (err);
	}

	/*
	 * Looks good so far, so link in device VCC
	 */
	LINK2TAIL ( cvp, Cmn_vcc, cup->cu_vcc, cv_next );

	/*
	 * Save my token
	 */
	(*++ssp)->sd_toku = cvp;

	/*
	 * Pass instantiation down the stack
	 */
	/*
	 * No need - we're the lowest point.
	 */
	/* err = (*(ssp + 1))->sd_inst(ssp, cvcp); */

	/*
	 * Save the lower layer's interface info
	 */
	/*
	 * No need - we're the lowest point
	 */
	/* cvp->cv_lower = (*++ssp)->sd_lower; */
	/* cvp->cv_tok1 = (*ssp)->sd_toku; */

	return (0);
}


/*
 * ATM Device Stack Command Handler
 *
 * Arguments
 *	cmd		stack command code
 *	tok		session token (Cmn_vcc)
 *	arg1		command specific argument
 *	arg2		command specific argument
 *
 * Returns
 *	none
 *
 */
/*ARGSUSED*/
void
atm_dev_lower(cmd, tok, arg1, arg2)
	int	cmd;
	void	*tok;
	int	arg1;
	int	arg2;
{
	Cmn_vcc		*cvp = (Cmn_vcc *)tok;
	Atm_connvc	*cvcp = cvp->cv_connvc;
	Cmn_unit	*cup = (Cmn_unit *)cvcp->cvc_attr.nif->nif_pif;
	struct vccb	*vcp;
	u_int		state;
	int		s;

	switch ( cmd ) {

	case CPCS_INIT:
		/*
		 * Sanity check
		 */
		if ( cvp->cv_state != CVS_INST ) {
			log ( LOG_ERR,
				"atm_dev_lower: INIT: tok=%p, state=%d\n",
				tok, cvp->cv_state );
			break;
		}

		vcp = cvp->cv_connvc->cvc_vcc;

		/*
		 * Validate SVC vpi.vci
		 */
		if ( vcp->vc_type & VCC_SVC ) {

			if (atm_dev_vcc_find(cup, vcp->vc_vpi, vcp->vc_vci,
					vcp->vc_type & (VCC_IN | VCC_OUT))
						!= cvp){
				log ( LOG_ERR,
				  "atm_dev_lower: dup SVC (%d,%d) tok=%p\n",
					vcp->vc_vpi, vcp->vc_vci, tok );
				atm_cm_abort(cvp->cv_connvc, &atm_dev_cause);
				break;
			}
		}

		/*
		 * Tell the device to open the VCC
		 */
		cvp->cv_state = CVS_INITED;
		s = splimp();
		if ((*cup->cu_openvcc)(cup, cvp)) {
			atm_cm_abort(cvp->cv_connvc, &atm_dev_cause);
			(void) splx(s);
			break;
		}
		(void) splx(s);
		break;

	case CPCS_TERM: {
		KBuffer		*m, *prev, *next;
		int		*ip;

		s = splimp();

		/*
		 * Disconnect the VCC - ignore return code
		 */
		if ((cvp->cv_state == CVS_INITED) || 
		    (cvp->cv_state == CVS_ACTIVE)) {
			(void) (*cup->cu_closevcc)(cup, cvp);
		}
		cvp->cv_state = CVS_TERM;

		/*
		 * Remove from interface list
		 */
		UNLINK ( cvp, Cmn_vcc, cup->cu_vcc, cv_next );

		/*
		 * Free any buffers from this VCC on the ATM interrupt queue
		 */
		prev = NULL;
		for (m = atm_intrq.ifq_head; m; m = next) {
			next = KB_QNEXT(m);

			/*
			 * See if this entry is for the terminating VCC
			 */
			KB_DATASTART(m, ip, int *);
			ip++;
			if (*ip == (int)cvp) {
				/*
				 * Yep, so dequeue the entry
				 */
				if (prev == NULL)
					atm_intrq.ifq_head = next;
				else
					KB_QNEXT(prev) = next;

				if (next == NULL)
					atm_intrq.ifq_tail = prev;

				atm_intrq.ifq_len--;

				/*
				 * Free the unwanted buffers
				 */
				KB_FREEALL(m);
			} else {
				prev = m;
			}
		}
		(void) splx(s);

		/*
		 * Free VCC resources
		 */
		(void) atm_free((caddr_t)cvp);
		break;
		}

	case CPCS_UNITDATA_INV:

		/*
		 * Sanity check
		 *
		 * Use temp state variable since we dont want to lock out
		 * interrupts, but initial VC activation interrupt may
		 * happen here, changing state somewhere in the middle.
		 */
		state = cvp->cv_state;
		if ((state != CVS_ACTIVE) && 
		    (state != CVS_INITED)) {
			log ( LOG_ERR,
			    "atm_dev_lower: UNITDATA: tok=%p, state=%d\n",
				tok, state );
			KB_FREEALL((KBuffer *)arg1);
			break;
		}

		/*
		 * Send the packet to the interface's bpf if this vc has one.
		 */
		if (cvcp->cvc_vcc != NULL && cvcp->cvc_vcc->vc_nif != NULL) {
			struct ifnet *ifp =
			    (struct ifnet *)cvcp->cvc_vcc->vc_nif;

			if (ifp->if_bpf)
				bpf_mtap(ifp, (KBuffer *)arg1);
		}

		/*
		 * Hand the data off to the device
		 */
		(*cup->cu_output)(cup, cvp, (KBuffer *)arg1);

		break;

	case CPCS_UABORT_INV:
		log ( LOG_ERR,
		    "atm_dev_lower: unimplemented stack cmd 0x%x, tok=%p\n",
			cmd, tok );
		break;

	default:
		log ( LOG_ERR,
			"atm_dev_lower: unknown stack cmd 0x%x, tok=%p\n",
			cmd, tok );

	}

	return;
}



/*
 * Allocate kernel memory block
 * 
 * This function will allocate a kernel memory block of the type specified
 * in the flags parameter.  The returned address will point to a memory
 * block of the requested size and alignment.  The memory block will also 
 * be zeroed.  The alloc/free functions will manage/mask both the OS-specific 
 * kernel memory management requirements and the bookkeeping required to
 * deal with data alignment issues. 
 *
 * This function should not be called from interrupt level.
 *
 * Arguments:
 *	size	size of memory block to allocate
 *	align	data alignment requirement 
 *	flags	allocation flags (ATM_DEV_*)
 *
 * Returns:
 *	uaddr	pointer to aligned memory block
 *	NULL	unable to allocate memory
 *
 */
void *         
atm_dev_alloc(size, align, flags)
	u_int		size;
	u_int		align;
	u_int		flags;
{
	Mem_blk		*mbp;
	Mem_ent		*mep;
	u_int		kalign, ksize;
	int		s, i;

	s = splimp();

	/*
	 * Find a free Mem_ent
	 */
	mep = NULL;
	for (mbp = atm_mem_head; mbp && mep == NULL; mbp = mbp->mb_next) {
		for (i = 0; i < MEM_NMEMENT; i++) {
			if (mbp->mb_mement[i].me_uaddr == NULL) {
				mep = &mbp->mb_mement[i];
				break;
			}
		}
	}

	/*
	 * If there are no free Mem_ent's, then allocate a new Mem_blk
	 * and link it into the chain
	 */
	if (mep == NULL) {
		mbp = (Mem_blk *) KM_ALLOC(sizeof(Mem_blk), M_DEVBUF, M_NOWAIT);
		if (mbp == NULL) {
			log(LOG_ERR, "atm_dev_alloc: Mem_blk failure\n");
			(void) splx(s);
			return (NULL);
		}
		KM_ZERO(mbp, sizeof(Mem_blk));

		mbp->mb_next = atm_mem_head;
		atm_mem_head = mbp;
		mep = mbp->mb_mement;
	}

	/*
	 * Now we need to get the kernel's allocation alignment minimum
	 *
	 * This is obviously very OS-specific stuff
	 */
#ifdef sun
	if (flags & ATM_DEV_NONCACHE) {
		/* Byte-aligned */
		kalign = sizeof(long);
	} else {
		/* Doubleword-aligned */
		kalign = sizeof(double);
	}
#elif (defined(BSD) && (BSD >= 199103))
	kalign = MINALLOCSIZE;
#else
	#error Unsupported/unconfigured OS
#endif

	/*
	 * Figure out how much memory we must allocate to satify the
	 * user's size and alignment needs
	 */
	if (align <= kalign)
		ksize = size;
	else
		ksize = size + align - kalign;

	/*
	 * Finally, go get the memory
	 */
	if (flags & ATM_DEV_NONCACHE) {
#ifdef sun
		mep->me_kaddr = IOPBALLOC(ksize);
#elif defined(__i386__)
		mep->me_kaddr = KM_ALLOC(ksize, M_DEVBUF, M_NOWAIT);
#else
		#error Unsupported/unconfigured OS
#endif
	} else {
		mep->me_kaddr = KM_ALLOC(ksize, M_DEVBUF, M_NOWAIT);
	}

	if (mep->me_kaddr == NULL) {
		log(LOG_ERR, "atm_dev_alloc: %skernel memory unavailable\n",
			(flags & ATM_DEV_NONCACHE) ? "non-cacheable " : "");
		(void) splx(s);
		return (NULL);
	}

	/*
	 * Calculate correct alignment address to pass back to user
	 */
	mep->me_uaddr = (void *) roundup((u_int)mep->me_kaddr, align);
	mep->me_ksize = ksize;
	mep->me_flags = flags;

	/*
	 * Clear memory for user
	 */
	KM_ZERO(mep->me_uaddr, size);

	ATM_DEBUG4("atm_dev_alloc: size=%d, align=%d, flags=%d, uaddr=%p\n", 
		size, align, flags, mep->me_uaddr);

	(void) splx(s);

	return (mep->me_uaddr);
}


/*
 * Free kernel memory block
 * 
 * This function will free a kernel memory block previously allocated by
 * the atm_dev_alloc function.  
 *
 * This function should not be called from interrupt level.
 *
 * Arguments:
 *	uaddr	pointer to allocated aligned memory block
 *
 * Returns:
 *	none
 *
 */
void
atm_dev_free(uaddr)
	void		*uaddr;
{
	Mem_blk		*mbp;
	Mem_ent		*mep;
	int		s, i;

	ATM_DEBUG1("atm_dev_free: uaddr=%p\n", uaddr);

	s = splimp();

	/*
	 * Protect ourselves...
	 */
	if (uaddr == NULL)
		panic("atm_dev_free: trying to free null address");

	/*
	 * Find our associated entry
	 */
	mep = NULL;
	for (mbp = atm_mem_head; mbp && mep == NULL; mbp = mbp->mb_next) {
		for (i = 0; i < MEM_NMEMENT; i++) {
			if (mbp->mb_mement[i].me_uaddr == uaddr) {
				mep = &mbp->mb_mement[i];
				break;
			}
		}
	}

	/*
	 * If we didn't find our entry, then unceremoniously let the caller
	 * know they screwed up (it certainly couldn't be a bug here...)
	 */
	if (mep == NULL)
		panic("atm_dev_free: trying to free unknown address");
	
	/*
	 * Give the memory space back to the kernel
	 */
	if (mep->me_flags & ATM_DEV_NONCACHE) {
#ifdef sun
		IOPBFREE(mep->me_kaddr, mep->me_ksize);
#elif defined(__i386__)
		KM_FREE(mep->me_kaddr, mep->me_ksize, M_DEVBUF);
#else
		#error Unsupported/unconfigured OS
#endif
	} else {
		KM_FREE(mep->me_kaddr, mep->me_ksize, M_DEVBUF);
	}

	/*
	 * Free our entry
	 */
	mep->me_uaddr = NULL;

	(void) splx(s);

	return;
}


#ifdef	sun4m

typedef int (*func_t)();

/*
 * Map an address into DVMA space
 * 
 * This function will take a kernel virtual address and map it to
 * a DMA virtual address which can be used during SBus DMA cycles.
 *
 * Arguments:
 *	addr	kernel virtual address
 *	len	length of DVMA space requested
 *	flags	allocation flags (ATM_DEV_*)
 *
 * Returns:
 *	a	DVMA address
 *	NULL	unable to map into DMA space
 *
 */
void *
atm_dma_map(addr, len, flags)
	caddr_t	addr;
	int	len;
	int	flags;
{
	if (flags & ATM_DEV_NONCACHE)
		/*
		 * Non-cacheable memory is already DMA'able
		 */
		return ((void *)addr);
	else
		return ((void *)mb_nbmapalloc(bigsbusmap, addr, len,
			MDR_BIGSBUS|MB_CANTWAIT, (func_t)NULL, (caddr_t)NULL));
}


/*
 * Free a DVMA map address
 * 
 * This function will free DVMA map resources (addresses) previously
 * allocated with atm_dma_map().
 *
 * Arguments:
 *	addr	DMA virtual address
 *	flags	allocation flags (ATM_DEV_*)
 *
 * Returns:
 *	none
 *
 */
void
atm_dma_free(addr, flags)
	caddr_t addr;
	int	flags;
{
	if ((flags & ATM_DEV_NONCACHE) == 0)
		mb_mapfree(bigsbusmap, (int)&addr);

	return;
}
#endif	/* sun4m */


/*
 * Compress buffer chain
 * 
 * This function will compress a supplied buffer chain into a minimum number
 * of kernel buffers.  Typically, this function will be used because the
 * number of buffers in an output buffer chain is too large for a device's
 * DMA capabilities.  This should only be called as a last resort, since
 * all the data copying will surely kill any hopes of decent performance.
 *
 * Arguments:
 *	m	pointer to source buffer chain
 *
 * Returns:
 *	n	pointer to compressed buffer chain
 *
 */
KBuffer *         
atm_dev_compress(m)
	KBuffer		*m;
{
	KBuffer		*n, *n0, **np;
	int		len, space;
	caddr_t		src, dst;

	n = n0 = NULL;
	np = &n0;
	dst = NULL;
	space = 0;

	/*
	 * Copy each source buffer into compressed chain
	 */
	while (m) {

		if (space == 0) {

			/*
			 * Allocate another buffer for compressed chain
			 */
			KB_ALLOCEXT(n, ATM_DEV_CMPR_LG, KB_F_NOWAIT, KB_T_DATA);
			if (n) {
				space = ATM_DEV_CMPR_LG;
			} else {
				KB_ALLOC(n, ATM_DEV_CMPR_SM, KB_F_NOWAIT, 
					KB_T_DATA);
				if (n) {
					space = ATM_DEV_CMPR_SM;
				} else {
					/*
					 * Unable to get any new buffers, so
					 * just return the partially compressed
					 * chain and hope...
					 */
					*np = m;
					break;
				}
			}

			KB_HEADSET(n, 0);
			KB_LEN(n) = 0;
			KB_BFRSTART(n, dst, caddr_t);

			*np = n;
			np = &KB_NEXT(n);
		}

		/*
		 * Copy what we can from source buffer
		 */
		len = MIN(space, KB_LEN(m));
		KB_DATASTART(m, src, caddr_t);
		KM_COPY(src, dst, len);

		/*
		 * Adjust for copied data
		 */
		dst += len;
		space -= len;

		KB_HEADADJ(m, -len);
		KB_TAILADJ(n, len);

		/*
		 * If we've exhausted our current source buffer, free it
		 * and move to the next one
		 */
		if (KB_LEN(m) == 0) {
			KB_FREEONE(m, m);
		}
	}

	return (n0);
}


/*
 * Locate VCC entry
 * 
 * This function will return the VCC entry for a specified interface and
 * VPI/VCI value.
 *
 * Arguments:
 *	cup	pointer to interface unit structure
 *	vpi	VPI value
 *	vci	VCI value
 *	type	VCC type
 *
 * Returns:
 *	vcp	pointer to located VCC entry matching
 *	NULL	no VCC found
 *
 */
Cmn_vcc *
atm_dev_vcc_find(cup, vpi, vci, type)
	Cmn_unit	*cup;
	u_int		vpi;
	u_int		vci;
	u_int		type;
{
	Cmn_vcc		*cvp;
	int		s = splnet();

	/*
	 * Go find VCC
	 *
	 * (Probably should stick in a hash table some time)
	 */
	for (cvp = cup->cu_vcc; cvp; cvp = cvp->cv_next) {
		struct vccb	*vcp;

		vcp = cvp->cv_connvc->cvc_vcc;
		if ((vcp->vc_vci == vci) && (vcp->vc_vpi == vpi) && 
		    ((vcp->vc_type & type) == type))
			break;
	}

	(void) splx(s);
	return (cvp);
}


#ifdef notdef
/*
 * Module unloading notification
 * 
 * This function must be called just prior to unloading the module from 
 * memory.  All allocated memory will be freed here and anything else that
 * needs cleaning up.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
atm_unload()
{
	Mem_blk		*mbp;
	Mem_ent		*mep;
	int		s, i;

	s = splimp();

	/*
	 * Free up all of our memory management storage
	 */
	while (mbp = atm_mem_head) {

		/*
		 * Make sure users have freed up all of their memory
		 */
		for (i = 0; i < MEM_NMEMENT; i++) {
			if (mbp->mb_mement[i].me_uaddr != NULL) {
				panic("atm_unload: unfreed memory");
			}
		}

		atm_mem_head = mbp->mb_next;

		/*
		 * Hand this block back to the kernel
		 */
		KM_FREE((caddr_t) mbp, sizeof(Mem_blk), M_DEVBUF);
	}

	(void) splx(s);

	return;
}
#endif	/* notdef */


/*
 * Print a PDU
 * 
 * Arguments:
 *	cup	pointer to device unit
 *	cvp	pointer to VCC control block
 *	m	pointer to pdu buffer chain
 *	msg	pointer to message string
 *
 * Returns:
 *	none
 *
 */
void
atm_dev_pdu_print(cup, cvp, m, msg)
	Cmn_unit	*cup;
	Cmn_vcc		*cvp;
	KBuffer		*m;
	char		*msg;
{
	char		buf[128];

	snprintf(buf, sizeof(buf), "%s vcc=(%d,%d)", msg, 
		cvp->cv_connvc->cvc_vcc->vc_vpi, 
		cvp->cv_connvc->cvc_vcc->vc_vci);

	atm_pdu_print(m, buf);
}

