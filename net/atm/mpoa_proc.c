#include <linux/config.h>

#ifdef CONFIG_PROC_FS
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h> 
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <asm/uaccess.h>
#include <linux/atmmpc.h>
#include <linux/atm.h>
#include "mpc.h"
#include "mpoa_caches.h"

/*
 * mpoa_proc.c: Implementation MPOA client's proc
 * file system statistics 
 */

#if 1
#define dprintk printk   /* debug */
#else
#define dprintk(format,args...)
#endif

#define STAT_FILE_NAME "mpc"     /* Our statistic file's name */

extern struct mpoa_client *mpcs;
extern struct proc_dir_entry *atm_proc_root;  /* from proc.c. */

static ssize_t proc_mpc_read(struct file *file, char *buff,
			     size_t count, loff_t *pos);

static ssize_t proc_mpc_write(struct file *file, const char *buff,
                              size_t nbytes, loff_t *ppos);

static int parse_qos(const char *buff, int len);

/*
 *   Define allowed FILE OPERATIONS
 */
static struct file_operations mpc_file_operations = {
	read:		proc_mpc_read,
	write:		proc_mpc_write,
};

static int print_header(char *buff,struct mpoa_client *mpc){
        if(mpc != NULL){
	        return sprintf(buff,"\nInterface %d:\n\n",mpc->dev_num);  
	  
	}
	return 0;
}

/*
 * Returns the state of an ingress cache entry as a string
 */
static const char *ingress_state_string(int state){
        switch(state) {
	case INGRESS_RESOLVING:
	        return "resolving  ";
		break;
	case INGRESS_RESOLVED:
                return "resolved   ";
		break;
	case INGRESS_INVALID:
	        return "invalid    ";
		break;
	case INGRESS_REFRESHING:
	        return "refreshing ";
		break;
	default:
	       return "";
	}
}

/*
 * Returns the state of an egress cache entry as a string
 */
static const char *egress_state_string(int state){
        switch(state) {
	case EGRESS_RESOLVED:
	        return "resolved   ";
		break;
	case EGRESS_PURGE:
                return "purge      ";
		break;
	case EGRESS_INVALID:
	        return "invalid    ";
		break;
	default:
	       return "";
	}
}

/*
 * READING function - called when the /proc/atm/mpoa file is read from.
 */
