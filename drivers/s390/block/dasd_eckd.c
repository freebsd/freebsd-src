/* 
 * File...........: linux/drivers/s390/block/dasd_eckd.c
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 *                  Horst Hummel <Horst.Hummel@de.ibm.com> 
 *                  Carsten Otte <Cotte@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 1999,2000
 *
 * $Revision $
 *
 * History of changes (starts July 2000)
 * 07/11/00 Enabled rotational position sensing
 * 07/14/00 Reorganized the format process for better ERP
 * 07/20/00 added experimental support for 2105 control unit (ESS)
 * 07/24/00 increased expiration time and added the missing zero
 * 08/07/00 added some bits to define_extent for ESS support
 * 09/20/00 added reserve and release ioctls
 * 10/04/00 changed RW-CCWS to R/W Key and Data
 * 10/10/00 reverted last change according to ESS exploitation
 * 10/10/00 now dequeuing init_cqr before freeing *ouch*
 * 26/10/00 fixed ITPM20144ASC (problems when accesing a device formatted by VIF)
 * 01/23/01 fixed kmalloc statement in dump_sense to be GFP_ATOMIC
 *          fixed partition handling and HDIO_GETGEO
 */

#include <linux/config.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/hdreg.h>	/* HDIO_GETGEO                      */
#include <linux/blk.h>

#include <asm/debug.h>
#include <asm/ccwcache.h>
#include <asm/idals.h>
#include <asm/ebcdic.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/s390dyn.h>

#include "dasd_int.h"
#include "dasd_eckd.h"

#ifdef PRINTK_HEADER
#undef PRINTK_HEADER
#endif				/* PRINTK_HEADER */
#define PRINTK_HEADER DASD_NAME"(eckd):"

#define ECKD_C0(i) (i->home_bytes)
#define ECKD_F(i) (i->formula)
#define ECKD_F1(i) (ECKD_F(i)==0x01?(i->factors.f_0x01.f1):(i->factors.f_0x02.f1))
#define ECKD_F2(i) (ECKD_F(i)==0x01?(i->factors.f_0x01.f2):(i->factors.f_0x02.f2))
#define ECKD_F3(i) (ECKD_F(i)==0x01?(i->factors.f_0x01.f3):(i->factors.f_0x02.f3))
#define ECKD_F4(i) (ECKD_F(i)==0x02?(i->factors.f_0x02.f4):0)
#define ECKD_F5(i) (ECKD_F(i)==0x02?(i->factors.f_0x02.f5):0)
#define ECKD_F6(i) (i->factor6)
#define ECKD_F7(i) (i->factor7)
#define ECKD_F8(i) (i->factor8)

#ifdef MODULE
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,12))
MODULE_LICENSE("GPL");
#endif
#endif

dasd_discipline_t dasd_eckd_discipline;

typedef struct dasd_eckd_private_t {
	dasd_eckd_characteristics_t rdc_data;
	dasd_eckd_confdata_t        conf_data;
	eckd_count_t                count_area[5];
	int                         uses_cdl;
        attrib_data_t               attrib;     /* e.g. cache operations */
} dasd_eckd_private_t;

#ifdef CONFIG_DASD_DYNAMIC
static
devreg_t dasd_eckd_known_devices[] = {
        {
                ci: { hc: {ctype:0x3880, dtype: 0x3390}},
                flag:(DEVREG_MATCH_CU_TYPE | DEVREG_MATCH_DEV_TYPE | 
                      DEVREG_TYPE_DEVCHARS),
                oper_func:dasd_oper_handler
        },
        {
                ci: { hc: {ctype:0x3990}},
                flag:(DEVREG_MATCH_CU_TYPE | DEVREG_TYPE_DEVCHARS),
                oper_func:dasd_oper_handler
        },
	{
                ci: { hc: {ctype:0x2105}},
                flag:(DEVREG_MATCH_CU_TYPE | DEVREG_TYPE_DEVCHARS),
                oper_func:dasd_oper_handler
        },
	{
                ci: { hc: {ctype:0x9343}},
                flag:(DEVREG_MATCH_CU_TYPE | DEVREG_TYPE_DEVCHARS),
                oper_func:dasd_oper_handler
        }
};
#endif

int sizes_trk0[] = { 28, 148, 84 };
#define LABEL_SIZE 140

static inline unsigned int
round_up_multiple (unsigned int no, unsigned int mult)
{
	int rem = no % mult;
	return (rem ? no - rem + mult : no);
}

static inline unsigned int
ceil_quot (unsigned int d1, unsigned int d2)
{
	return (d1 + (d2 - 1)) / d2;
}

static inline int
bytes_per_record (dasd_eckd_characteristics_t * rdc, int kl,	/* key length */
		  int dl /* data length */ )
{
	int bpr = 0;
	switch (rdc->formula) {
	case 0x01:{
			unsigned int fl1, fl2;
			fl1 = round_up_multiple (ECKD_F2 (rdc) + dl,
						 ECKD_F1 (rdc));
			fl2 = round_up_multiple (kl ? ECKD_F2 (rdc) + kl : 0,
						 ECKD_F1 (rdc));
			bpr = fl1 + fl2;
			break;
		}
	case 0x02:{
			unsigned int fl1, fl2, int1, int2;
			int1 = ceil_quot (dl + ECKD_F6 (rdc),
					  ECKD_F5 (rdc) << 1);
			int2 = ceil_quot (kl + ECKD_F6 (rdc),
					  ECKD_F5 (rdc) << 1);
			fl1 = round_up_multiple (ECKD_F1 (rdc) *
						 ECKD_F2 (rdc) +
						 (dl + ECKD_F6 (rdc) +
						  ECKD_F4 (rdc) * int1),
						 ECKD_F1 (rdc));
			fl2 = round_up_multiple (ECKD_F1 (rdc) *
						 ECKD_F3 (rdc) +
						 (kl + ECKD_F6 (rdc) +
						  ECKD_F4 (rdc) * int2),
						 ECKD_F1 (rdc));
			bpr = fl1 + fl2;
			break;
		}
	default:

		MESSAGE (KERN_ERR,
                         "unknown formula%d", 
                         rdc->formula);
	}
	return bpr;
}

static inline unsigned int
bytes_per_track (dasd_eckd_characteristics_t * rdc)
{
	return *(unsigned int *) (rdc->byte_per_track) >> 8;
}

static inline unsigned int
recs_per_track (dasd_eckd_characteristics_t * rdc,
		unsigned int kl, unsigned int dl)
{
	int rpt = 0;
	int dn;
	switch (rdc->dev_type) {
	case 0x3380:
		if (kl)
			return 1499 / (15 +
				       7 + ceil_quot (kl + 12, 32) +
				       ceil_quot (dl + 12, 32));
		else
			return 1499 / (15 + ceil_quot (dl + 12, 32));
	case 0x3390:
		dn = ceil_quot (dl + 6, 232) + 1;
		if (kl) {
			int kn = ceil_quot (kl + 6, 232) + 1;
			return 1729 / (10 +
				       9 + ceil_quot (kl + 6 * kn, 34) +
				       9 + ceil_quot (dl + 6 * dn, 34));
		} else
			return 1729 / (10 + 9 + ceil_quot (dl + 6 * dn, 34));
	case 0x9345:
		dn = ceil_quot (dl + 6, 232) + 1;
		if (kl) {
			int kn = ceil_quot (kl + 6, 232) + 1;
			return 1420 / (18 +
				       7 + ceil_quot (kl + 6 * kn, 34) +
				       ceil_quot (dl + 6 * dn, 34));
		} else
			return 1420 / (18 + 7 + ceil_quot (dl + 6 * dn, 34));
	}
	return rpt;
}

