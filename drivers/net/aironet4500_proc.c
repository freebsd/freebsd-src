/*
 *	 Aironet 4500 /proc interface
 *
 *		Elmer Joandi, Januar 1999
 *	Copyright GPL
 *	
 *
 *	Revision 0.1 ,started  30.12.1998
 *
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/kernel.h>

#include <linux/version.h>

#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/bitops.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>


#ifdef CONFIG_PROC_FS

#ifdef CONFIG_PROC_FS
#include <linux/sysctl.h>
#else
#error awc driver needs CONFIG_PROC_FS
#endif


#include "aironet4500.h"
#include "aironet4500_rid.c"


#define AWC_STR_SIZE 	0x2ff0
#define DEV_AWC_INFO 	1
#define DEV_AWC 	1

struct awc_proc_private{
	struct ctl_table_header *	sysctl_header;
  	struct ctl_table	*	proc_table;
  	struct ctl_table		proc_table_device_root[2];
  	struct ctl_table		proc_table_sys_root[2];
	char 				proc_name[10];
};	        
static char awc_drive_info[AWC_STR_SIZE]="Zcom \n\0";
static char awc_proc_buff[AWC_STR_SIZE];
static int  awc_int_buff;
static struct awc_proc_private awc_proc_priv[MAX_AWCS]; 

extern int awc_proc_unset_device(int device_number);

int awc_proc_format_array(int write,char * buff, size_t * len, struct awc_rid_dir * rid_dir, struct aironet4500_RID * rid){

  u8 * data = rid_dir->buff + rid->offset;
  int pos = 0;
  int null_past = 0;
  int hex = ((rid->mask == 0xff) && (rid->value == 0x0 ));
  int string = ((rid->mask == 0) && (rid->value == 0 ));
  u32 val =0;
  int bytes = (rid->bits / 8);
  int ch =0;
  int i,k;
  int array_len = rid->array;
  int nullX = 0;

 
  	AWC_ENTRY_EXIT_DEBUG("awc_proc_format_array");

      if (rid->bits %8 ) bytes +=1;
     
     if (bytes > 4 && rid->array == 1){
     	array_len = bytes;
     	bytes = 1;
     	hex = 1;
     };
     if (bytes < 1 || bytes > 4){
     	printk(KERN_ERR " weird number of bytes %d in aironet rid \n",bytes);
     	return -1;
     };    	
     DEBUG(0x20000,"awc proc array  bytes %d",bytes);
     DEBUG(0x20000," hex %d",hex);
     DEBUG(0x20000," string %d",string);

     DEBUG(0x20000," array_len %d \n",array_len);
     DEBUG(0x20000," offset %d \n",rid->offset);

     if (!write){
	for (i=0; i < array_len ; i++){
	
		if 	(bytes <= 1 ) val = data[i*bytes];
		else if (bytes <= 2 ) val = *((u16 *)&data[i*bytes]);
		else if (bytes <= 4 ) val = *((u32 *)&data[i*bytes]);
		
		if (rid->null_terminated && !val)
			null_past =1;
			 
		if (hex && !string)
			for (k=0; k <bytes; k++)
				pos += sprintf(buff+pos, "%02x",(unsigned char ) data[i*bytes +k]);
		else if (string)
			pos += sprintf(buff+pos, "%c",val);
		else	pos += sprintf(buff+pos, "%c",val);

		DEBUG(0x20000, "awcproc %x %x \n",data[i], val);
	};
	
     } else {
     	for (i=0; i < array_len ; i++){
     	
     		DEBUG(0x20000, "awcproc %x %x \n",data[i], buff[i]);

     		if (hex && ! string){
     			
     			val = 0;
     			
     			for (k=0; k < bytes; k++){
     				val <<= 8;
       				ch = *(buff + 2*i*bytes +k + nullX);
     				if (ch >= '0' && ch <='9')
     					ch -= '0';
     				if (ch >= 'A' && ch <='F')
     					ch -= 'A'+ 0xA;
     				if (ch >= 'a' && ch <='f')
     					ch -= 'a'+ 0xA;
				val += ch <<4;
				k++;
				
     				ch = *(buff + 2*i*bytes +k + nullX);
     				if (val == 0 && (ch == 'X' || ch == 'x')){
     					nullX=2;
     					val = 0;
     					k = -1;
     					continue;
     				};
     				if (ch >= '0' && ch <='9')
     					ch -= '0';
     				if (ch >= 'A' && ch <='F')
     					ch -= 'A'+ 0xA;
     				if (ch >= 'a' && ch <='f')
     					ch -= 'a'+ 0xA;
     					
     				val += ch;
     				if (i*bytes > *len )
     					val = 0;	
     			}
			if (rid->bits <=8 ) 	            data[i*bytes]  = val;
			else if (rid->bits <=16 ) *((u16 *)&data[i*bytes]) = val;
			else if (rid->bits <=32 ) *((u32 *)&data[i*bytes]) = val;
     			if (!val) null_past=1;	
     			
     		} else {
     			for (k=0; k < bytes; k++){
     				data[i*bytes +k] = *(buff + i*bytes +k);
     				if (i*bytes +k > *len || !data[i*bytes +k])
     					null_past = 1;;
     			}
     	
     		}
     		if (null_past){
     			if (rid->bits <=8 ) 	            data[i*bytes]  = 0;
			else if (rid->bits <=16 ) *((u16 *)&data[i*bytes]) = 0;
			else if (rid->bits <=32 ) *((u32 *)&data[i*bytes]) = 0;
		}

     	}
     	
     };
     
	
//     *len = pos;
 
  	AWC_ENTRY_EXIT_DEBUG("awc_proc_format_array");
     return 0;	
};


int awc_proc_format_bits(int write,u32 * buff, size_t* lenp, struct awc_rid_dir * rid_dir, struct aironet4500_RID * rid){

  u8 * data = rid_dir->buff + rid->offset;
  u32 val = 0;
  int not_bool = 0;
 
  	AWC_ENTRY_EXIT_DEBUG("awc_proc_format_bits");

	if ((rid->bits == 8 && rid->mask == 0xff) 	|| 
	    (rid->bits == 16 && rid->mask == 0xffff) 	|| 
	    (rid->bits == 32 && rid->mask == 0xffffffff)   )
	    not_bool = 1;
	    
	if (rid->bits <=8 ) 		val = 		*data;
	else if (rid->bits <=16 ) 	val = *((u16 *)data);
	else if (rid->bits <=32 ) 	val = *((u32 *)data);

	DEBUG(0x20000,"awc proc int enter data %x \n",val);
	DEBUG(0x20000,"awc proc int enter buff %x \n",*buff);
	DEBUG(0x20000,"awc proc int enter intbuff %x \n",awc_int_buff);
	DEBUG(0x20000,"awc proc int enter lenp  %x \n",*lenp);



	if (!write){
		if (rid->mask)
			val &= rid->mask;

		if (!not_bool && rid->mask && 
		    ((val & rid->mask) == (rid->value & rid->mask)))
			*buff = 1;
		else if (!not_bool) *buff = 0;
		else *buff = val;
	} else {
		if (not_bool){
			val &= ~rid->mask; 
			val |= (*buff & rid->mask);
		} else {
			if (*buff){
				val &= ~rid->mask;
				if (rid->value)
					val |= rid->mask & rid->value;
				else 	val |= rid->mask & ~rid->value;
			} else val &= ~rid->mask;
		};
		if (rid->bits == 8) *data = val & 0xff;
		if (rid->bits == 16) *((u16*)data) = val &0xffff;
		if (rid->bits == 32) *((u32*)data) = val &0xffffffff; 
	
	}
	DEBUG(0x20000,"awc proc int buff %x \n",awc_int_buff);
	if (rid->bits <=8 ) 		val = 		*data;
	else if (rid->bits <=16 ) 	val = *((u16 *)data);
	else if (rid->bits <=32 ) 	val = *((u32 *)data);

	DEBUG(0x20000,"awc proc int data %x \n",val);
	
// both of them are crazy
//	*lenp = sizeof(int);
// 	*lenp += 1;
 	
  	AWC_ENTRY_EXIT_DEBUG("exit");
	return 0;

};

int awc_proc_fun(ctl_table *ctl, int write, struct file * filp,
                           void *buffer, size_t *lenp)
{
        int retv =-1;
   	struct awc_private *priv = NULL;
	unsigned long  flags;
//	int device_number = (int ) ctl->extra1;

	struct awc_rid_dir * rid_dir;

	struct net_device * dev= NULL;
	struct aironet4500_RID * rid = (struct aironet4500_RID * ) ctl->extra2;
	
 
  	AWC_ENTRY_EXIT_DEBUG("awc_proc_fun");

	if (!write && filp)
	 if (filp->f_pos){
//	 	printk(KERN_CRIT "Oversize read\n");
		*lenp = 0;// hack against reading til eof
	  	return	0;
	 }
 
	MOD_INC_USE_COUNT;

	rid_dir = ((struct awc_rid_dir *)ctl->extra1);
	dev = rid_dir->dev;
	
	if (!dev){
		printk(KERN_ERR " NO device here \n");
		goto final;
	}

	if(ctl->procname == NULL || awc_drive_info == NULL ){
		printk(KERN_WARNING " procname is NULL in sysctl_table or awc_mib_info is NULL \n at awc module\n ");
		MOD_DEC_USE_COUNT;
		return -1;
	}
	priv = (struct awc_private * ) dev->priv; 

	if ((rid->selector->read_only || rid->read_only) && write){
		printk(KERN_ERR "This value is read-only \n");
		goto final;
	};

	if (!write && rid->selector->may_change) {
		save_flags(flags);
		cli();	
		awc_readrid(dev,rid,rid_dir->buff + rid->offset);
		restore_flags(flags);
	};
	
	if (rid->array > 1 || rid->bits > 32){
		if (write){
        		retv = proc_dostring(ctl, write, filp, buffer, lenp);
        		if (retv) goto final;
			retv = awc_proc_format_array(write, awc_proc_buff, lenp, rid_dir, rid);
			if (retv) goto final;
		} else {
			retv = awc_proc_format_array(write, awc_proc_buff, lenp, rid_dir, rid);
			if (retv) goto final;
        		retv = proc_dostring(ctl, write, filp, buffer, lenp);
			if (retv) goto final;
        	}
        } else {
        	if (write){
        		retv = proc_dointvec(ctl, write, filp, buffer, lenp);        
			if (retv) goto final;	
			retv = awc_proc_format_bits(write, &awc_int_buff, lenp, rid_dir, rid);
			if (retv) goto final;	
		} else {
			retv = awc_proc_format_bits(write, &awc_int_buff, lenp,rid_dir, rid);
			if (retv) goto final;	
        		retv = proc_dointvec(ctl, write, filp, buffer, lenp);        
			if (retv) goto final;	
		}
        }
	if (write) {
		save_flags(flags);
		cli();	

		if (rid->selector->MAC_Disable_at_write){
			awc_disable_MAC(dev);
		};
		awc_writerid(dev,rid,rid_dir->buff + rid->offset);
		if (rid->selector->MAC_Disable_at_write){
			awc_enable_MAC(dev);
		};
		restore_flags(flags);

	};

       	DEBUG(0x20000,"awc proc ret  %x \n",retv);
       	DEBUG(0x20000,"awc proc lenp  %x \n",*lenp);
 
	MOD_DEC_USE_COUNT;
	return retv;
  
final:
 
  	AWC_ENTRY_EXIT_DEBUG("exit");
	MOD_DEC_USE_COUNT;
        return -1 ;
}


char  conf_reset_result[200];


ctl_table awc_exdev_table[] = {
       {0, NULL, NULL,0, 0400, NULL},
       {0}
};
ctl_table awc_exroot_table[] = {
        {254, "aironet4500", NULL, 0, 0555, NULL},
        {0}
};

ctl_table awc_driver_proc_table[] = {
        {1, "debug"			, &awc_debug, sizeof(awc_debug), 0600,NULL, proc_dointvec},
        {2, "bap_sleep"			, &bap_sleep, sizeof(bap_sleep), 0600,NULL, proc_dointvec},
        {3, "bap_sleep_after_setup"	, &bap_sleep_after_setup, sizeof(bap_sleep_after_setup), 0600,NULL, proc_dointvec},
        {4, "sleep_before_command"	, &sleep_before_command, sizeof(sleep_before_command), 0600,NULL, proc_dointvec},
        {5, "bap_sleep_before_write"	, &bap_sleep_before_write, sizeof(bap_sleep_before_write), 0600,NULL, proc_dointvec},
        {6, "sleep_in_command"		, &sleep_in_command	, sizeof(sleep_in_command), 0600,NULL, proc_dointvec},
        {7, "both_bap_lock"		, &both_bap_lock	, sizeof(both_bap_lock), 0600,NULL, proc_dointvec},
        {8, "bap_setup_spinlock"	, &bap_setup_spinlock	, sizeof(bap_setup_spinlock), 0600,NULL, proc_dointvec},
        {0}
};

ctl_table awc_driver_level_ctable[] = {
        {1, "force_rts_on_shorter"	, NULL, sizeof(int), 0600,NULL, proc_dointvec},
        {2, "force_tx_rate"		, NULL, sizeof(int), 0600,NULL, proc_dointvec},
        {3, "ip_tos_reliability_rts"	, NULL, sizeof(int), 0600,NULL, proc_dointvec},
        {4, "ip_tos_troughput_no_retries", NULL, sizeof(int), 0600,NULL, proc_dointvec},
        {5, "debug"			, NULL, sizeof(int), 0600,NULL, proc_dointvec},
        {6, "simple_bridge"		, NULL, sizeof(int), 0600,NULL, proc_dointvec},
        {7, "p802_11_send"		, NULL, sizeof(int), 0600,NULL, proc_dointvec},
        {8, "full_stats"		, NULL, sizeof(int), 0600,NULL, proc_dointvec},
        {0}
};

ctl_table awc_root_table[] = {
        {254, "aironet4500", NULL, 0, 0555, awc_driver_proc_table},
        {0}
};

struct ctl_table_header * awc_driver_sysctl_header;

const char awc_procname[]= "awc5";


int awc_proc_set_device(int device_number){
  int group =0;
  int rid = 0;
  struct awc_private * priv;
  ctl_table * tmp_table_ptr;
 
  AWC_ENTRY_EXIT_DEBUG("awc_proc_set_device");  
  if (!aironet4500_devices[device_number] || (awc_nof_rids <=0 )) return -1 ;
  priv = (struct awc_private * )aironet4500_devices[device_number]->priv;

  awc_rids_setup(aironet4500_devices[device_number]);

  memcpy(&(awc_proc_priv[device_number].proc_table_sys_root[0]), awc_exroot_table,sizeof(struct ctl_table)*2);
  awc_proc_priv[device_number].proc_table_sys_root[0].ctl_name = 254 - device_number;
  memcpy(awc_proc_priv[device_number].proc_table_device_root, awc_exdev_table,sizeof(awc_exdev_table) );
  awc_proc_priv[device_number].proc_table_device_root[0].ctl_name = device_number+1;

  awc_proc_priv[device_number].proc_table_sys_root->child = awc_proc_priv[device_number].proc_table_device_root;
  memcpy(awc_proc_priv[device_number].proc_name,(struct NET_DEVICE * )aironet4500_devices[device_number]->name,5);
  awc_proc_priv[device_number].proc_name[4]=0;
 // awc_proc_priv[device_number].proc_name[3]=48+device_number;
  awc_proc_priv[device_number].proc_table_device_root[0].procname = &(awc_proc_priv[device_number].proc_name[0]);
  awc_proc_priv[device_number].proc_table = kmalloc(sizeof(struct ctl_table) * (awc_nof_rids+2),GFP_KERNEL);
  if (!awc_proc_priv[device_number].proc_table){
   printk(KERN_CRIT "Out of memory on aironet4500_proc huge table alloc \n");
   return -1;
  }
  awc_proc_priv[device_number].proc_table_device_root[0].child=awc_proc_priv[device_number].proc_table;
  

 if (awc_debug) printk("device  %d of %d proc interface setup ",device_number, awc_nof_rids);


  while (awc_rids[group].selector && group < awc_nof_rids){
     	if (awc_debug & 0x20000)
     		printk(KERN_CRIT "ridgroup %s  size %d \n", awc_rids[group].selector->name,awc_rids[group].size);

  	awc_proc_priv[device_number].proc_table[group].ctl_name = group +1;
  	awc_proc_priv[device_number].proc_table[group+1].ctl_name = 0;
  	awc_proc_priv[device_number].proc_table[group].procname = awc_rids[group].selector->name;
  	awc_proc_priv[device_number].proc_table[group].data	= awc_proc_buff;
  	awc_proc_priv[device_number].proc_table[group].maxlen  = sizeof(awc_proc_buff) -1;
  	awc_proc_priv[device_number].proc_table[group].mode	= 0600;
  	awc_proc_priv[device_number].proc_table[group].child	= kmalloc(sizeof(struct ctl_table) * (awc_rids[group].size +2), GFP_KERNEL);
  	awc_proc_priv[device_number].proc_table[group].proc_handler = NULL;
  	awc_proc_priv[device_number].proc_table[group].strategy = NULL;
  	awc_proc_priv[device_number].proc_table[group].de	= NULL;
  	awc_proc_priv[device_number].proc_table[group].extra1	= NULL;
  	awc_proc_priv[device_number].proc_table[group].extra2	= NULL;
  	if (!awc_proc_priv[device_number].proc_table[group].child) {
  		awc_proc_priv[device_number].proc_table[group].ctl_name = 0;
   		printk(KERN_CRIT "Out of memory on aironet4500_proc huge table alloc \n");
  		return 0;
  	}
  	rid=0;
  	while (awc_rids[group].rids[rid].selector && (rid < awc_rids[group].size -1)){

//  	   	DEBUG(0x20000,"rid %s  \n", awc_rids[group].rids[rid].name);

	  	awc_proc_priv[device_number].proc_table[group].child[rid].ctl_name 	= rid +1;
	  	awc_proc_priv[device_number].proc_table[group].child[rid+1].ctl_name 	= 0;
	  	awc_proc_priv[device_number].proc_table[group].child[rid].procname 	= awc_rids[group].rids[rid].name;
	  	if (awc_rids[group].rids[rid].array > 1 ||
	  	    awc_rids[group].rids[rid].bits  > 32 ){
	  		awc_proc_priv[device_number].proc_table[group].child[rid].data		= awc_proc_buff;
	  		awc_proc_priv[device_number].proc_table[group].child[rid].maxlen  	= sizeof(awc_proc_buff) -1;		
	  	} else {
	  	 	awc_proc_priv[device_number].proc_table[group].child[rid].data		= &awc_int_buff;
	  		awc_proc_priv[device_number].proc_table[group].child[rid].maxlen  	= sizeof(awc_int_buff);
	  
	  	}
	  		if ( awc_rids[group].rids[rid].read_only ||
	  	     awc_rids[group].rids[rid].selector->read_only )
	  		awc_proc_priv[device_number].proc_table[group].child[rid].mode		= 0400;
	  	else
	  		awc_proc_priv[device_number].proc_table[group].child[rid].mode          = 0600;
	  	awc_proc_priv[device_number].proc_table[group].child[rid].child		= NULL;
	  	awc_proc_priv[device_number].proc_table[group].child[rid].proc_handler 	= awc_proc_fun;
	  	awc_proc_priv[device_number].proc_table[group].child[rid].strategy 	= NULL;
	  	awc_proc_priv[device_number].proc_table[group].child[rid].de		= NULL;
	  	awc_proc_priv[device_number].proc_table[group].child[rid].extra1	= (void *) &(((struct awc_private* )aironet4500_devices[device_number]->priv)->rid_dir[group]);
	  	awc_proc_priv[device_number].proc_table[group].child[rid].extra2	= (void *) &(awc_rids[group].rids[rid]);

  		rid++;	
  	}
  	
  	group++;

  };
// here are driver-level params dir  
  	awc_proc_priv[device_number].proc_table[group].ctl_name = group +1;
  	awc_proc_priv[device_number].proc_table[group+1].ctl_name = 0;
  	awc_proc_priv[device_number].proc_table[group].procname = "driver-level";
  	awc_proc_priv[device_number].proc_table[group].data	= awc_proc_buff;
  	awc_proc_priv[device_number].proc_table[group].maxlen  = sizeof(awc_proc_buff) -1;
  	awc_proc_priv[device_number].proc_table[group].mode	= 0600;
  	awc_proc_priv[device_number].proc_table[group].child	= kmalloc(sizeof(awc_driver_level_ctable) , GFP_KERNEL);
  	awc_proc_priv[device_number].proc_table[group].proc_handler = NULL;
  	awc_proc_priv[device_number].proc_table[group].strategy = NULL;
  	awc_proc_priv[device_number].proc_table[group].de	= NULL;
  	awc_proc_priv[device_number].proc_table[group].extra1	= NULL;
  	awc_proc_priv[device_number].proc_table[group].extra2	= NULL;
  	if (!awc_proc_priv[device_number].proc_table[group].child) {
  		awc_proc_priv[device_number].proc_table[group].ctl_name = 0;
   		printk(KERN_CRIT "Out of memory on aironet4500_proc huge table alloc \n");
  		return 0;
  	}

	
	tmp_table_ptr = awc_proc_priv[device_number].proc_table[group].child;
	memcpy(tmp_table_ptr,awc_driver_level_ctable,sizeof(awc_driver_level_ctable));


        tmp_table_ptr[0].data = 
         &(priv->force_rts_on_shorter);
        tmp_table_ptr[1].data =   &priv->force_tx_rate;
        tmp_table_ptr[2].data = (void *) &priv->ip_tos_reliability_rts;
        tmp_table_ptr[3].data = (void *) &priv->ip_tos_troughput_no_retries;
        tmp_table_ptr[4].data = (void *) &priv->debug;
        tmp_table_ptr[5].data = (void *) &priv->simple_bridge;
        tmp_table_ptr[6].data = (void *) &priv->p802_11_send;
        tmp_table_ptr[7].data = (void *) &priv->full_stats;


	awc_proc_priv[device_number].sysctl_header = 
		register_sysctl_table(awc_proc_priv[device_number].proc_table_sys_root,0);
 
	AWC_ENTRY_EXIT_DEBUG("exit");

	if (awc_proc_priv[device_number].sysctl_header)
		return 0;
	return 1;  

};

int awc_proc_unset_device(int device_number){
  int k;

 AWC_ENTRY_EXIT_DEBUG("awc_proc_unset_device");
  if (awc_proc_priv[device_number].sysctl_header){
  	unregister_sysctl_table(awc_proc_priv[device_number].sysctl_header);
	awc_proc_priv[device_number].sysctl_header = NULL;
  }
  if (awc_proc_priv[device_number].proc_table){
	  for (k=0; awc_proc_priv[device_number].proc_table[k].ctl_name ; k++ ){
	  	if (awc_proc_priv[device_number].proc_table[k].child)
	  		kfree(awc_proc_priv[device_number].proc_table[k].child);
	  }
	  kfree(awc_proc_priv[device_number].proc_table);
	  awc_proc_priv[device_number].proc_table = NULL;
  }
  if (awc_proc_priv[device_number].proc_table_device_root[0].ctl_name)
          awc_proc_priv[device_number].proc_table_device_root[0].ctl_name = 0;
  if (awc_proc_priv[device_number].proc_table_sys_root[0].ctl_name)
          awc_proc_priv[device_number].proc_table_sys_root[0].ctl_name = 0;
  
	AWC_ENTRY_EXIT_DEBUG("exit");
   return 0;
};

static int aironet_proc_init(void) {
	int i=0;

	AWC_ENTRY_EXIT_DEBUG("init_module");


	for (i=0; i < MAX_AWCS;  i++){
		awc_proc_set_device(i);
	}

	awc_register_proc(awc_proc_set_device, awc_proc_unset_device);

	awc_driver_sysctl_header = register_sysctl_table(awc_root_table,0);

	AWC_ENTRY_EXIT_DEBUG("exit");
	return 0;

};

static void aironet_proc_exit(void){

	int i=0;
	AWC_ENTRY_EXIT_DEBUG("cleanup_module");
	awc_unregister_proc();
	for (i=0; i < MAX_AWCS;  i++){
		awc_proc_unset_device(i);
	}
	if (awc_driver_sysctl_header)
		unregister_sysctl_table(awc_driver_sysctl_header);
	AWC_ENTRY_EXIT_DEBUG("exit");
};

module_init(aironet_proc_init);
module_exit(aironet_proc_exit);

#endif // whole proc system styff
MODULE_LICENSE("GPL");
