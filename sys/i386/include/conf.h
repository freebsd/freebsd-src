#ifndef _MACHINE_CONF_H_
#define	_MACHINE_CONF_H_

#ifdef KERNEL

#ifndef ACTUALLY_LKM_NOT_KERNEL
/*
 * XXX instead of this, the per-driver declarations should probably be
 * put in the "driver.h" headers.  Then ioconf.h could include all the
 * "driver.h" headers and drivers would automatically include their
 * own "driver.h" header, so we wouldn't need to include ioconf.h here.
 * Interrupt handlers should probably be static.
 */
#include "ioconf.h"
#endif

/*
 * The following was copied from the bogusly non-machine-generated
 * file <i386/i386/conf.c>.  Eventually the routines should be static.
 */

/* bdevs. */

d_open_t	wdopen;
d_close_t	wdclose;
d_strategy_t	wdstrategy;
d_ioctl_t	wdioctl;
d_dump_t	wddump;
d_psize_t	wdsize;

d_open_t	wormopen;
d_close_t	wormclose;
d_strategy_t	wormstrategy;
d_ioctl_t	wormioctl;
d_dump_t	wormdump;
d_psize_t	wormsize;

d_open_t	sctargopen;
d_close_t	sctargclose;
d_strategy_t	sctargstrategy;
d_ioctl_t	sctargioctl;
d_dump_t	sctargdump;
d_psize_t	sctargsize;

d_open_t	ptopen;
d_close_t	ptclose;
d_strategy_t	ptstrategy;
d_ioctl_t	ptioctl;
d_dump_t	ptdump;
d_psize_t	ptsize;

d_open_t	sdopen;
d_close_t	sdclose;
d_strategy_t	sdstrategy;
d_ioctl_t	sdioctl;
d_dump_t	sddump;
d_psize_t	sdsize;

d_open_t	stopen;
d_close_t	stclose;
d_strategy_t	ststrategy;
d_ioctl_t	stioctl;

d_open_t	odopen;
d_close_t	odclose;
d_strategy_t	odstrategy;
d_ioctl_t	odioctl;
d_psize_t	odsize;

d_open_t	cdopen;
d_close_t	cdclose;
d_strategy_t	cdstrategy;
d_ioctl_t	cdioctl;
d_psize_t	cdsize;

d_open_t	mcdopen;
d_close_t	mcdclose;
d_strategy_t	mcdstrategy;
d_ioctl_t	mcdioctl;
d_psize_t	mcdsize;

d_open_t	scdopen;
d_close_t	scdclose;
d_strategy_t	scdstrategy;
d_ioctl_t	scdioctl;
d_psize_t	scdsize;

d_open_t	matcdopen;
d_close_t	matcdclose;
d_strategy_t	matcdstrategy;
d_ioctl_t	matcdioctl;
d_dump_t	matcddump;
d_psize_t	matcdsize;

d_open_t	ataopen;
d_close_t	ataclose;
d_strategy_t	atastrategy;
d_ioctl_t	ataioctl;
d_psize_t	atasize;

d_open_t	wcdbopen;
d_open_t	wcdropen;
d_close_t	wcdbclose;
d_close_t	wcdrclose;
d_strategy_t	wcdstrategy;
d_ioctl_t	wcdioctl;

d_open_t	chopen;
d_close_t	chclose;
d_strategy_t	chstrategy;	/* XXX not used */
d_ioctl_t	chioctl;

d_open_t	wtopen;
d_close_t	wtclose;
d_strategy_t	wtstrategy;
d_ioctl_t	wtioctl;
d_dump_t	wtdump;
d_psize_t	wtsize;

d_open_t	Fdopen;
d_close_t	fdclose;
d_strategy_t	fdstrategy;
d_ioctl_t	fdioctl;

d_open_t	vnopen;
d_close_t	vnclose;
d_strategy_t	vnstrategy;
d_ioctl_t	vnioctl;
d_dump_t	vndump;
d_psize_t	vnsize;

d_open_t        meteor_open; 
d_close_t       meteor_close;
d_read_t        meteor_read;
d_write_t       meteor_write;
d_ioctl_t       meteor_ioctl;
d_mmap_t        meteor_mmap;

d_rdwr_t swread, swwrite;

/* cdevs. */

d_open_t	mmopen;
d_close_t	mmclose;
d_rdwr_t	mmrw;
d_mmap_t	memmmap;
d_ioctl_t	mmioctl;

d_open_t	ptsopen;
d_close_t	ptsclose;
d_rdwr_t	ptsread;
d_rdwr_t	ptswrite;
d_stop_t	ptsstop;
d_open_t	ptcopen;
d_close_t	ptcclose;
d_rdwr_t	ptcread;
d_rdwr_t	ptcwrite;
d_select_t	ptcselect;
d_ttycv_t	ptydevtotty;
d_ioctl_t	ptyioctl;

d_open_t	snpopen;
d_close_t	snpclose;
d_rdwr_t	snpread;
d_rdwr_t	snpwrite;
d_select_t	snpselect;
d_ioctl_t	snpioctl;

d_open_t	logopen;
d_close_t	logclose;
d_rdwr_t	logread;
d_ioctl_t	logioctl;
d_select_t	logselect;

d_open_t	bquopen;
d_close_t	bquclose;
d_rdwr_t	bquread, bquwrite;
d_select_t	bquselect;
d_ioctl_t	bquioctl;

d_open_t	lptopen;
d_close_t	lptclose;
d_rdwr_t	lptwrite;
d_ioctl_t	lptioctl;

d_open_t	twopen;
d_close_t	twclose;
d_rdwr_t	twread, twwrite;
d_select_t	twselect;