static inline void
check_XRC (ccw1_t         *de_ccw,
           DE_eckd_data_t *data,
           dasd_device_t  *device)
{
        
	dasd_eckd_private_t *private = (dasd_eckd_private_t *) device->private;

        /* switch on System Time Stamp - needed for XRC Support */
        if (private->rdc_data.facilities.XRC_supported) {
                
                data->ga_extended |= 0x08;   /* switch on 'Time Stamp Valid'   */
                data->ga_extended |= 0x02;   /* switch on 'Extended Parameter' */
                
                data->ep_sys_time = get_clock ();
                
                de_ccw->count = sizeof (DE_eckd_data_t);
                de_ccw->flags |= CCW_FLAG_SLI;  
        }

        return;

} /* end check_XRC */

static inline int
define_extent (ccw1_t * de_ccw,
	       DE_eckd_data_t * data,
	       int trk, int totrk, 
               int cmd, dasd_device_t * device, ccw_req_t* cqr)
{
        int rc=0;
	ch_t geo, beg, end;
	dasd_eckd_private_t *private = (dasd_eckd_private_t *) device->private;

	geo.cyl = private->rdc_data.no_cyl;
	geo.head = private->rdc_data.trk_per_cyl;
	beg.cyl = trk / geo.head;
	beg.head = trk % geo.head;
	end.cyl = totrk / geo.head;
	end.head = totrk % geo.head;

	memset (de_ccw, 0, sizeof (ccw1_t));

	de_ccw->cmd_code = DASD_ECKD_CCW_DEFINE_EXTENT;
	de_ccw->count = 16;

	if ((rc=dasd_set_normalized_cda (de_ccw, __pa (data), cqr, device))) 
                return rc;

	memset (data, 0, sizeof (DE_eckd_data_t));
	switch (cmd) {
	case DASD_ECKD_CCW_READ_HOME_ADDRESS:
	case DASD_ECKD_CCW_READ_RECORD_ZERO:
	case DASD_ECKD_CCW_READ:
	case DASD_ECKD_CCW_READ_MT:
	case DASD_ECKD_CCW_READ_CKD:	/* Fallthrough */
	case DASD_ECKD_CCW_READ_CKD_MT:
	case DASD_ECKD_CCW_READ_KD:
	case DASD_ECKD_CCW_READ_KD_MT:
	case DASD_ECKD_CCW_READ_COUNT:
		data->mask.perm = 0x1;
		data->attributes.operation = private->attrib.operation;
		break;
	case DASD_ECKD_CCW_WRITE:
	case DASD_ECKD_CCW_WRITE_MT:
	case DASD_ECKD_CCW_WRITE_KD:
	case DASD_ECKD_CCW_WRITE_KD_MT:
		data->mask.perm = 0x02;
		data->attributes.operation = private->attrib.operation;

                check_XRC (de_ccw,
                           data,
                           device);
		break;
	case DASD_ECKD_CCW_WRITE_CKD:
	case DASD_ECKD_CCW_WRITE_CKD_MT:
		data->attributes.operation = DASD_BYPASS_CACHE;

                check_XRC (de_ccw,
                           data,
                           device);
		break;
	case DASD_ECKD_CCW_ERASE:
	case DASD_ECKD_CCW_WRITE_HOME_ADDRESS:
	case DASD_ECKD_CCW_WRITE_RECORD_ZERO:
		data->mask.perm = 0x3;
		data->mask.auth = 0x1;
		data->attributes.operation = DASD_BYPASS_CACHE;

                check_XRC (de_ccw,
                           data,
                           device);
		break;
	default:

		MESSAGE (KERN_ERR,
                         "unknown opcode 0x%x", 
                         cmd);

		break;
	}

	data->attributes.mode = 0x3; /* ECKD */

	if (private->rdc_data.cu_type == 0x2105
	    && !(private->uses_cdl && trk < 2) ) {
                
		data->ga_extended |= 0x40;
	}
        
        /* check for sequential prestage - enhance cylinder range */
        if (data->attributes.operation == DASD_SEQ_PRESTAGE ||
            data->attributes.operation == DASD_SEQ_ACCESS     ) {
                
                if (end.cyl + private->attrib.nr_cyl < geo.cyl) {

                        end.cyl +=  private->attrib.nr_cyl; 

                        DBF_DEV_EVENT (DBF_NOTICE, device,
                                       "Enhanced DE Cylinder from  %x to %x",
                                       (totrk / geo.head),
                                       end.cyl);


                } else {
                        end.cyl = (geo.cyl -1);

                        DBF_DEV_EVENT (DBF_NOTICE, device,
                                       "Enhanced DE Cylinder from  %x to "
                                       "End of device %x",
                                       (totrk / geo.head),
                                       end.cyl);

                }
        }

	data->beg_ext.cyl  = beg.cyl;
	data->beg_ext.head = beg.head;
	data->end_ext.cyl  = end.cyl;
	data->end_ext.head = end.head;

        return rc;
}

static inline int
locate_record (ccw1_t * lo_ccw,
	       LO_eckd_data_t * data,
	       int trk,
	       int rec_on_trk,
	       int no_rec, 
               int cmd, 
               dasd_device_t * device, 
               int reclen, 
               ccw_req_t* cqr)
{
        int rc=0;
	dasd_eckd_private_t *private = (dasd_eckd_private_t *) device->private;
	ch_t geo = { private->rdc_data.no_cyl,
		private->rdc_data.trk_per_cyl
	};
	ch_t seek = { trk / (geo.head), trk % (geo.head) };
	int sector = 0;

        DBF_EVENT (DBF_INFO, 
                   "Locate: trk %d, rec %d, no_rec %d, cmd %d, reclen %d",
                   trk,
                   rec_on_trk, 
                   no_rec, 
                   cmd, 
                   reclen);

	memset (lo_ccw, 0, sizeof (ccw1_t));
	lo_ccw->cmd_code = DASD_ECKD_CCW_LOCATE_RECORD;
	lo_ccw->count = 16;
	if ((rc=dasd_set_normalized_cda (lo_ccw, __pa (data), cqr, device)))
                return rc;

	memset (data, 0, sizeof (LO_eckd_data_t));
        if (rec_on_trk) {
                switch (private->rdc_data.dev_type) {
                case 0x3390:{
                        int dn, d;
                        dn = ceil_quot (reclen + 6, 232);
                        d = 9 + ceil_quot (reclen + 6 * (dn + 1), 34);
                        sector = (49 + (rec_on_trk - 1) * (10 + d)) / 8;
                        break;
                }
                case 0x3380:{
                        int d;
                        d = 7 + ceil_quot (reclen + 12, 32);
                        sector = (39 + (rec_on_trk - 1) * (8 + d)) / 7;
                        break;
                }
                case 0x9345:
                default:
                        sector = 0;
                }
        }
        data->sector = sector;
	data->count = no_rec;
	switch (cmd) {
	case DASD_ECKD_CCW_WRITE_HOME_ADDRESS:
		data->operation.orientation = 0x3;
		data->operation.operation = 0x03;
		break;
	case DASD_ECKD_CCW_READ_HOME_ADDRESS:
		data->operation.orientation = 0x3;
		data->operation.operation = 0x16;
		break;
	case DASD_ECKD_CCW_WRITE_RECORD_ZERO:
		data->operation.orientation = 0x1;
		data->operation.operation = 0x03;
		data->count++;
		break;
	case DASD_ECKD_CCW_READ_RECORD_ZERO:
		data->operation.orientation = 0x3;
		data->operation.operation = 0x16;
		data->count++;
		break;
	case DASD_ECKD_CCW_WRITE:
	case DASD_ECKD_CCW_WRITE_MT:
	case DASD_ECKD_CCW_WRITE_KD:
	case DASD_ECKD_CCW_WRITE_KD_MT:
		data->auxiliary.last_bytes_used = 0x1;
		data->length = reclen;
		data->operation.operation = 0x01;
		break;
	case DASD_ECKD_CCW_WRITE_CKD:
	case DASD_ECKD_CCW_WRITE_CKD_MT:
		data->auxiliary.last_bytes_used = 0x1;
		data->length = reclen;
		data->operation.operation = 0x03;
		break;
	case DASD_ECKD_CCW_READ:
	case DASD_ECKD_CCW_READ_MT:
	case DASD_ECKD_CCW_READ_KD:
	case DASD_ECKD_CCW_READ_KD_MT:
		data->auxiliary.last_bytes_used = 0x1;
		data->length = reclen;
		data->operation.operation = 0x06;
		break;
	case DASD_ECKD_CCW_READ_CKD:
	case DASD_ECKD_CCW_READ_CKD_MT:
		data->auxiliary.last_bytes_used = 0x1;
		data->length = reclen;
		data->operation.operation = 0x16;
		break;
	case DASD_ECKD_CCW_READ_COUNT:
		data->operation.operation = 0x06;
		break;
	case DASD_ECKD_CCW_ERASE:
                data->length = reclen;
                data->auxiliary.last_bytes_used = 0x1;
		data->operation.operation = 0x0b;
		break;
	default:

                MESSAGE (KERN_ERR,
                         "unknown opcode 0x%x",
                         cmd);
	}
	memcpy (&(data->seek_addr), &seek, sizeof (ch_t));
	memcpy (&(data->search_arg), &seek, sizeof (ch_t));
	data->search_arg.record = rec_on_trk;
        return rc;
}