static ssize_t proc_mpc_read(struct file *file, char *buff,
			     size_t count, loff_t *pos){
        unsigned long page = 0;
	unsigned char *temp;
        int length = 0;
	int i = 0;
	struct mpoa_client *mpc = mpcs;
	in_cache_entry *in_entry;
	eg_cache_entry *eg_entry;
	struct timeval now;
	unsigned char ip_string[16];
	if(count == 0)
	        return 0;
	page = get_free_page(GFP_KERNEL);
	if(!page)
	        return -ENOMEM;
	atm_mpoa_disp_qos((char *)page, &length);
	while(mpc != NULL){
	        length += print_header((char *)page + length, mpc);
		length += sprintf((char *)page + length,"Ingress Entries:\nIP address      State      Holding time  Packets fwded  VPI  VCI\n");
		in_entry = mpc->in_cache;
		do_gettimeofday(&now);
		while(in_entry != NULL){
		        temp = (unsigned char *)&in_entry->ctrl_info.in_dst_ip;                        sprintf(ip_string,"%d.%d.%d.%d", temp[0], temp[1], temp[2], temp[3]);
		        length += sprintf((char *)page + length,"%-16s%s%-14lu%-12u", ip_string, ingress_state_string(in_entry->entry_state), (in_entry->ctrl_info.holding_time-(now.tv_sec-in_entry->tv.tv_sec)), in_entry->packets_fwded);
			if(in_entry->shortcut)
			        length += sprintf((char *)page + length,"   %-3d  %-3d",in_entry->shortcut->vpi,in_entry->shortcut->vci);
			length += sprintf((char *)page + length,"\n");
			in_entry = in_entry->next;
		}
		length += sprintf((char *)page + length,"\n");
		eg_entry = mpc->eg_cache;
		length += sprintf((char *)page + length,"Egress Entries:\nIngress MPC ATM addr\nCache-id        State      Holding time  Packets recvd  Latest IP addr   VPI VCI\n");
		while(eg_entry != NULL){
		  for(i=0;i<ATM_ESA_LEN;i++){
		          length += sprintf((char *)page + length,"%02x",eg_entry->ctrl_info.in_MPC_data_ATM_addr[i]);}  
		        length += sprintf((char *)page + length,"\n%-16lu%s%-14lu%-15u",(unsigned long) ntohl(eg_entry->ctrl_info.cache_id), egress_state_string(eg_entry->entry_state), (eg_entry->ctrl_info.holding_time-(now.tv_sec-eg_entry->tv.tv_sec)), eg_entry->packets_rcvd);
			
			/* latest IP address */
			temp = (unsigned char *)&eg_entry->latest_ip_addr;
			sprintf(ip_string, "%d.%d.%d.%d", temp[0], temp[1], temp[2], temp[3]);
			length += sprintf((char *)page + length, "%-16s", ip_string);

			if(eg_entry->shortcut)
			        length += sprintf((char *)page + length," %-3d %-3d",eg_entry->shortcut->vpi,eg_entry->shortcut->vci);
			length += sprintf((char *)page + length,"\n");
			eg_entry = eg_entry->next;
		}
		length += sprintf((char *)page + length,"\n");
		mpc = mpc->next;
	}

	if (*pos >= length) length = 0;
	else {
	  if ((count + *pos) > length) count = length - *pos;
	  if (copy_to_user(buff, (char *)page , count)) {
 		  free_page(page);
		  return -EFAULT;
          }
	  *pos += count;
	}

 	free_page(page);
        return length;
}

static ssize_t proc_mpc_write(struct file *file, const char *buff,
                              size_t nbytes, loff_t *ppos)
{
        int incoming, error, retval;
        char *page, c;
        const char *tmp;

        if (nbytes == 0) return 0;
        if (nbytes >= PAGE_SIZE) nbytes = PAGE_SIZE-1;

        error = verify_area(VERIFY_READ, buff, nbytes);
        if (error) return error;

        page = (char *)__get_free_page(GFP_KERNEL);
        if (page == NULL) return -ENOMEM;

        incoming = 0;
        tmp = buff;
        while(incoming < nbytes){
                if (get_user(c, tmp++)) return -EFAULT;
                incoming++;
                if (c == '\0' || c == '\n')
                        break;
        }

        retval = copy_from_user(page, buff, incoming);
        if (retval != 0) {
                printk("mpoa: proc_mpc_write: copy_from_user() failed\n");
                return -EFAULT;
        }

        *ppos += incoming;

        page[incoming] = '\0';
	retval = parse_qos(page, incoming);
        if (retval == 0)
                printk("mpoa: proc_mpc_write: could not parse '%s'\n", page);

        free_page((unsigned long)page);
        
        return nbytes;
}