d_open_t	psmopen;
d_close_t	psmclose;
d_rdwr_t	psmread;
d_select_t	psmselect;
d_ioctl_t	psmioctl;

d_open_t	sndopen;
d_close_t	sndclose;
d_ioctl_t	sndioctl;
d_rdwr_t	sndread, sndwrite;
d_select_t	sndselect;

d_open_t fdopen;

d_open_t	bpfopen;
d_close_t	bpfclose;
d_rdwr_t	bpfread, bpfwrite;
d_select_t	bpfselect;
d_ioctl_t	bpfioctl;

d_open_t	spkropen;
d_close_t	spkrclose;
d_rdwr_t	spkrwrite;
d_ioctl_t	spkrioctl;

d_open_t	pcaopen;
d_close_t	pcaclose;
d_rdwr_t	pcawrite;
d_ioctl_t	pcaioctl;
d_select_t	pcaselect;

d_open_t	mseopen;
d_close_t	mseclose;
d_rdwr_t	mseread;
d_select_t	mseselect;

d_open_t	sioopen;
d_close_t	sioclose;
d_rdwr_t	sioread, siowrite;
d_ioctl_t	sioioctl;
d_stop_t	siostop;
d_ttycv_t	siodevtotty;

d_open_t	suopen;
d_close_t	suclose;
d_ioctl_t	suioctl;
d_rdwr_t	suread, suwrite;
d_select_t	suselect;
d_strategy_t	sustrategy;

d_open_t	ukopen;
d_close_t	ukclose;
d_strategy_t	ukstrategy;	/* XXX not used */
d_ioctl_t	ukioctl;

d_open_t	lkmcopen;
d_close_t	lkmcclose;
d_ioctl_t	lkmcioctl;
d_open_t	lkmenodev;	/* XXX bogus; used for non-opens */

d_open_t	apmopen;
d_close_t	apmclose;
d_ioctl_t	apmioctl;

d_open_t	ctxopen;
d_close_t	ctxclose;
d_rdwr_t	ctxread;
d_rdwr_t	ctxwrite;
d_ioctl_t	ctxioctl;

d_open_t	sscopen;
d_close_t	sscclose;
d_ioctl_t	sscioctl;

d_open_t	cxopen;
d_close_t	cxclose;
d_rdwr_t	cxread, cxwrite;
d_ioctl_t	cxioctl;
d_select_t	cxselect;
d_stop_t	cxstop;
d_ttycv_t	cxdevtotty;

d_open_t	gpopen;
d_close_t	gpclose;
d_rdwr_t	gpwrite;
d_ioctl_t	gpioctl;

d_open_t	gscopen;
d_close_t	gscclose;
d_rdwr_t	gscread;
d_ioctl_t	gscioctl;

d_open_t	crdopen;
d_close_t	crdclose;
d_rdwr_t	crdread, crdwrite;
d_ioctl_t	crdioctl;
d_select_t	crdselect;

d_open_t	joyopen;
d_close_t	joyclose;
d_rdwr_t	joyread;
d_ioctl_t	joyioctl;

d_open_t      ascopen;
d_close_t     ascclose;
d_rdwr_t      ascread;
d_ioctl_t     ascioctl;
d_select_t    ascselect;

d_open_t	tunopen;
d_close_t	tunclose;
d_rdwr_t	tunread, tunwrite;
d_ioctl_t	tunioctl;
d_select_t	tunselect;

d_open_t        spigot_open;
d_close_t       spigot_close;
d_ioctl_t       spigot_ioctl;
d_rdwr_t        spigot_read, spigot_write;
d_select_t      spigot_select;
d_mmap_t        spigot_mmap;

d_open_t        cyopen;
d_close_t       cyclose;
d_read_t        cyread;
d_write_t       cywrite;
d_ioctl_t	cyioctl;
d_stop_t        cystop;
d_ttycv_t	cydevtotty;

d_open_t		dgbopen;     
d_close_t		dgbclose;   
d_rdwr_t		dgbread;
d_rdwr_t		dgbwrite; 
d_ioctl_t		dgbioctl;   
d_stop_t		dgbstop;     
d_ttycv_t		dgbdevtotty;

d_open_t        siopen;
d_close_t       siclose;
d_read_t        siread;
d_write_t       siwrite;
d_ioctl_t	siioctl;
d_stop_t        sistop;
d_ttycv_t	sidevtotty;

d_open_t	ityopen;
d_close_t	ityclose;
d_read_t	ityread;
d_write_t	itywrite;
d_ioctl_t	ityioctl;
d_stop_t	itystop;
d_ttycv_t	itydevtotty;

d_open_t	nicopen;
d_close_t	nicclose;
d_ioctl_t	nicioctl;

d_open_t  nnicopen;
d_close_t nnicclose;
d_ioctl_t nnicioctl;

d_open_t isdnopen;
d_close_t isdnclose;
d_read_t isdnread;
d_ioctl_t isdnioctl;

d_open_t itelopen;
d_close_t itelclose;
d_read_t itelread;
d_write_t itelwrite;
d_ioctl_t itelioctl;

d_open_t  ispyopen;
d_close_t ispyclose;
d_read_t ispyread;
d_write_t ispywrite;
d_ioctl_t ispyioctl;

d_open_t        rcopen;
d_close_t       rcclose;
d_rdwr_t        rcread, rcwrite;
d_ioctl_t       rcioctl;
d_stop_t        rcstop;
d_ttycv_t       rcdevtotty;

d_open_t     labpcopen;
d_close_t    labpcclose;
d_strategy_t labpcstrategy;
d_ioctl_t    labpcioctl;

#endif /* KERNEL */

#endif /* !_MACHINE_CONF_H_ */