static int
dasd_eckd_id_check (s390_dev_info_t * info)
{
	if (info->sid_data.cu_type == 0x3990 ||
	    info->sid_data.cu_type == 0x2105)
		    if (info->sid_data.dev_type == 0x3390) return 0;
	if (info->sid_data.cu_type == 0x3990 ||
	    info->sid_data.cu_type == 0x2105)
		    if (info->sid_data.dev_type == 0x3380) return 0;
	if (info->sid_data.cu_type == 0x9343)
		if (info->sid_data.dev_type == 0x9345)
			return 0;
	return -ENODEV;
}

static int
dasd_eckd_check_characteristics (struct dasd_device_t *device)
{
	int   rc = 0;
	void *conf_data;
        void *rdc_data;
	int   conf_len;
	dasd_eckd_private_t *private;

	if (device == NULL) {

		MESSAGE (KERN_WARNING, "%s",
                         "Null device pointer passed to characteristics "
                         "checker");

                return -ENODEV;
	}
	device->private = kmalloc (sizeof (dasd_eckd_private_t), 
                                   GFP_KERNEL);

	if (device->private == NULL) {

		MESSAGE (KERN_WARNING, "%s",
			"memory allocation failed for private data");

		rc = -ENOMEM;
                goto fail;
	}

	private  = (dasd_eckd_private_t *) device->private;
	rdc_data = (void *) &(private->rdc_data);

        /* Read Device Characteristics */
	rc = read_dev_chars (device->devinfo.irq, 
                             &rdc_data, 
                             64);
	if (rc) {

		MESSAGE (KERN_WARNING,
			"Read device characteristics returned error %d",
                         rc);

                goto fail;
        }

        DEV_MESSAGE (KERN_INFO, device,
                     "%04X/%02X(CU:%04X/%02X) Cyl:%d Head:%d Sec:%d",
                     private->rdc_data.dev_type, 
                     private->rdc_data.dev_model,
                     private->rdc_data.cu_type, 
                     private->rdc_data.cu_model.model,
                     private->rdc_data.no_cyl, 
                     private->rdc_data.trk_per_cyl,
                     private->rdc_data.sec_per_trk);

        /* set default cache operations */
        private->attrib.operation = DASD_NORMAL_CACHE;
        private->attrib.nr_cyl    = 0;
        
        /* Read Configuration Data */
        rc = read_conf_data (device->devinfo.irq, 
                             &conf_data, 
                             &conf_len,
                             LPM_ANYPATH);

        if (rc == -EOPNOTSUPP) {
                rc = 0; /* this one is ok */
        }
	if (rc) {

		MESSAGE (KERN_WARNING,
                         "Read configuration data returned error %d",
                         rc);

                goto fail;
	}
        if (conf_data == NULL) {

		MESSAGE (KERN_WARNING, "%s",
                         "No configuration data retrieved");

                goto out; /* no errror */
	} 
        if (conf_len != sizeof (dasd_eckd_confdata_t)) {

		MESSAGE (KERN_WARNING,
                         "sizes of configuration data mismatch"
                         "%d (read) vs %ld (expected)",
                         conf_len, 
                         sizeof (dasd_eckd_confdata_t));

                goto out; /* no errror */
	} 
        memcpy (&private->conf_data, conf_data, 
                sizeof (dasd_eckd_confdata_t));

        DEV_MESSAGE (KERN_INFO, device,
                "%04X/%02X(CU:%04X/%02X): Configuration data read",
                private->rdc_data.dev_type, 
                private->rdc_data.dev_model,
                private->rdc_data.cu_type, 
                private->rdc_data.cu_model.model);
        goto out;

 fail:
        if (device->private) {
                kfree (device->private);
                device->private = NULL;
        }
        
 out:
	return rc;
}

static inline int
dasd_eckd_cdl_reclen (dasd_device_t * device, int recid)
{
	dasd_eckd_private_t *private = (dasd_eckd_private_t *) device->private;
	int byt_per_blk = device->sizes.bp_block;
	int blk_per_trk = recs_per_track (&(private->rdc_data), 0, byt_per_blk);
	if (recid < 3)
		return sizes_trk0[recid];
	if (recid < blk_per_trk)
		return byt_per_blk;
	if (recid < 2 * blk_per_trk)
		return LABEL_SIZE;
	return byt_per_blk;
}

static ccw_req_t *
dasd_eckd_init_analysis (struct dasd_device_t *device)
{
	ccw_req_t *cqr = NULL;
	ccw1_t *ccw;
	DE_eckd_data_t *DE_data;
	LO_eckd_data_t *LO_data;
	dasd_eckd_private_t *private = (dasd_eckd_private_t *)device->private;
	eckd_count_t *count_data = private->count_area;

	cqr = dasd_alloc_request (dasd_eckd_discipline.name, 8 + 1,
				 sizeof (DE_eckd_data_t) +
				 2 * sizeof (LO_eckd_data_t),
                                 device);
	if (cqr == NULL) {

                MESSAGE (KERN_WARNING, "%s",
                        "No memory to allocate initialization request");
                goto out;
	}
	DE_data = cqr->data;
	LO_data = cqr->data + sizeof (DE_eckd_data_t);
	ccw = cqr->cpaddr;
	if (define_extent (ccw, DE_data, 0, 2, DASD_ECKD_CCW_READ_COUNT, device, cqr)) {
                goto clear_cqr;
        }        
	ccw->flags |= CCW_FLAG_CC;
	ccw++;
	if (locate_record (ccw, LO_data++, 0, 0, 4, DASD_ECKD_CCW_READ_COUNT,
                           device, 0, cqr)) {
                goto clear_cqr;
        }       
	ccw->flags |= CCW_FLAG_CC;
	ccw++;
	ccw->cmd_code = DASD_ECKD_CCW_READ_COUNT;
	ccw->count = 8;
	if (dasd_set_normalized_cda (ccw, __pa (count_data++), cqr, device)) {
                goto clear_cqr;
        }
	ccw->flags |= CCW_FLAG_CC;
	ccw++;
	ccw->cmd_code = DASD_ECKD_CCW_READ_COUNT;
	ccw->count = 8;
	if (dasd_set_normalized_cda (ccw, __pa (count_data++), cqr, device)) {
                goto clear_cqr;
        }
	ccw->flags |= CCW_FLAG_CC;
	ccw++;
	ccw->cmd_code = DASD_ECKD_CCW_READ_COUNT;
	ccw->count = 8;
	if (dasd_set_normalized_cda (ccw, __pa (count_data++), cqr, device)) {
                goto clear_cqr;
        }
	ccw->flags |= CCW_FLAG_CC;
	ccw++;
	ccw->cmd_code = DASD_ECKD_CCW_READ_COUNT;
	ccw->count = 8;
	if (dasd_set_normalized_cda (ccw, __pa (count_data++), cqr, device)) {
                goto clear_cqr;
        }
	ccw->flags |= CCW_FLAG_CC;
	ccw++;
	if (locate_record (ccw, LO_data++, 2, 0, 1, DASD_ECKD_CCW_READ_COUNT,
                           device, 0, cqr)) {
                goto clear_cqr;
        }       
	ccw->flags |= CCW_FLAG_CC;
	ccw++;
	ccw->cmd_code = DASD_ECKD_CCW_READ_COUNT;
	ccw->count = 8;
	if (dasd_set_normalized_cda (ccw, __pa (count_data), cqr, device)) {
                goto clear_cqr;
        }
	cqr->device = device;
	cqr->retries = 0;
        cqr->buildclk = get_clock ();
	cqr->status = CQR_STATUS_FILLED;
        dasd_chanq_enq (&device->queue, cqr);
        goto out;

 clear_cqr:
        dasd_free_request (cqr,device);

        MESSAGE (KERN_WARNING, "%s",
                "No memory to allocate initialization request");

        cqr=NULL;
 out:
	return cqr;
}