static int parse_qos(const char *buff, int len)
{
        /* possible lines look like this
         * add 130.230.54.142 tx=max_pcr,max_sdu rx=max_pcr,max_sdu
         */
        
        int pos, i;
        uint32_t ipaddr;
        unsigned char ip[4]; 
        char cmd[4], temp[256];
        const char *tmp, *prev;
	struct atm_qos qos; 
	int value[5];
        
        memset(&qos, 0, sizeof(struct atm_qos));
        strncpy(cmd, buff, 3);
        if( strncmp(cmd,"add", 3) &&  strncmp(cmd,"del", 3))
	        return 0;  /* not add or del */

	pos = 4;
        /* next parse ip */
        prev = buff + pos;
        for (i = 0; i < 3; i++) {
                tmp = strchr(prev, '.');
                if (tmp == NULL) return 0;
                memset(temp, '\0', 256);
                memcpy(temp, prev, tmp-prev);
                ip[i] = (char)simple_strtoul(temp, NULL, 0);
		tmp ++; 
		prev = tmp;
        }
	tmp = strchr(prev, ' ');
        if (tmp == NULL) return 0;
        memset(temp, '\0', 256);
        memcpy(temp, prev, tmp-prev);
        ip[i] = (char)simple_strtoul(temp, NULL, 0);
        ipaddr = *(uint32_t *)ip;
                
	if(!strncmp(cmd, "del", 3))
	         return atm_mpoa_delete_qos(atm_mpoa_search_qos(ipaddr));

        /* next transmit values */
	tmp = strstr(buff, "tx=");
	if(tmp == NULL) return 0;
	tmp += 3;
	prev = tmp;
	for( i = 0; i < 1; i++){
	         tmp = strchr(prev, ',');
		 if (tmp == NULL) return 0;
		 memset(temp, '\0', 256);
		 memcpy(temp, prev, tmp-prev);
		 value[i] = (int)simple_strtoul(temp, NULL, 0);
		 tmp ++; 
		 prev = tmp;
	}
	tmp = strchr(prev, ' ');
        if (tmp == NULL) return 0;
	memset(temp, '\0', 256);
        memcpy(temp, prev, tmp-prev);
        value[i] = (int)simple_strtoul(temp, NULL, 0);
	qos.txtp.traffic_class = ATM_CBR;
	qos.txtp.max_pcr = value[0];
	qos.txtp.max_sdu = value[1];

        /* next receive values */
	tmp = strstr(buff, "rx=");
	if(tmp == NULL) return 0;
        if (strstr(buff, "rx=tx")) { /* rx == tx */
                qos.rxtp.traffic_class = qos.txtp.traffic_class;
                qos.rxtp.max_pcr = qos.txtp.max_pcr;
                qos.rxtp.max_cdv = qos.txtp.max_cdv;
                qos.rxtp.max_sdu = qos.txtp.max_sdu;
        } else {
                tmp += 3;
                prev = tmp;
                for( i = 0; i < 1; i++){
                        tmp = strchr(prev, ',');
                        if (tmp == NULL) return 0;
                        memset(temp, '\0', 256);
                        memcpy(temp, prev, tmp-prev);
                        value[i] = (int)simple_strtoul(temp, NULL, 0);
                        tmp ++; 
                        prev = tmp;
                }
                tmp = strchr(prev, '\0');
                if (tmp == NULL) return 0;
                memset(temp, '\0', 256);
                memcpy(temp, prev, tmp-prev);
                value[i] = (int)simple_strtoul(temp, NULL, 0);
                qos.rxtp.traffic_class = ATM_CBR;
                qos.rxtp.max_pcr = value[0];
                qos.rxtp.max_sdu = value[1];
        }
        qos.aal = ATM_AAL5;
	dprintk("mpoa: mpoa_proc.c: parse_qos(): setting qos paramameters to tx=%d,%d rx=%d,%d\n",
		qos.txtp.max_pcr,
		qos.txtp.max_sdu,
		qos.rxtp.max_pcr,
		qos.rxtp.max_sdu
		);

	atm_mpoa_add_qos(ipaddr, &qos);
	return 1;
}

/*
 * INITIALIZATION function - called when module is initialized/loaded.
 */
int mpc_proc_init(void)
{
	struct proc_dir_entry *p;

        p = create_proc_entry(STAT_FILE_NAME, 0, atm_proc_root);
	if (!p) {
                printk(KERN_ERR "Unable to initialize /proc/atm/%s\n", STAT_FILE_NAME);
                return -ENOMEM;
        }
	p->proc_fops = &mpc_file_operations;
	p->owner = THIS_MODULE;
	return 0;
}

/*
 * DELETING function - called when module is removed.
 */
void mpc_proc_clean(void)
{
	remove_proc_entry(STAT_FILE_NAME,atm_proc_root);
}


#endif /* CONFIG_PROC_FS */