static int
dasd_eckd_do_analysis (struct dasd_device_t *device)
{
	int sb, rpt;
	dasd_eckd_private_t *private = (dasd_eckd_private_t *) device->private;
	eckd_count_t *count_area = NULL;
	char *cdl_msg;
        int status;
	int i;

	private->uses_cdl = 1;
        status = device->init_cqr->status;
	dasd_chanq_deq (&device->queue, device->init_cqr);
	dasd_free_request (device->init_cqr, device);
	/* Free the cqr and cleanup device->sizes */
        if ( status != CQR_STATUS_DONE ) {

                DEV_MESSAGE (KERN_WARNING, device, "%s",
                             "volume analysis returned unformatted disk");

                return -EMEDIUMTYPE;
        }
	/* Check Track 0 for Compatible Disk Layout */
	for (i = 0; i < 3; i++) {
		if ((i < 3) &&
		    ((private->count_area[i].kl != 4) ||
		     (private->count_area[i].dl !=
		      dasd_eckd_cdl_reclen (device, i) - 4))) {
			private->uses_cdl = 0;
			break;
		}
	}
	if (i == 3) {
		count_area = &private->count_area[4];
	}
	if (private->uses_cdl == 0) {
		for (i = 0; i < 5; i++) {
			if ((private->count_area[i].kl != 0) ||
			    (private->count_area[i].dl !=
			     private->count_area[0].dl)) {
				break;
			}
		}
		if (i == 5) {
			count_area = &private->count_area[0];
		}
	} else {
		if (private->count_area[3].record == 1) {

			DEV_MESSAGE (KERN_WARNING, device, "%s",
                                     "Trk 0: no records after VTOC!");
		}
	}
	if (count_area != NULL &&	/* we found notthing violating our disk layout */
	    count_area->kl == 0) {
                /* find out blocksize */
                switch (count_area->dl) {
                case 512:
		case 1024:
		case 2048:
		case 4096:
			device->sizes.bp_block = count_area->dl;
			break;
		}
	}
	if (device->sizes.bp_block == 0) {

		DEV_MESSAGE (KERN_WARNING, device, "%s",
                             "Volume has incompatible disk layout");

		return -EMEDIUMTYPE;
	}
	device->sizes.s2b_shift = 0;	/* bits to shift 512 to get a block */
	device->sizes.pt_block = 2;
	for (sb = 512; sb < device->sizes.bp_block; sb = sb << 1)
		device->sizes.s2b_shift++;

	rpt = recs_per_track (&private->rdc_data, 0, device->sizes.bp_block);
	device->sizes.blocks = (private->rdc_data.no_cyl *
				private->rdc_data.trk_per_cyl *
				recs_per_track (&private->rdc_data, 0,
						device->sizes.bp_block));
	cdl_msg =
	    private->
	    uses_cdl ? "compatible disk layout" : "linux disk layout";

	DEV_MESSAGE (KERN_INFO, device, 
                     "(%dkB blks): %dkB at %dkB/trk %s",
                     (device->sizes.bp_block >> 10),
                     ((private->rdc_data.no_cyl *
                       private->rdc_data.trk_per_cyl *
                       recs_per_track (&private->rdc_data, 0,
                                       device->sizes.bp_block) *
                       (device->sizes.bp_block >> 9)) >> 1),
                     ((recs_per_track (&private->rdc_data, 0,
                                       device->sizes.bp_block) *
                       device->sizes.bp_block) >> 10),
                     cdl_msg);

	return 0;
}

static int
dasd_eckd_fill_geometry (struct dasd_device_t *device, struct hd_geometry *geo)
{
	int rc = 0;
	dasd_eckd_private_t *private = (dasd_eckd_private_t *) device->private;
	switch (device->sizes.bp_block) {
	case 512:
	case 1024:
	case 2048:
	case 4096:
            geo->sectors = recs_per_track (&(private->rdc_data), 
                                           0, device->sizes.bp_block);
            break;
	default:
            break;
	}
	geo->cylinders = private->rdc_data.no_cyl;
	geo->heads = private->rdc_data.trk_per_cyl;
	return rc;
}

static ccw_req_t *
dasd_eckd_format_device (dasd_device_t * device, format_data_t * fdata)
{
	int i;
	ccw_req_t *fcp = NULL;
	DE_eckd_data_t *DE_data = NULL;
	LO_eckd_data_t *LO_data = NULL;
	eckd_count_t *ct_data = NULL;
	eckd_count_t *r0_data = NULL;
	eckd_home_t *ha_data = NULL;
	ccw1_t *last_ccw = NULL;
	void *last_data = NULL;
	dasd_eckd_private_t *private = (dasd_eckd_private_t *) device->private;

	int rpt = recs_per_track (&(private->rdc_data), 0, fdata->blksize);
	int cyl = fdata->start_unit / private->rdc_data.trk_per_cyl;
	int head = fdata->start_unit % private->rdc_data.trk_per_cyl;
	int wrccws = rpt;
	int datasize = sizeof (DE_eckd_data_t) + sizeof (LO_eckd_data_t);
        
	if (fdata->start_unit >= 
            (private->rdc_data.no_cyl * private->rdc_data.trk_per_cyl)){

                DEV_MESSAGE (KERN_INFO, device, 
                             "Track no %d too big!", 
                             fdata->start_unit);

                return NULL;
        }
        if ( fdata->start_unit > fdata->stop_unit) {

                DEV_MESSAGE (KERN_INFO, device, 
                             "Track %d reached! ending.",
                             fdata->start_unit);

		return NULL;
	}
	switch (fdata->blksize) {
	case 512:
	case 1024:
	case 2048:
	case 4096:
		break;
	default:

		MESSAGE (KERN_WARNING,
                         "Invalid blocksize %d...terminating!", 
                         fdata->blksize);

		return NULL;
	}
	switch (fdata->intensity) {
	case 0x00:
	case 0x01:
	case 0x03:
	case 0x04:		/* make track invalid */
	case 0x08:
	case 0x09:
	case 0x0b:
	case 0x0c:
		break;
	default:

		MESSAGE (KERN_WARNING,
                         "Invalid flags 0x%x...terminating!", 
                         fdata->intensity);

		return NULL;
	}

	/* print status line */
	if ((private->rdc_data.no_cyl < 20) ?
	    (fdata->start_unit % private->rdc_data.no_cyl == 0) :
	    (fdata->start_unit % private->rdc_data.no_cyl == 0 &&
	     (fdata->start_unit / private->rdc_data.no_cyl) %
	     (private->rdc_data.no_cyl / 20))) {

		DBF_DEV_EVENT (DBF_NOTICE, device,
                               "Format Cylinder: %d Flags: %d",
                               fdata->start_unit / private->rdc_data.trk_per_cyl,
                               fdata->intensity);

	}
	if ((fdata->intensity & ~0x8) & 0x04) {
		wrccws = 1;
		rpt = 1;
	} else {
		if (fdata->intensity & 0x1) {
			wrccws++;
			datasize += sizeof (eckd_count_t);
		}
		if (fdata->intensity & 0x2) {
			wrccws++;
			datasize += sizeof (eckd_home_t);
		}
	}
	fcp = dasd_alloc_request (dasd_eckd_discipline.name,
				 wrccws + 2 + 1,
				 datasize + rpt * sizeof (eckd_count_t),
                                 device );
	if (fcp != NULL) {
		fcp->device = device;
		fcp->retries = 2;	/* set retry counter to enable ERP */
		last_data = fcp->data;
		DE_data = (DE_eckd_data_t *) last_data;
		last_data = (void *) (DE_data + 1);
		LO_data = (LO_eckd_data_t *) last_data;
		last_data = (void *) (LO_data + 1);
		if (fdata->intensity & 0x2) {
			ha_data = (eckd_home_t *) last_data;
			last_data = (void *) (ha_data + 1);
		}
		if (fdata->intensity & 0x1) {
			r0_data = (eckd_count_t *) last_data;
			last_data = (void *) (r0_data + 1);
		}
		ct_data = (eckd_count_t *) last_data;

		last_ccw = fcp->cpaddr;

		switch (fdata->intensity & ~0x08) {
		case 0x03:
			if (define_extent (last_ccw, DE_data, fdata->start_unit, fdata->start_unit,
                                           DASD_ECKD_CCW_WRITE_HOME_ADDRESS,
                                           device, fcp)) {
                                goto clear_fcp;
                        }
			last_ccw->flags |= CCW_FLAG_CC;
			last_ccw++;
			if (locate_record (last_ccw, LO_data, fdata->start_unit, 0, wrccws,
                                           DASD_ECKD_CCW_WRITE_HOME_ADDRESS, device,
                                           device->sizes.bp_block, fcp)) {
                                goto clear_fcp;
                        }
			last_ccw->flags |= CCW_FLAG_CC;
			last_ccw++;
			break;
		case 0x01:
			if (define_extent (last_ccw, DE_data, fdata->start_unit, fdata->start_unit,
                                           DASD_ECKD_CCW_WRITE_RECORD_ZERO, device, fcp)) {
                                goto clear_fcp;
                        }
			last_ccw->flags |= CCW_FLAG_CC;
			last_ccw++;
			if (locate_record (last_ccw, LO_data, fdata->start_unit, 0, wrccws,
                                           DASD_ECKD_CCW_WRITE_RECORD_ZERO, device,
                                           device->sizes.bp_block, fcp)) {
                                goto clear_fcp;
                        }
			last_ccw->flags |= CCW_FLAG_CC;
			last_ccw++;
			memset (r0_data, 0, sizeof (eckd_count_t));
			break;
		case 0x04:
                        fdata->blksize = 8;
		case 0x00:
			if (define_extent (last_ccw, DE_data, fdata->start_unit, fdata->start_unit,
                                           DASD_ECKD_CCW_WRITE_CKD, device, fcp)) {
                                dasd_free_request (fcp, device);
                                return NULL;
                        }
			last_ccw->flags |= CCW_FLAG_CC;
			last_ccw++;
			if (locate_record (last_ccw, LO_data, fdata->start_unit, 0, wrccws,
                                           DASD_ECKD_CCW_WRITE_CKD, device, fdata->blksize, fcp)) {
                                goto clear_fcp;
                        }
			last_ccw->flags |= CCW_FLAG_CC;
			last_ccw++;
			break;
		default:

			MESSAGE (KERN_WARNING,
                                 "Unknown format flags...%d", 
                                 fdata->intensity);

			return NULL;
		}
		if (fdata->intensity & 0x02) {

			MESSAGE (KERN_WARNING,
                                 "Unsupported format flag...%d", 
                                 fdata->intensity);

			return NULL;
		}
		if (fdata->intensity & 0x01) {	/* write record zero */
			r0_data->cyl = cyl;
			r0_data->head = head;
			r0_data->record = 0;
			r0_data->kl = 0;
			r0_data->dl = 8;
			last_ccw->cmd_code = DASD_ECKD_CCW_WRITE_RECORD_ZERO;
			last_ccw->count = 8;
			last_ccw->flags |= CCW_FLAG_CC | CCW_FLAG_SLI;
			if (dasd_set_normalized_cda (last_ccw, __pa (r0_data), fcp, device)) {
                                goto clear_fcp;
                        }
			last_ccw++;
		}
		if ((fdata->intensity & ~0x08) & 0x04) {	/* erase track */
			memset (ct_data, 0, sizeof (eckd_count_t));
			ct_data->cyl = cyl;
			ct_data->head = head;
			ct_data->record = 1;
			ct_data->kl = 0;
			ct_data->dl = 0;
			last_ccw->cmd_code = DASD_ECKD_CCW_WRITE_CKD;
			last_ccw->count = 8;
			last_ccw->flags |= CCW_FLAG_CC | CCW_FLAG_SLI;
			if (dasd_set_normalized_cda (last_ccw, __pa (ct_data), fcp, device)) {
                                goto clear_fcp;
                        }
			last_ccw++;
		} else {	/* write remaining records */
			for (i = 0; i < rpt; i++) {
				memset (ct_data + i, 0, sizeof (eckd_count_t));
				(ct_data + i)->cyl = cyl;
				(ct_data + i)->head = head;
				(ct_data + i)->record = i + 1;
				(ct_data + i)->kl = 0;
				if (fdata->intensity & 0x08) {
					// special handling when formatting CDL
					switch (fdata->start_unit) {
					case 0:
						if (i < 3) {
							(ct_data + i)->kl = 4;
							
							    (ct_data + i)->dl =
							    sizes_trk0[i] - 4;
						} else
							(ct_data + i)->dl = fdata->blksize;
						break;
					case 1:
						(ct_data + i)->kl = 44;
						(ct_data + i)->dl = LABEL_SIZE - 44;
						break;
					default:
						(ct_data + i)->dl = fdata->blksize;
						break;
					}
				} else
					(ct_data + i)->dl = fdata->blksize;
				last_ccw->cmd_code = DASD_ECKD_CCW_WRITE_CKD;
				last_ccw->flags |= CCW_FLAG_CC | CCW_FLAG_SLI;
				last_ccw->count = 8;
				if (dasd_set_normalized_cda (last_ccw,
                                                             __pa (ct_data + i), fcp, device)) {
                                goto clear_fcp;
                                }
				last_ccw++;
			}
		}
		(last_ccw - 1)->flags &= ~(CCW_FLAG_CC | CCW_FLAG_DC);
		fcp->device = device;
                fcp->buildclk = get_clock ();
		fcp->status = CQR_STATUS_FILLED;
	}
        goto out;
 clear_fcp:
        dasd_free_request (fcp, device);
        fcp=NULL;
 out:
	return fcp;
}

static dasd_era_t
dasd_eckd_examine_error (ccw_req_t * cqr, devstat_t * stat)
{
	dasd_device_t *device = (dasd_device_t *) cqr->device;

	if (stat->cstat == 0x00 &&
	    stat->dstat == (DEV_STAT_CHN_END | DEV_STAT_DEV_END))
		    return dasd_era_none;

	switch (device->devinfo.sid_data.cu_type) {
	case 0x3990:
	case 0x2105:
		return dasd_3990_erp_examine (cqr, stat);
	case 0x9343:
		return dasd_9343_erp_examine (cqr, stat);
	default:

		MESSAGE (KERN_WARNING, "%s",
                         "default (unknown CU type) - RECOVERABLE return");

		return dasd_era_recover;
	}
}

static dasd_erp_action_fn_t
dasd_eckd_erp_action (ccw_req_t * cqr)
{
	dasd_device_t *device = (dasd_device_t *) cqr->device;

	switch (device->devinfo.sid_data.cu_type) {
	case 0x3990:
	case 0x2105:
		return dasd_3990_erp_action;
	case 0x9343:
		/* Return dasd_9343_erp_action; */
	default:
		return dasd_default_erp_action;
	}
}

static dasd_erp_postaction_fn_t
dasd_eckd_erp_postaction (ccw_req_t * cqr)
{
	return dasd_default_erp_postaction;
}


inline unsigned char
dasd_eckd_cdl_cmd (dasd_device_t * device, int recid, int cmd)
{
	dasd_eckd_private_t *private = (dasd_eckd_private_t *) device->private;
	int byt_per_blk = device->sizes.bp_block;
	int blk_per_trk = recs_per_track (&(private->rdc_data), 0, byt_per_blk);
	switch (cmd) {
	case READ:
		if (recid < 3)
			return DASD_ECKD_CCW_READ_KD_MT;
		if (recid < blk_per_trk)
			return DASD_ECKD_CCW_READ_MT;
		if (recid < 2 * blk_per_trk)
			return DASD_ECKD_CCW_READ_KD_MT;
		return DASD_ECKD_CCW_READ_MT;
		break;
	case WRITE:
		if (recid < 3)
			return DASD_ECKD_CCW_WRITE_KD_MT;
		if (recid < blk_per_trk)
			return DASD_ECKD_CCW_WRITE_MT;
		if (recid < 2 * blk_per_trk)
			return DASD_ECKD_CCW_WRITE_KD_MT;
		return DASD_ECKD_CCW_WRITE_MT;
		break;
	default:
		BUG ();
	}
	return 0;		// never executed
}


static ccw_req_t *
dasd_eckd_build_cp_from_req (dasd_device_t * device, struct request *req)
{
	ccw_req_t *rw_cp = NULL;
	int rw_cmd;
	int bhct;
	long size;
	ccw1_t *ccw;
	DE_eckd_data_t *DE_data;
	LO_eckd_data_t *LO_data;
	struct buffer_head *bh;
	dasd_eckd_private_t *private = (dasd_eckd_private_t *) device->private;
	int byt_per_blk = device->sizes.bp_block;
	int shift = device->sizes.s2b_shift;
	int blk_per_trk = recs_per_track (&(private->rdc_data), 0, byt_per_blk);
        unsigned long reloc_sector = req->sector + 
                device->major_info->gendisk.part[MINOR(req->rq_dev)].start_sect;
        int btrk = (reloc_sector >> shift) / blk_per_trk; 
	int etrk = ((reloc_sector + req->nr_sectors - 1) >> shift) / blk_per_trk;
	int recid = reloc_sector >> shift;
	int locate4k_set = 0;
	int nlocs = 0;
	int errcode;

	if (req->cmd == READ) {
		rw_cmd = DASD_ECKD_CCW_READ_MT;
	} else if (req->cmd == WRITE) {
		rw_cmd = DASD_ECKD_CCW_WRITE_MT;
	} else {

		MESSAGE (KERN_ERR,
                         "Unknown command %d", 
                         req->cmd);

		return ERR_PTR(-EINVAL);
	}
	/* Build the request */
	/* count bhs to prevent errors, when bh smaller than block */
	bhct = 0;
	for (bh = req->bh; bh; bh = bh->b_reqnext) {
		if (bh->b_size < byt_per_blk) {
			MESSAGE(KERN_ERR, "ignoring bogus sized request: %d<%d",
					  bh->b_size, byt_per_blk);
                        return ERR_PTR(-EINVAL);
		}
                bhct+= bh->b_size >> (device->sizes.s2b_shift+9);
	}
	if (btrk < 2 && private->uses_cdl) {
		if (etrk < 2)
                        nlocs = bhct;
                else
                        nlocs = 2 * blk_per_trk - recid;
	}
	rw_cp = dasd_alloc_request (dasd_eckd_discipline.name, 
                                    2 + nlocs + bhct + 1,
                                    sizeof (DE_eckd_data_t) + (1 +
                                                               nlocs) *
                                    sizeof (LO_eckd_data_t),
                                    device);
	if (!rw_cp) {
		return ERR_PTR(-ENOMEM);
	}
	DE_data = rw_cp->data;
	LO_data = rw_cp->data + sizeof (DE_eckd_data_t);
	ccw = rw_cp->cpaddr;
	if ((errcode = define_extent (ccw, DE_data, btrk, etrk, 
                                      rw_cmd, device, rw_cp))) {
                goto clear_rw_cp;
        }
	ccw->flags |= CCW_FLAG_CC;
	for (bh = req->bh; bh != NULL;) {
                for (size = 0; size < bh->b_size; size += byt_per_blk) {
                        if (!locate4k_set) {
                                // we need to chain a locate record before our rw-ccw
                                ccw++;
                                if ((recid / blk_per_trk) < 2
                                    && private->uses_cdl) {
                                        /* Do a locate record for our special blocks */
                                        int cmd = dasd_eckd_cdl_cmd (device,recid, req->cmd);
                                        if ((errcode = locate_record (ccw, 
                                                       LO_data++,
                                                       recid / blk_per_trk, 
                                                       recid % blk_per_trk + 1, 
                                                       1, cmd, device,
                                                       dasd_eckd_cdl_reclen(device, recid), rw_cp))) {
                                                goto clear_rw_cp;
                                        }
                                } else {
                                        // Do a locate record for standard blocks */
                                        if ((errcode = locate_record (ccw, 
                                                       LO_data++,
                                                       recid /blk_per_trk,
                                                       recid %blk_per_trk + 1,
                                                       (((reloc_sector +
                                                          req->nr_sectors) >>
                                                         shift) - recid), 
                                                       rw_cmd, device,
                                                       device->sizes.bp_block, rw_cp))) {
                                                goto clear_rw_cp;
                                        }
                                        locate4k_set = 1;
                                }
                                ccw->flags |= CCW_FLAG_CC;
                        }
                        ccw++;
                        ccw->flags |= CCW_FLAG_CC;
                        ccw->cmd_code = locate4k_set ? rw_cmd :
                                dasd_eckd_cdl_cmd (device, recid, req->cmd);
                        ccw->count = byt_per_blk;
                        if (!locate4k_set) {
                                ccw->count = dasd_eckd_cdl_reclen (device,recid);
                                if (ccw->count < byt_per_blk) {
                                    memset (bh->b_data + size + ccw->count,
                                            0xE5, byt_per_blk - ccw->count);
                                }
                        }
                        if ((errcode = dasd_set_normalized_cda (ccw, __pa (bh->b_data+size), 
                                                                rw_cp, device))) {
                                goto clear_rw_cp;
                        }
                        recid++;
                }
                bh = bh->b_reqnext;
	}
	ccw->flags    &= ~(CCW_FLAG_DC | CCW_FLAG_CC);
	rw_cp->device  = device;
	rw_cp->expires = 5 * TOD_MIN;	/* 5 minutes */
	rw_cp->req     = req;
	rw_cp->lpm     = LPM_ANYPATH;
	rw_cp->retries = 256;

	rw_cp->buildclk = get_clock ();

	check_then_set (&rw_cp->status, 
                        CQR_STATUS_EMPTY, 
                        CQR_STATUS_FILLED);

        goto out;
 clear_rw_cp:
        dasd_free_request (rw_cp, 
                           device);
        rw_cp=ERR_PTR(errcode);
 out:
	return rw_cp;
}

#if 0
int
dasd_eckd_cleanup_request (ccw_req_t * cqr)
{
	int ret = 0;
	struct request *req = cqr->req;
	dasd_device_t *device = cqr->device;
	int byt_per_blk = device->sizes.bp_block;

	for (bh = req->bh; bh != NULL;) {
		if (bh->b_size > byt_per_blk) {
			for (size = 0; size < bh->b_size; size += byt_per_blk) {
				ccw++;
				ccw->flags |= CCW_FLAG_CC;
				ccw->cmd_code = rw_cmd;
				ccw->count = byt_per_blk;
				set_normalized_cda (ccw,
						    __pa (bh->b_data + size));
			}
			bh = bh->b_reqnext;
		} else {	/* group N bhs to fit into byt_per_blk */
			for (size = 0; bh != NULL && size < byt_per_blk;) {
				ccw++;
				ccw->flags |= CCW_FLAG_DC;
				ccw->cmd_code = rw_cmd;
				ccw->count = bh->b_size;
				set_normalized_cda (ccw, __pa (bh->b_data));
				size += bh->b_size;
				bh = bh->b_reqnext;
			}
		}
	}
	return ret;
}
#endif

/*
 * DASD_ECKD_RESERVE
 *
 * DESCRIPTION
 *    Buils a channel programm to reserve a device.
 *    Options are set to 'synchronous wait for interrupt' and
 *    'timeout the request'. This leads to an terminate IO if 
 *    the interrupt is outstanding for a certain time. 
 */
ccw_req_t *
dasd_eckd_reserve (struct dasd_device_t * device)
{
	ccw_req_t *cqr;

        cqr = dasd_alloc_request (dasd_eckd_discipline.name, 
                                  1 + 1, 32, device);
	if (cqr == NULL) {

		MESSAGE (KERN_WARNING, "%s",
                         "No memory to allocate initialization request");

		return NULL;
	}
	cqr->cpaddr->cmd_code = DASD_ECKD_CCW_RESERVE;
        cqr->cpaddr->flags |= CCW_FLAG_SLI;
        cqr->cpaddr->count = 32;

	if (dasd_set_normalized_cda (cqr->cpaddr, __pa (cqr->data), 
                                     cqr, device)) {
                dasd_free_request (cqr, device);
                return NULL;
        }
        
	cqr->device  = device;
	cqr->retries = 0;
	cqr->expires = 10 * TOD_SEC;
	cqr->buildclk = get_clock ();
	cqr->status  = CQR_STATUS_FILLED;
	return cqr; 
}

/*
 * DASD_ECKD_RELEASE
 *
 * DESCRIPTION
 *    Buils a channel programm to releases a prior reserved 
 *    (see dasd_eckd_reserve) device.
 */
ccw_req_t *
dasd_eckd_release (struct dasd_device_t * device)
{
	ccw_req_t *cqr;

        cqr = dasd_alloc_request (dasd_eckd_discipline.name, 
                                  1 + 1, 32, device);
	if (cqr == NULL) {

		MESSAGE (KERN_WARNING, "%s",
                         "No memory to allocate initialization request");

		return NULL;
	}
	cqr->cpaddr->cmd_code = DASD_ECKD_CCW_RELEASE;
        cqr->cpaddr->flags |= CCW_FLAG_SLI;
        cqr->cpaddr->count = 32;

	if (dasd_set_normalized_cda (cqr->cpaddr, __pa (cqr->data), 
                                     cqr, device)) {
                dasd_free_request (cqr, device);
                return NULL;
        }

	cqr->device  = device;
	cqr->retries = 0;
	cqr->expires = 10 * TOD_SEC;
	cqr->buildclk = get_clock ();
	cqr->status  = CQR_STATUS_FILLED;
	return cqr;

}

/*
 * DASD_ECKD_STEAL_LOCK
 *
 * DESCRIPTION
 *    Buils a channel programm to break a device's reservation. 
 *    (unconditional reserve)
 */
ccw_req_t *
dasd_eckd_steal_lock (struct dasd_device_t * device)
{
	ccw_req_t *cqr;

        cqr = dasd_alloc_request (dasd_eckd_discipline.name, 
                                  1 + 1, 32, device);
	if (cqr == NULL) {

		MESSAGE (KERN_WARNING, "%s",
			"No memory to allocate initialization request");

		return NULL;
	}
	cqr->cpaddr->cmd_code = DASD_ECKD_CCW_SLCK;
        cqr->cpaddr->flags |= CCW_FLAG_SLI;
        cqr->cpaddr->count = 32;

	if (dasd_set_normalized_cda (cqr->cpaddr, __pa (cqr->data), 
                                     cqr, device)) {
                dasd_free_request (cqr, device);
                return NULL;
        }
        
	cqr->device  = device;
	cqr->retries = 0;
	cqr->expires = 10 * TOD_SEC;
	cqr->buildclk = get_clock ();
	cqr->status  = CQR_STATUS_FILLED;
	return cqr;
}

static inline ccw1_t *
dasd_eckd_find_cmd (ccw_req_t * cqr, int cmd)
{
	ccw1_t *cp;

	cp = cqr->cpaddr;
	do {
		if (cp->cmd_code == cmd)
			return cp;
		if (cp->cmd_code == CCW_CMD_TIC) {
			cp = (ccw1_t *) (long) cp->cda;
			continue;
		}
		if (cp->flags & (CCW_FLAG_DC | CCW_FLAG_CC)) {
			cp++;
			continue;
		}
		break;
	} while (1);
	return NULL;
}

static ccw_req_t *
dasd_eckd_merge_cp (dasd_device_t * device)
{
	return NULL;
}

static int
dasd_eckd_fill_info (dasd_device_t * device, dasd_information2_t * info)
{
	int rc = 0;
	info->label_block = 2;
	if (((dasd_eckd_private_t *) device->private)->uses_cdl) {
		info->FBA_layout = 0;
                info->format = DASD_FORMAT_CDL;
        } else {
                info->FBA_layout = 1;
                info->format = DASD_FORMAT_LDL;
        }
	info->characteristics_size = sizeof (dasd_eckd_characteristics_t);
	memcpy (info->characteristics,
		&((dasd_eckd_private_t *) device->private)->rdc_data,
		sizeof (dasd_eckd_characteristics_t));
	info->confdata_size = sizeof (dasd_eckd_confdata_t);
	memcpy (info->configuration_data,
		&((dasd_eckd_private_t *) device->private)->conf_data,
		sizeof (dasd_eckd_confdata_t));
	return rc;
}

/*
 * DASD_ECKD_READ_STATS
 * 
 * DESCRIPTION
 *   build the channel program to read the performance statistics
 *   of the attached subsystem
 */
ccw_req_t *
dasd_eckd_read_stats (struct dasd_device_t * device)
{

        int                    rc;
	ccw1_t                 *ccw;
	ccw_req_t              *cqr;
	dasd_psf_prssd_data_t  *prssdp;
	dasd_rssd_perf_stats_t *statsp;

        cqr = dasd_alloc_request (dasd_eckd_discipline.name, 
                                  1 /* PSF */ + 1 /* RSSD */,
                                  (sizeof (dasd_psf_prssd_data_t) +  
                                   sizeof (dasd_rssd_perf_stats_t) ), 
                                  device);

	if (cqr == NULL) {
                
                MESSAGE (KERN_WARNING, "%s",
                         "No memory to allocate initialization request");
                
		return NULL;
	}

	cqr->device  = device;
	cqr->retries = 0;
	cqr->expires = 10 * TOD_SEC;

        /* Prepare for Read Subsystem Data */
	prssdp = (dasd_psf_prssd_data_t *) cqr->data;

	memset (prssdp, 0, sizeof (dasd_psf_prssd_data_t));

	prssdp->order     = PSF_ORDER_PRSSD;
	prssdp->suborder  = 0x01; /* Perfomance Statistics */
	prssdp->varies[1] = 0x01; /* Perf Statistics for the Subsystem */

	ccw = cqr->cpaddr;

	ccw->cmd_code = DASD_ECKD_CCW_PSF;
	ccw->count    = sizeof (dasd_psf_prssd_data_t);
	ccw->flags   |= CCW_FLAG_CC;

	if ((rc = dasd_set_normalized_cda (ccw,__pa (prssdp), cqr, device))) {

                dasd_free_request (cqr, 
                                   device);
                return NULL;
        }

	ccw++;

        /* Read Subsystem Data - Performance Statistics */
	statsp = (dasd_rssd_perf_stats_t *) (prssdp + 1);
	memset (statsp, 0, sizeof (dasd_rssd_perf_stats_t));

	ccw->cmd_code = DASD_ECKD_CCW_RSSD;
	ccw->count    = sizeof (dasd_rssd_perf_stats_t);

	if ((rc = dasd_set_normalized_cda (ccw,__pa (statsp), cqr, device))) {

                dasd_free_request (cqr, 
                                   device);
                return NULL;
        }
        cqr->buildclk = get_clock ();
	cqr->status   = CQR_STATUS_FILLED;

	return cqr; 
} /* end  dasd_eckd_rstat */

/*
 * DASD_ECKD_RET_STATS
 * 
 * DESCRIPTION
 *   returns pointer to Performance Statistics Data.
 */
dasd_rssd_perf_stats_t *
dasd_eckd_ret_stats (ccw_req_t *cqr)
{
        
        dasd_psf_prssd_data_t  *prssdp;
	dasd_rssd_perf_stats_t *statsp;
        
	if (cqr == NULL) {

		return NULL;
	}

        /* Prepare for Read Subsystem Data */
	prssdp = (dasd_psf_prssd_data_t *)  cqr->data;
	statsp = (dasd_rssd_perf_stats_t *) (prssdp + 1);

	return statsp;

} /* end  dasd_eckd_rstat */


/*
 * DASD_ECKD_GET_ATTRIB
 * 
 * DESCRIPTION
 *   returnes the cache attributes used in Define Extend (DE).
 */
int
dasd_eckd_get_attrib (dasd_device_t *device,
                      attrib_data_t *attrib)
{
	dasd_eckd_private_t *private;

        private = (dasd_eckd_private_t *) device->private;
        *attrib = private->attrib;
        
        return 0;
        
} /* end dasd_eckd_get_attrib */

/*
 * DASD_ECKD_SET_ATTRIB
 * 
 * DESCRIPTION
 *   stores the attributes for cache operation to be used in Define Extend (DE).
 */
int
dasd_eckd_set_attrib (dasd_device_t *device,
                      attrib_data_t *attrib)
{
	dasd_eckd_private_t *private;

        private = (dasd_eckd_private_t *) device->private;
        private->attrib = *attrib;

        DBF_DEV_EVENT (DBF_ERR, device,
                     "cache operation mode set to "
                     "%x (%i cylinder prestage)",
                     private->attrib.operation,
                     private->attrib.nr_cyl);

        return 0;
        
} /* end dasd_eckd_set_attrib */

static void
dasd_eckd_dump_sense (struct dasd_device_t *device, 
                      ccw_req_t            *req)
{

	char *page = (char *) get_free_page (GFP_ATOMIC);
	devstat_t *stat = &device->dev_status;
	char *sense = stat->ii.sense.data;
	int len, sl, sct;

	if (page == NULL) {

                MESSAGE (KERN_ERR, "%s",
                        "No memory to dump sense data");

		return;
	}

	len = sprintf (page, KERN_ERR PRINTK_HEADER
		       "device %04X on irq %d: I/O status report:\n",
		       device->devinfo.devno, device->devinfo.irq);
	len += sprintf (page + len, KERN_ERR PRINTK_HEADER
			"in req: %p CS: 0x%02X DS: 0x%02X\n",
			req, stat->cstat, stat->dstat);
	len += sprintf (page + len, KERN_ERR PRINTK_HEADER
			"Failing CCW: %p\n", (void *) (long) stat->cpa);
	{

		ccw1_t *act = req->cpaddr;
		int i = req->cplength;

		do {

			DBF_EVENT (DBF_INFO, 
                                   "CCW %p: %08X %08X",
                                   act, 
                                   ((int *) act)[0], 
                                   ((int *) act)[1]);

			DBF_EVENT (DBF_INFO, 
                                   "DAT: %08X %08X %08X %08X",
                                   ((int *) (addr_t) act->cda)[0], 
                                   ((int *) (addr_t) act->cda)[1],
                                   ((int *) (addr_t) act->cda)[2], 
                                   ((int *) (addr_t) act->cda)[3]);

			act++;

		} while (--i);
	}
	if (stat->flag & DEVSTAT_FLAG_SENSE_AVAIL) {
		for (sl = 0; sl < 4; sl++) {
			len += sprintf (page + len, KERN_ERR PRINTK_HEADER
					"Sense(hex) %2d-%2d:",
					(8 * sl), ((8 * sl) + 7));

			for (sct = 0; sct < 8; sct++) {
				len += sprintf (page + len, " %02x",
						sense[8 * sl + sct]);
			}
			len += sprintf (page + len, "\n");
		}

		if (sense[27] & DASD_SENSE_BIT_0) {
			/* 24 Byte Sense Data */
			len += sprintf (page + len, KERN_ERR PRINTK_HEADER
					"24 Byte: %x MSG %x, %s MSGb to SYSOP\n",
					sense[7] >> 4, sense[7] & 0x0f,
					sense[1] & 0x10 ? "" : "no");
		} else {
			/* 32 Byte Sense Data */
			len += sprintf (page + len, KERN_ERR PRINTK_HEADER
					"32 Byte: Format: %x "
                                        "Exception class %x\n",
					sense[6] & 0x0f, sense[22] >> 4);
		}
	}

        MESSAGE_LOG (KERN_ERR,
                     "Sense data:\n%s", 
                     page);

        free_page ((unsigned long) page);
}


dasd_discipline_t dasd_eckd_discipline = {
        owner: THIS_MODULE,
	name:"ECKD",
	ebcname:"ECKD",
	max_blocks:240,
	id_check:dasd_eckd_id_check,
	check_characteristics:dasd_eckd_check_characteristics,
	init_analysis:dasd_eckd_init_analysis,
	do_analysis:dasd_eckd_do_analysis,
	fill_geometry:dasd_eckd_fill_geometry,
	start_IO:dasd_start_IO,
	term_IO:dasd_term_IO,
	format_device:dasd_eckd_format_device,
	examine_error:dasd_eckd_examine_error,
	erp_action:dasd_eckd_erp_action,
	erp_postaction:dasd_eckd_erp_postaction,
	build_cp_from_req:dasd_eckd_build_cp_from_req,
	dump_sense:dasd_eckd_dump_sense,
	int_handler:dasd_int_handler,
	reserve:dasd_eckd_reserve,
	release:dasd_eckd_release,
        steal_lock:dasd_eckd_steal_lock,
	merge_cp:dasd_eckd_merge_cp,
	fill_info:dasd_eckd_fill_info,
        read_stats:dasd_eckd_read_stats,
        ret_stats:dasd_eckd_ret_stats,
        get_attrib:dasd_eckd_get_attrib,
        set_attrib:dasd_eckd_set_attrib,
	list:LIST_HEAD_INIT(dasd_eckd_discipline.list),
};

int
dasd_eckd_init (void)
{
	int rc = 0;

	MESSAGE (KERN_INFO,
		"%s discipline initializing", 
                 dasd_eckd_discipline.name);

	ASCEBC (dasd_eckd_discipline.ebcname, 4);
	dasd_discipline_add (&dasd_eckd_discipline);
#ifdef CONFIG_DASD_DYNAMIC
	{
		int i;
		for (i = 0;
		     i < sizeof (dasd_eckd_known_devices) / sizeof (devreg_t);
		     i++) {

			MESSAGE (KERN_INFO,
                                 "We are interested in: CU %04X/%02x",
                                 dasd_eckd_known_devices[i].ci.hc.ctype,
                                 dasd_eckd_known_devices[i].ci.hc.cmode);

			s390_device_register (&dasd_eckd_known_devices[i]);
		}
	}
#endif				/* CONFIG_DASD_DYNAMIC */
	return rc;
}

void
dasd_eckd_cleanup (void)
{

	MESSAGE (KERN_INFO,
		"%s discipline cleaning up", 
                 dasd_eckd_discipline.name);

#ifdef CONFIG_DASD_DYNAMIC
	{
		int i;
		for (i = 0;
		     i < sizeof (dasd_eckd_known_devices) / sizeof (devreg_t);
		     i++) {

			MESSAGE (KERN_INFO,
                                 "We were interested in: CU %04X/%02x",
                                 dasd_eckd_known_devices[i].ci.hc.ctype,
                                 dasd_eckd_known_devices[i].ci.hc.cmode);

			s390_device_unregister (&dasd_eckd_known_devices[i]);
		}
	}
#endif				/* CONFIG_DASD_DYNAMIC */
	dasd_discipline_del (&dasd_eckd_discipline);
}

#ifdef MODULE
int
init_module (void)
{
	int rc = 0;
	rc = dasd_eckd_init ();
	return rc;
}

void
cleanup_module (void)
{
	dasd_eckd_cleanup ();
	return;
}
#endif

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4 
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
