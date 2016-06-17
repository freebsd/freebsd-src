/*
 *	 Aironet 4500/4800 driver core
 *
 *		Elmer Joandi, Januar 1999
 *		Copyright: 	GPL
 *	
 *
 *	Revision 0.1 ,started  30.12.1998
 *
 *
 */
 /* CHANGELOG:
 	march 99, stable version 2.0
 	august 99, stable version 2.2
 	november 99, integration with 2.3
	17.12.99: finally, got SMP near-correct. 
		timing issues remain- on SMP box its 15% slower on tcp	
	10.03.00 looks like softnet take us back to normal on SMP
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/system.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include "aironet4500.h"
#include <linux/ip.h>


int bap_sleep = 10 ;
int bap_sleep_after_setup = 1;
int sleep_before_command  = 1;
int bap_sleep_before_write= 1;
int sleep_in_command	  = 1;
int both_bap_lock;		/* activated at awc_init in this */
int bap_setup_spinlock;		/* file if numcpu >1 */

EXPORT_SYMBOL(bap_sleep);
EXPORT_SYMBOL(bap_sleep_after_setup);
EXPORT_SYMBOL(sleep_before_command);
EXPORT_SYMBOL(bap_sleep_before_write);
EXPORT_SYMBOL(sleep_in_command);
EXPORT_SYMBOL(both_bap_lock);
EXPORT_SYMBOL(bap_setup_spinlock);

struct awc_strings awc_status_error_codes[]=awc_reply_error_strings;
struct awc_strings awc_command_names[]=awc_command_name_strings;
struct awc_strings awc_link_status_names[]=awc_link_status_strings;
struct awc_strings awc_rid_names[]=aironet4500_RID_Select_strings;
struct awc_strings awc_link_failure_reason_names[]=IEEE_802_11_LINK_STATUS_FAILURE_REASON_STRINGS;

const char *  awc_print_string( struct awc_strings* strings, int code){
	
	struct awc_strings * str = strings;
	int i = 0;
	while (str[i].string != NULL){
		if (str[i].par == (code & str[i].mask )){
			return str[i].string;
		};
		i++;
	};
	return "UNKNOWN";
};

int awc_dump_registers(struct net_device * dev){

#ifdef AWC_DEBUG
	int i;
#endif
	int status= inw(dev->base_addr +4*2);	
	int r1= inw(dev->base_addr +5*2);	
	int r2= inw(dev->base_addr +6*2);	
	int r3= inw(dev->base_addr +7*2);	

	printk(KERN_ERR "Command %s , result: %s, at memblk %x(RID %s) , offset %x \n",
		awc_print_string(awc_command_names,status), 
		awc_print_string(awc_status_error_codes,r1),
		r2, awc_print_string(awc_rid_names,r2),
		r3);

#ifdef AWC_DEBUG
	printk(KERN_ERR "%s aironet register dump ",dev->name );
  
	                        
	for (i=0; i < 32; i++){
		printk("%4x ", inw(dev->base_addr + i*2 ) );
		if ( (i+1)%8 == 0){
			printk("\n");
			printk(KERN_ERR "%02x",(i+1)*2);
		}
	};
	printk(KERN_ERR " \n");
#endif
	return 0;                       
};
          
/******************************		COMMAND 	******************/


inline 
int	awc_command_busy_clear_wait(struct net_device * dev){
//	long long jiff = jiffies;
        u16  active_interrupts;
        int  cnt= 0;
        
  	AWC_ENTRY_EXIT_DEBUG(" entry awc_command_busy_clear_wait ");
  		     
	while (awc_command_busy(dev->base_addr)){
		if (cnt > 1000 ){
			printk(KERN_ERR "awc command busy too long, clearing\n");
			awc_dump_registers(dev);
			awc_event_ack_ClrStckCmdBsy(dev->base_addr);
			break;
		};
		if (((struct awc_private*) dev->priv)->ejected)
			return -1;
		cnt++;
		udelay(10);
	}
	
	cnt = 0;
	while (awc_command_busy(dev->base_addr)){
		//if (jiffies - jiff > (HZ/3)){
		if (cnt > 30000 ){
			printk(KERN_CRIT "awc command busy WAY too long, clearing\n");
			awc_dump_registers(dev);
			awc_event_ack_ClrStckCmdBsy(dev->base_addr);
 			active_interrupts = awc_event_status(dev->base_addr);
			awc_event_ack(dev->base_addr, active_interrupts);
			                                
			AWC_ENTRY_EXIT_DEBUG("BAD exit\n ");
			return -1 ;
			
		};
		if (((struct awc_private*) dev->priv)->ejected)
			return -1;
		cnt++;
		udelay(10);
	}

	
	AWC_ENTRY_EXIT_DEBUG(" exit\n ");
	     
	return 0;
	  

};



inline unsigned short 
awc_issue_command_and_block(struct awc_command * cmd){

	int ticks;               
     long long jiff;
     u16	enabled_interrupts;
     int cnt = 0;
//     unsigned long flags;
     
     jiff = jiffies; 
          

  AWC_ENTRY_EXIT_DEBUG(" entry awc_issue_command_and_block ");
               
     AWC_LOCK_COMMAND_ISSUING(cmd->priv);

     if (awc_command_busy_clear_wait(cmd->dev)) 		goto final;

     if (cmd->priv->sleeping_bap) udelay(sleep_before_command);               	

     awc4500wout(cmd->port,cmd->command,cmd->par0,cmd->par1,cmd->par2);
//     awc_dump_registers(cmd->dev);


     if (cmd->priv->sleeping_bap) udelay(sleep_in_command);
     
     enabled_interrupts = awc_ints_enabled(cmd->dev->base_addr);
     awc_ints_enable(cmd->dev->base_addr, enabled_interrupts & ~0x10);
      if(cmd->priv->enabled_interrupts & 0x10)
      	cmd->priv->enabled_interrupts &= ~0x10;

	
     while ( awc_command_read(cmd->port) == cmd->command) {
       	  udelay(1);
          awc_command_write(cmd->port, cmd->command);
          //if ((jiffies - jiff) > 2){
	  if (cnt > 2000 ){
          	printk(" long wait with commmand reg busy in blocking command \n");
          	awc_dump_registers(cmd->dev);
         	goto final;
          };
          if (cmd->priv->ejected)
		goto final;
	  cnt++;
  	  udelay(10);

     };
     AWC_ENTRY_EXIT_DEBUG(" issued " ); 

     ticks = 0;
     while ( awc_event_status_Cmd(cmd->port) == 0) {
	  ticks++;
          if (ticks > 100000){
		printk(" long wait with commmand reg busy \n");
          	awc_dump_registers(cmd->dev);
          		goto final;
          };
	  if (ticks > 500){
	       DEBUG(1, " long wait after issue 10mks * %d ", ticks ); 
         	//printk(" long wait with command reg busy about ticks\n");
	  	// sti();
          }
          if (cmd->priv->ejected)
		goto final;
	  udelay(10);
     }            
     if (cmd->priv->sleeping_bap) udelay(sleep_in_command);     

     awc_read_response(cmd);
     AWC_ENTRY_EXIT_DEBUG(" resp read \n"); 

     if (awc_command_busy(cmd->port)) 
     	awc_event_ack_ClrStckCmdBsy(cmd->port);

     awc_event_ack_Cmd(cmd->port);
    if (cmd->priv->sleeping_bap) udelay(sleep_in_command);     
 
     if (cmd->status & 0xff00){
     	printk(KERN_ERR " bad response to command %s, parameter %x \n",awc_print_string(awc_command_names, cmd->command),cmd->par0);
     	awc_dump_registers(cmd->dev);
  	goto final;   	 
     }	

     AWC_UNLOCK_COMMAND_ISSUING(cmd->priv);
     AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
    udelay(1);
     return 0;
final: 
     AWC_UNLOCK_COMMAND_ISSUING(cmd->priv);
     AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
     return -1; ;
};


inline 
unsigned short 
awc_issue_command(struct awc_command * cmd){


//     long long jiff = jiffies;          
//     unsigned short enabled_ints;               
     int cnt = 0;
//	int i=0; 
 
     AWC_ENTRY_EXIT_DEBUG(" entry awc_issue_command");
     
     if (!cmd){
     	printk(KERN_CRIT "cmd == NULL in awc_issue_command\n");
     	return -1;
     
     }
     if (!cmd->dev){
     	printk(KERN_CRIT "cmd->dev == NULL in awc_issue_command\n");
     	return -1;
     
     }

     AWC_LOCK_COMMAND_ISSUING(cmd->priv);	

     if(awc_command_busy_clear_wait(cmd->dev))		goto final;               	

      if(!cmd->priv->enabled_interrupts & 0x10){
      	cmd->priv->enabled_interrupts |= 0x10;
     	awc_ints_enable(cmd->port, cmd->priv->enabled_interrupts );
      }

     cmd->priv->async_command_start = jiffies;
     cmd->priv->command_semaphore_on++;


     awc4500wout(cmd->port,cmd->command,cmd->par0,cmd->par1,cmd->par2);
     
     while ( awc_command_read(cmd->port) == cmd->command) {
       
          awc_command_write(cmd->port, cmd->command);
          //if ((jiffies - jiff) > 2){
          if (cnt > 2000) {  
		printk(" long wait with commmand reg busy in async command \n");
          	awc_dump_registers(cmd->dev);
         	goto final;
          };
          if (cmd->priv->ejected)
		goto final;
	   cnt++;
	  udelay(10);
     };
     
     cmd->priv->cmd = *cmd;
     
     
     AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
     return 0;
 final:
     AWC_UNLOCK_COMMAND_ISSUING(cmd->priv);
     AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	   return -1; ;

};

inline 
unsigned short 
awc_issue_command_no_ack(struct net_device * dev,
			u16 com, u16 par1, u16 par2, u16 par3){

     struct awc_private * priv = (struct awc_private *)dev->priv;
     int cnt = 0;
     long long jiff;
     jiff = jiffies;          

     AWC_ENTRY_EXIT_DEBUG(" entry awc_issue_command_no_ack ");
     
               
     AWC_LOCK_COMMAND_ISSUING(priv);	

     if (awc_command_busy_clear_wait(dev)) {
		printk("aironet4x00 no_ack command (reset) with stuck card \n");
     }

     awc4500wout(dev->base_addr,com, par1, par2,par3);

     udelay(10);     
     while ( awc_event_status_Cmd(dev->base_addr) == 0) {
          if (awc_command_read(dev->base_addr) == com) {                
               awc_command_write(dev->base_addr, com);
          }
          //if ((jiffies - jiff) > 2){
          if (cnt > 2000) {  
		printk(" long wait with commmand reg busy in noack command %d par %d %d %d\n",com,par1,par2,par3);
          	awc_dump_registers(dev);
          		goto final;
          };
          if (priv->ejected)
		goto final;
	  udelay(10);
	  cnt++;
     }            
     
     if (awc_command_busy(dev->base_addr)) 
     	awc_event_ack_ClrStckCmdBsy(dev->base_addr);

     AWC_UNLOCK_COMMAND_ISSUING(priv);
     AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 return 0;
final: 
     AWC_UNLOCK_COMMAND_ISSUING(priv);
     AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	   return -1; ;
};


/******************************** 	BAP	*************************/

// inline // too long for inline
int awc_bap_setup(struct awc_command * cmd) {

	int status;
	long long jiff;
	unsigned long flags;	
	int cleared = 0;
	int cycles = 0;
	
     AWC_ENTRY_EXIT_DEBUG(" entry awc_bap_setup ");
     	
     if ( cmd->priv->sleeping_bap)
	udelay(bap_sleep);
	
	if (cmd->priv->ejected)
		return -1;
          
     if (!cmd->bap || !(cmd->lock_state & (AWC_BAP_SEMALOCKED |AWC_BAP_LOCKED)))
     	DEBUG(1,"no bap or bap not locked cmd %d !!", cmd->command);

	if (bap_setup_spinlock)
		spin_lock_irqsave(&cmd->priv->bap_setup_spinlock,cmd->priv->bap_setup_spinlock_flags);	  
	  status = AWC_IN(cmd->bap->offset);
	  
	  if (status & ~0x2000 ){
	  	WAIT61x3;
	  	status = AWC_IN(cmd->bap->offset);
	  }

	  if (status & ~0x2000 ){
                WAIT61x3;
	        AWC_IN(cmd->dev->base_addr + 0x26);
                AWC_OUT(cmd->dev->base_addr + 0x26, 0);    
	  	WAIT61x3;
	  	udelay(60);
	  	#ifdef AWC_DEBUG
	  		printk("b");
	  	#endif
	  	status = AWC_IN(cmd->bap->offset);
	  }

	  
	  if (status & 0xC000){
	  	printk(KERN_ERR "bap entered with err or busy bit set %x \n",status);
		if (cmd->bap->lock != 1) 
			printk(KERN_ERR "bap lock bad same time %x\n",cmd->bap->lock);
	  	awc_dump_registers(cmd->dev);
	  	//	AWC_OUT(cmd->bap->offset, 0x800);
	  }

	  save_flags(flags);
	  cli();
	    
          AWC_OUT(cmd->bap->select, cmd->rid);
	  WAIT61x3;
          AWC_OUT(cmd->bap->offset, cmd->offset);
 
          restore_flags(flags);

	  WAIT61x3;          
          
          jiff = jiffies;

          while (1) {
              cycles++;
              status = AWC_IN(cmd->bap->offset);
              if ( cmd->priv->sleeping_bap)
              		udelay(bap_sleep);
              if (cmd->priv->ejected)
			goto ejected_unlock;
	      udelay(1);
	      if (cycles > 10000) {
			printk(KERN_CRIT "deadlock in bap\n");
			goto return_AWC_ERROR;
	      };
              status = AWC_IN(cmd->bap->offset);
              if (status & AWC_BAP_BUSY) {
                 if (cycles % 100 == 99 ) {
                      save_flags(flags);
                      cli();
                      if (!cleared){
                      	AWC_IN(cmd->dev->base_addr + 0x26);
                      	AWC_OUT(cmd->dev->base_addr + 0x26, 0);
                      	WAIT61x3;
                      	cleared = 1;
                      }  
                      AWC_OUT(cmd->bap->select, cmd->rid);
                      WAIT61x3;
                      AWC_OUT(cmd->bap->offset, cmd->offset);
                      restore_flags(flags);
                	#ifdef AWC_DEBUG
	  			printk("B");
	  		#endif      
                      
                      if ( cmd->priv->sleeping_bap)
         		udelay(bap_sleep);
         	      else udelay(30);
                      //restart_timeout();
                  }
                  if (jiffies - jiff > 1 ) {
                  	AWC_ENTRY_EXIT_DEBUG(" BAD BUSY  exit \n");
                  	awc_dump_registers(cmd->dev);
                  	goto return_AWC_ERROR;
                  }
                  continue;
              }
             if (status & AWC_BAP_DONE) {
                  WAIT61x3; WAIT61x3; WAIT61x3;
                  
                //  if ((status & 0xfff) != cmd->offset)
                //  	printk(KERN_ERR "awcPBD %x ",status);
                   AWC_ENTRY_EXIT_DEBUG(" exit \n");
                  if (cmd->priv->sleeping_bap)
                  	udelay(bap_sleep_after_setup); 
                   
                  // success
                  goto return_AWC_SUCCESS;
              }

              if (status & AWC_BAP_ERR) {
             	  AWC_ENTRY_EXIT_DEBUG(" BAD  exit \n");
                  // invalid rid or offset
                  printk(KERN_ERR "bap setup error bit set for rid %x offset %x \n",cmd->rid,cmd->offset);
                  awc_dump_registers(cmd->dev);
                  goto return_AWC_ERROR;
              }
              if ( cmd->priv->sleeping_bap)
         		udelay(bap_sleep);
              else udelay(1);
              // -- awc missed it, try again
         
              save_flags(flags);
              cli();
              AWC_OUT(cmd->bap->select, cmd->rid);
              WAIT61x3;
              AWC_OUT(cmd->bap->offset, cmd->offset);
              WAIT61x3;
              restore_flags(flags);
		
	      if (jiffies - jiff > HZ)
	      if (! (status &(AWC_BAP_ERR |AWC_BAP_DONE |AWC_BAP_BUSY))){
		printk("aironet4500: bap setup lock without any status bits set");
		awc_dump_registers(cmd->dev);
                goto return_AWC_ERROR;

	      };
         
          }

     AWC_ENTRY_EXIT_DEBUG(" WE MUST NOT BE HERE exit \n");

ejected_unlock:
     if (bap_setup_spinlock)
	spin_unlock_irqrestore(&cmd->priv->bap_setup_spinlock,cmd->priv->bap_setup_spinlock_flags);	  
     AWC_ENTRY_EXIT_DEBUG(" ejected_unlock_exit \n");	
     return -1;

return_AWC_ERROR:
     if (bap_setup_spinlock)
	spin_unlock_irqrestore(&cmd->priv->bap_setup_spinlock,cmd->priv->bap_setup_spinlock_flags);	  
     AWC_ENTRY_EXIT_DEBUG(" AWC_ERROR_exit \n");
     return AWC_ERROR;          

return_AWC_SUCCESS:
     if (bap_setup_spinlock)
	spin_unlock_irqrestore(&cmd->priv->bap_setup_spinlock,cmd->priv->bap_setup_spinlock_flags);	  
     AWC_ENTRY_EXIT_DEBUG(" exit \n");
     return AWC_SUCCESS;          
}


	// requires call to awc_bap_setup() first
inline 
int
awc_bap_read(struct awc_command * cmd) {
	register u16 len;
	register u16 * buff = (u16 *) cmd->buff;
	register u16 port= cmd->bap->data;


        AWC_ENTRY_EXIT_DEBUG(" entry awc_bap_read ");
     	if (!cmd->bap && !(cmd->lock_state & (AWC_BAP_SEMALOCKED |AWC_BAP_LOCKED)))
     		DEBUG(0,"no bap or bap not locked %d !!", cmd->command);
        cmd->len = (cmd->len + 1) & (~1);               // round up to even value
        len = cmd->len / 2;
	if (cmd->priv->ejected)
			return -1;


	if (cmd->priv->sleeping_bap)
		udelay(bap_sleep_before_write);
		
        if (!cmd->priv->sleeping_bap)
        	while ( len-- > 0) 
			*buff++ = AWC_IN(port);
	else 
		while ( len-- > 0){
	                *buff++ = AWC_IN(port);
	        }
	AWC_ENTRY_EXIT_DEBUG(" exit  \n");
  	if (cmd->priv->ejected)
			return -1;
        	
        return AWC_SUCCESS;
}

      // requires call to awc_bap_setup() first
inline 
int
awc_bap_write(struct awc_command * cmd){
          register u16 len;
          register u16 * buff = (u16 *) cmd->buff;
          register u16 port= cmd->bap->data;
          
           
      AWC_ENTRY_EXIT_DEBUG(" entry awc_bap_write ");
      if (!cmd->bap && !(cmd->lock_state & (AWC_BAP_SEMALOCKED |AWC_BAP_LOCKED)))
     		DEBUG(0,"no bap or bap not locked %d !!", cmd->command);
     
          cmd->len = (cmd->len + 1) & (~1);               // round up to even value
          len = cmd->len / 2;

	  if (cmd->priv->ejected)
			return -1;

	  if (cmd->priv->sleeping_bap)
		udelay(bap_sleep_before_write);


          if (!cmd->priv->sleeping_bap)
          	while (len-- > 0) 
              		AWC_OUT(port, *buff++);
          else
          	while ( len-- > 0){
          		AWC_OUT(port, *buff++);
          	}
	  if (cmd->priv->ejected)
			return -1;


      AWC_ENTRY_EXIT_DEBUG(" exit  \n");
              	
          return AWC_SUCCESS;
}




/***************************** 	RID READ/WRITE	********************/

const struct aironet4500_rid_selector  aironet4500_RID_Select_General_Config	=(const struct aironet4500_rid_selector){ 0xFF10, 1,0,0, "General Configuration" }; //        See notes General Configuration        Many configuration items.
const struct aironet4500_rid_selector  aironet4500_RID_Select_SSID_list		=(const struct aironet4500_rid_selector){ 0xFF11, 1,0,0, "Valid SSID list" }; //          See notes Valid SSID list              List of SSIDs which the station may associate to.
const struct aironet4500_rid_selector  aironet4500_RID_Select_AP_list		=(const struct aironet4500_rid_selector){ 0xFF12, 1,0,0, "Valid AP list" }; //          See notes Valid AP list                List of APs which the station may associate to.
const struct aironet4500_rid_selector  aironet4500_RID_Select_Driver_name	=(const struct aironet4500_rid_selector){ 0xFF13, 1,0,0, "Driver name" }; //          See notes Driver name                  The name and version of the driver (for debugging)
const struct aironet4500_rid_selector  aironet4500_RID_Select_Encapsulation	=(const struct aironet4500_rid_selector){ 0xFF14, 1,0,0, "Ethernet Protocol" }; //          See notes Ethernet Protocol            Rules for encapsulating ethernet payloads onto 802.11.
const struct aironet4500_rid_selector  aironet4500_RID_Select_WEP_volatile	=(const struct aironet4500_rid_selector){ 0xFF15, 1,0,0, "WEP key volatile" }; //          
const struct aironet4500_rid_selector  aironet4500_RID_Select_WEP_nonvolatile	=(const struct aironet4500_rid_selector){ 0xFF16, 1,0,0, "WEP key non-volatile" }; //
const struct aironet4500_rid_selector  aironet4500_RID_Select_Modulation	=(const struct aironet4500_rid_selector){ 0xFF17, 1,0,0, "Modulation" }; //
const struct aironet4500_rid_selector  aironet4500_RID_Select_Active_Config	=(const struct aironet4500_rid_selector){ 0xFF20, 0,1,1, "Actual Configuration" }; //          Read only      Actual Configuration    This has the same format as the General Configuration.
const struct aironet4500_rid_selector  aironet4500_RID_Select_Capabilities	=(const struct aironet4500_rid_selector){ 0xFF00, 0,1,0, "Capabilities" }; //          Read Only      Capabilities            PC4500 Information
const struct aironet4500_rid_selector  aironet4500_RID_Select_AP_Info		=(const struct aironet4500_rid_selector){ 0xFF01, 0,1,1, "AP Info" }; //          Read Only      AP Info                 Access Point Information
const struct aironet4500_rid_selector  aironet4500_RID_Select_Radio_Info	=(const struct aironet4500_rid_selector){ 0xFF02, 0,1,1, "Radio Info" }; //          Read Only      Radio Info              Radio Information -- note radio specific
const struct aironet4500_rid_selector  aironet4500_RID_Select_Status		=(const struct aironet4500_rid_selector){ 0xFF50, 0,1,1, "Status" }; //          Read Only      Status                  PC4500 Current Status Information
const struct aironet4500_rid_selector  aironet4500_RID_Select_16_stats		=(const struct aironet4500_rid_selector){ 0xFF60, 0,1,1, "Cumulative 16-bit Statistics" }; //          Read Only      16-bit Statistics       Cumulative 16-bit Statistics
const struct aironet4500_rid_selector  aironet4500_RID_Select_16_stats_delta	=(const struct aironet4500_rid_selector){ 0xFF61, 0,1,1, "Delta 16-bit Statistics" }; //          Read Only      16-bit Statistics       Delta 16-bit Statistics (since last clear)
const struct aironet4500_rid_selector  aironet4500_RID_Select_16_stats_clear	=(const struct aironet4500_rid_selector){ 0xFF62, 0,1,1, "Delta 16-bit Statistics and Clear" }; //          Read Only /    16-bit Statistics       Delta 16-bit Statistics and Clear
const struct aironet4500_rid_selector  aironet4500_RID_Select_32_stats      	=(const struct aironet4500_rid_selector){ 0xFF68, 0,1,1, "Cumulative 32-bit Statistics" }; //          Read Only      32-bit Statistics       Cumulative 32-bit Statistics
const struct aironet4500_rid_selector  aironet4500_RID_Select_32_stats_delta	=(const struct aironet4500_rid_selector){ 0xFF69, 0,1,1, "Delta 32-bit Statistics"  }; //          Read Only      32-bit Statistics       Delta 32-bit Statistics (since last clear)
const struct aironet4500_rid_selector  aironet4500_RID_Select_32_stats_clear	=(const struct aironet4500_rid_selector){ 0xFF6A, 0,1,1, "Delta 32-bit Statistics and Clear" }; //          Read Only /    32-bit Statistics       Delta 32-bit Statistics and Clear

EXPORT_SYMBOL(aironet4500_RID_Select_General_Config); 
EXPORT_SYMBOL(aironet4500_RID_Select_SSID_list); 
EXPORT_SYMBOL(aironet4500_RID_Select_AP_list); 
EXPORT_SYMBOL(aironet4500_RID_Select_Driver_name); 
EXPORT_SYMBOL(aironet4500_RID_Select_Encapsulation); 
EXPORT_SYMBOL(aironet4500_RID_Select_WEP_volatile); 
EXPORT_SYMBOL(aironet4500_RID_Select_WEP_nonvolatile); 
EXPORT_SYMBOL(aironet4500_RID_Select_Modulation); 
EXPORT_SYMBOL(aironet4500_RID_Select_Active_Config); 
EXPORT_SYMBOL(aironet4500_RID_Select_Capabilities); 
EXPORT_SYMBOL(aironet4500_RID_Select_AP_Info); 
EXPORT_SYMBOL(aironet4500_RID_Select_Radio_Info); 
EXPORT_SYMBOL(aironet4500_RID_Select_Status); 
EXPORT_SYMBOL(aironet4500_RID_Select_16_stats); 
EXPORT_SYMBOL(aironet4500_RID_Select_16_stats_delta); 
EXPORT_SYMBOL(aironet4500_RID_Select_16_stats_clear); 
EXPORT_SYMBOL(aironet4500_RID_Select_32_stats); 
EXPORT_SYMBOL(aironet4500_RID_Select_32_stats_delta); 
EXPORT_SYMBOL(aironet4500_RID_Select_32_stats_clear); 


struct awc_rid_dir awc_rids_temp[]={
	// following MUST be consistent with awc_rids_setup !!!
   {&aironet4500_RID_Select_General_Config,		0x100 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_SSID_list, 			 0x68 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_AP_list, 			 0x20 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_Driver_name, 		 0x12 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_Encapsulation, 		 0x22 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_Active_Config, 		0x100 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_Capabilities, 		 0x80 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_Status, 			 0x6c , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_AP_Info, 			 0x06 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_32_stats, 			0x184 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_32_stats_delta, 		0x184 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_32_stats_clear, 		0x184 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_WEP_volatile,  		0x1c , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_WEP_nonvolatile,		0x1c , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_Modulation, 		0x04 , NULL, NULL, NULL,0 },

#ifdef AWC_USE_16BIT_STATS
   {&aironet4500_RID_Select_16_stats, 			0xC2 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_16_stats_delta,		0xC2 , NULL, NULL, NULL,0 },
   {&aironet4500_RID_Select_16_stats_clear, 		0xC2 , NULL, NULL, NULL,0 },
#else 
   {NULL},{NULL},{NULL},
#endif	
 
   {0} 


};



int 
awc_readrid(struct net_device * dev, struct aironet4500_RID * rid, void *pBuf ){
	  struct awc_command cmd;

	  int sleep_state ;

       AWC_ENTRY_EXIT_DEBUG(" entry awc_readrid ");
          if (!rid) return -1;
          if (!rid->selector) return -1;
          AWC_INIT_COMMAND(AWC_NOT_CLI,cmd,dev,0x21, rid->selector->selector,
          	rid->selector->selector, rid->offset, (rid->bits / 8),pBuf);

	  sleep_state = cmd.priv->sleeping_bap ;
	  cmd.priv->sleeping_bap = 1;
	  udelay(500);
	  AWC_BAP_LOCK_NOT_CLI(cmd);
	  if (awc_issue_command_and_block(&cmd))	goto final;
	  udelay(1);
          if (awc_bap_setup(&cmd))			goto final;
          udelay(1);
          if (awc_bap_read(&cmd))			goto final;
          cmd.priv->sleeping_bap = sleep_state;

	  AWC_RELEASE_COMMAND(cmd);
          AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	  return 0;
     final:
     	  cmd.priv->sleeping_bap = sleep_state;
     	  AWC_RELEASE_COMMAND(cmd);
     	  AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	   return -1; ;
}

int 
awc_writerid(struct net_device * dev, struct aironet4500_RID * rid, void *pBuf){

	  struct awc_command cmd;
	  int sleep_state ;

     AWC_ENTRY_EXIT_DEBUG(" entry awc_writerid ");
     

          AWC_INIT_COMMAND(AWC_NOT_CLI,cmd,dev,0x21, rid->selector->selector,
          	rid->selector->selector,rid->offset, rid->bits/8,pBuf);

	  sleep_state = cmd.priv->sleeping_bap ;
	  cmd.priv->sleeping_bap = 1;

	  udelay(500);
	  AWC_BAP_LOCK_NOT_CLI(cmd);	  
	  if (awc_issue_command_and_block(&cmd))	goto final;
	  udelay(10);
          if (awc_bap_setup(&cmd))			goto final;
          udelay(10);
          if (awc_bap_write(&cmd))			goto final;
          udelay(10);	
          cmd.command=0x121;
	  if (awc_issue_command_and_block(&cmd))	goto final;
          cmd.priv->sleeping_bap = sleep_state;

	  AWC_RELEASE_COMMAND(cmd);
          AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	  return 0;
     final:
     	  cmd.priv->sleeping_bap = sleep_state;
     	  AWC_RELEASE_COMMAND(cmd);
     	  AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	   return -1; ;
}

int 
awc_readrid_dir(struct net_device * dev, struct awc_rid_dir * rid ){
	  struct awc_command cmd;
	  int sleep_state;

     AWC_ENTRY_EXIT_DEBUG(" entry awcreadrid_dir ");
     

          AWC_INIT_COMMAND(AWC_NOT_CLI,cmd,dev,0x21, rid->selector->selector,
          	rid->selector->selector,0, rid->bufflen,rid->buff);

	  sleep_state = cmd.priv->sleeping_bap ;
	  cmd.priv->sleeping_bap = 1;

	  udelay(500);

	  AWC_BAP_LOCK_NOT_CLI(cmd);
	  if (awc_issue_command_and_block(&cmd))	goto final;
	  
          if (awc_bap_setup(&cmd))			goto final;
          if (awc_bap_read(&cmd))			goto final;
          cmd.priv->sleeping_bap = sleep_state;

	  AWC_RELEASE_COMMAND(cmd);
          AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	  return 0;
     final:
     	  cmd.priv->sleeping_bap = sleep_state;
     	  AWC_RELEASE_COMMAND(cmd);
     	  AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	   return -1; ;
}

int 
awc_writerid_dir(struct net_device * dev, struct awc_rid_dir * rid){

	  struct awc_command cmd;
	  int sleep_state ;


     AWC_ENTRY_EXIT_DEBUG(" entry awc_writerid_dir ");
     


          AWC_INIT_COMMAND(AWC_NOT_CLI,cmd,dev,0x21, rid->selector->selector,
          	rid->selector->selector,0, rid->bufflen,((char *)rid->buff));

	  sleep_state = cmd.priv->sleeping_bap ;
	  cmd.priv->sleeping_bap = 1;

	  udelay(500);

	  AWC_BAP_LOCK_NOT_CLI(cmd);

	  if (awc_issue_command_and_block(&cmd))	goto final;
          if (awc_bap_setup(&cmd))			goto final;
          if (awc_bap_write(&cmd))			goto final;
          cmd.priv->sleeping_bap = sleep_state;
	            
          cmd.command=0x121;
          udelay(500);
	  if (awc_issue_command_and_block(&cmd))	goto final;

	  AWC_RELEASE_COMMAND(cmd);
          AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	  return 0;
     final:
     	  cmd.priv->sleeping_bap = sleep_state;
     	  AWC_RELEASE_COMMAND(cmd);
     	  AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	   return -1; ;
}

EXPORT_SYMBOL(awc_readrid);
EXPORT_SYMBOL(awc_writerid);
EXPORT_SYMBOL(awc_readrid_dir);
EXPORT_SYMBOL(awc_writerid_dir);

/*****************************		STARTUP		*******************/


inline
int
awc_issue_blocking_command(struct net_device * dev,u16 comm){

	  struct awc_command cmd;
//	  struct awc_private * priv = (struct awc_private *)dev->priv;

     AWC_ENTRY_EXIT_DEBUG(" entry awc_issue_blocking_command ");
     
          AWC_INIT_COMMAND(AWC_NOT_CLI,cmd,dev,comm,0, 0, 0, 0 ,0 );

          AWC_BAP_LOCK_NOT_CLI(cmd);

          if (awc_issue_command_and_block(&cmd))
          	goto final;

	  AWC_RELEASE_COMMAND(cmd);
          AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	  return 0;
     final:
     	  AWC_RELEASE_COMMAND(cmd);
     	  AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	  return -1; ;
	  
};

int 
awc_issue_soft_reset(struct net_device * dev){

	u16 status ;
//	int i= 0;

/*	outw(inw(dev->base_addr + 0x30), dev->base_addr + 0x32);
	udelay(10);
	outw(inw(dev->base_addr + 0x30), dev->base_addr + 0x34);

	for (i=0; i< 32; i++)
		outw(0,dev->base_addr + i*2);
	udelay(100);
	outw(0x6,dev->base_addr + 0x34);
	udelay(100);
	outw(0x6,dev->base_addr + 0x34);
	outw(0x6,dev->base_addr + 0x34);
                WAIT61x3;
	        AWC_IN(dev->base_addr + 0x26);
                AWC_OUT(dev->base_addr + 0x26, 0);    
	  	WAIT61x3;
	  	udelay(60);
	

	outw(0x4, dev->base_addr);
	udelay(1000);
        WAIT61x3;
        AWC_IN(dev->base_addr + 0x26);
        AWC_OUT(dev->base_addr + 0x26, 0);    
 	WAIT61x3;
	udelay(60);
*/

	status =  awc_issue_command_no_ack(dev, AWC_COMMAND_SOFT_RESET,0,0,0);
	
//	awc_command_busy_clear_wait(dev);

	return status;
};

int
awc_issue_noop(struct net_device * dev){
	int retval;
	AWC_OUT(dev->base_addr + 0x28, 0);
	AWC_OUT(dev->base_addr + 0x2A, 0);
	udelay(1000);
	retval= awc_issue_blocking_command(dev, AWC_COMMAND_NOOP);
	udelay(1000);
	return retval;
};

EXPORT_SYMBOL(awc_enable_MAC);

int
awc_enable_MAC(struct net_device * dev){
        
   struct awc_private * priv = (struct awc_private *)dev->priv;
     AWC_ENTRY_EXIT_DEBUG(" entry awc_enable_MAC ");
            
        if (priv->mac_enabled){
        
        	AWC_ENTRY_EXIT_DEBUG(" mac already enabled exit \n"); 
 		return 0;
        }
        udelay(500);
	if (awc_issue_blocking_command(dev, AWC_COMMAND_ENABLE)){
		AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	   return -1; ;
	}
        udelay(500);

	priv->mac_enabled = 1;
     
	AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	return 0;
};

EXPORT_SYMBOL(awc_disable_MAC);
int
awc_disable_MAC(struct net_device * dev){
        
   struct awc_private * priv = (struct awc_private *)dev->priv;
     AWC_ENTRY_EXIT_DEBUG(" entry awc_disable_MAC ");
            
        if (!priv->mac_enabled){
        	AWC_ENTRY_EXIT_DEBUG(" mac allready disabled exit \n"); 
 		return 0;
        }
        udelay(1000);
	if (awc_issue_blocking_command(dev, AWC_COMMAND_DISABLE)){
		AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
		return -1; ;
	}
	udelay(1000);
	priv->mac_enabled = 0;
        AWC_ENTRY_EXIT_DEBUG(" exit \n");
	return 0;
};



int
awc_read_all_rids(struct net_device * dev){

	struct awc_private * priv = (struct awc_private *)dev->priv;
	int status,i;
     AWC_ENTRY_EXIT_DEBUG(" entry awc_read_all_rids ");
                                   
  	for (i=0; i< AWC_NOF_RIDS && priv->rid_dir[i].selector  ; i++){
  		status = awc_readrid_dir(dev,&priv->rid_dir[i]);
  		udelay(50);
  		if (status) return status;
  		                
  	}
  	priv->rids_read = 1;
  	
     AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
     return 0;
}

int
awc_write_all_rids(struct net_device * dev){

	struct awc_private * priv = (struct awc_private *)dev->priv;
  	int i,status ;
     AWC_ENTRY_EXIT_DEBUG(" entry awc_write_all_rids ");
                                   
  	for (i=0;i < 5 &&  i< AWC_NOF_RIDS && priv->rid_dir[i].selector  ; i++){
  	     status = awc_writerid_dir(dev,&priv->rid_dir[i]);
  	     udelay(10);
  	     if(status) return status;
  	}
     AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
     return 0;
}

/**************************	FID QUEUES ****************************/
/****************************	TX  ALLOC / DEALLOC 	***************/



int  awc_tx_alloc(struct net_device * dev) {

	  struct awc_command cmd;
	  int k=0;
	  int tot=0;
	 struct awc_fid * fid = NULL;
	 
     AWC_ENTRY_EXIT_DEBUG(" entry awc_tx_alloc ");
     	  

          AWC_INIT_COMMAND(AWC_NOT_CLI,cmd,dev,0x0A,0, 0,0,0,NULL);
	  cmd.par0 = dev->mtu + AWC_TX_HEAD_SIZE + 8 ;

	  DEBUG(32,"about to allocate %x bytes ",cmd.priv->large_buff_mem);
	  DEBUG(32,"in %x large buffers ",cmd.priv->large_buff_mem / (dev->mtu + AWC_TX_HEAD_SIZE + 8) );
	  	
	  k=0;tot=0;
	  AWC_BAP_LOCK_NOT_CLI(cmd);

	  while (k < cmd.priv->large_buff_mem / (dev->mtu + AWC_TX_HEAD_SIZE + 8) ) {
	  	
	  	fid = kmalloc(sizeof(struct awc_fid),GFP_KERNEL );
	  	if (!fid)	goto final;
		memset(fid, 0, sizeof(struct awc_fid));

		if (awc_issue_command_and_block(&cmd))		goto final;
		
          	while ( awc_event_status_Alloc(cmd.port) == 0) ;
		fid->u.tx.fid 		= awc_Tx_Allocated_Fid(cmd.port);
		fid->u.tx.fid_size 	= dev->mtu + AWC_TX_HEAD_SIZE ;
		
		DEBUG(32,"allocated large tx fid %x ",fid->u.tx.fid);
		if(fid->u.tx.fid == 0
		   || cmd.status != 0xA){
			printk(KERN_ERR "%s bad tx_alloc\n",dev->name);
			fid->busy =1;
			goto final;
		} else {
			fid->busy =0;
			tot++;
		}
		awc_event_ack_Alloc(cmd.port);
		
		// shoudlnt goto final after that
		awc_fid_queue_push_tail(&cmd.priv->tx_large_ready,fid);
				
		k++;
	  }
	  cmd.priv->tx_buffs_total = tot;
	  DEBUG(32,"allocated %d large tx buffs\n",tot);

	  cmd.par0 = AWC_TX_ALLOC_SMALL_SIZE ;
	  k =0; tot = 0;

	  while (k < cmd.priv->small_buff_no) {
	  	
	  	fid = kmalloc(sizeof(struct awc_fid),GFP_KERNEL );
	  	if (!fid)	goto final;
		memset(fid, 0, sizeof(struct awc_fid));

	  	cmd.par0 = AWC_TX_ALLOC_SMALL_SIZE ;

		if (awc_issue_command_and_block(&cmd))		goto final;
		
          	while ( awc_event_status_Alloc(cmd.port) == 0) ;
		fid->u.tx.fid 		= awc_Tx_Allocated_Fid(cmd.port);
		fid->u.tx.fid_size 	= AWC_TX_ALLOC_SMALL_SIZE;
		
		DEBUG(32,"allocated large tx fid %x ",fid->u.tx.fid);
		if(fid->u.tx.fid == 0
		   || cmd.status != 0xA){
			printk(KERN_ERR "%s bad tx_alloc\n",dev->name);
			fid->busy =1;
			goto final;
		} else {
			fid->busy =0;
			tot++;
		}
		awc_event_ack_Alloc(cmd.port);
		
		// shoudlnt goto final after that
		awc_fid_queue_push_tail(&cmd.priv->tx_small_ready,fid);
				
		k++;
	  }

	  cmd.priv->tx_small_buffs_total = tot;
	  DEBUG(32,"allocated %d small tx buffs\n",tot);

	  AWC_RELEASE_COMMAND(cmd);
          AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	  return 0;

     final:
     	  if (fid ) 
     	  	kfree(fid);
     	  printk(KERN_CRIT "%s awc tx prealloc failed \n",dev->name);
     	  AWC_RELEASE_COMMAND(cmd);
     	  AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	  return -1; ;

};

int 
awc_tx_dealloc_fid(struct net_device * dev,struct awc_fid * fid){

	  struct awc_command cmd;
	  int fid_handle = 0;
	  
          AWC_INIT_COMMAND(AWC_NOT_CLI,cmd,dev,0x0C,0, 0,0,0,NULL);

	  AWC_BAP_LOCK_NOT_CLI(cmd);

	  if (fid->u.tx.fid){
	  		fid_handle = cmd.par0 = fid->u.tx.fid;
	  		fid->u.tx.fid = 0;
			fid->busy =0;
	  		kfree(fid);

			if (!cmd.priv->ejected)
				if (awc_issue_command_and_block(&cmd))	goto final;
						//awc_event_ack_Alloc(cmd.port);
	  }

	  AWC_RELEASE_COMMAND(cmd);
          AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	  return 0;

     	  final:
     	  	printk(KERN_ERR "awc_tx_dealloc failed for fid %x \n",fid_handle);
     	  	AWC_RELEASE_COMMAND(cmd);
     	  	AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	  return -1; ;


};

int
awc_tx_dealloc(struct net_device * dev){

 	struct awc_private * priv = (struct awc_private *)dev->priv;



//	  int k=0;
	  struct awc_fid * fid;
	  
          AWC_ENTRY_EXIT_DEBUG(" entry awc_tx_dealloc ");
     
	  while (NULL != (fid = awc_fid_queue_pop_head(&priv->tx_large_ready)))
		awc_tx_dealloc_fid(dev,fid);
	  while (NULL != (fid = awc_fid_queue_pop_head(&priv->tx_small_ready)))
		awc_tx_dealloc_fid(dev,fid);
	  while (NULL != (fid = awc_fid_queue_pop_head(&priv->tx_post_process)))
		awc_tx_dealloc_fid(dev,fid);
	  while (NULL != (fid = awc_fid_queue_pop_head(&priv->tx_in_transmit)))
		awc_tx_dealloc_fid(dev,fid);
	
	  return 0;

};



inline struct awc_fid *
awc_tx_fid_lookup_and_remove(struct net_device * dev, u16 fid_handle){

 	struct awc_private * priv = (struct awc_private *)dev->priv;
//	int k = 0;
	unsigned long flags;
	struct awc_fid * fid = NULL;
	int cnt=0;
	
     AWC_ENTRY_EXIT_DEBUG(" entry awc_tx_fid_lookup ");

	spin_lock_irqsave(&(priv->queues_lock),flags);


	fid = priv->tx_in_transmit.head;
	cnt = 0;
	while (fid){
	  	if (fid->u.tx.fid == fid_handle){
	  		awc_fid_queue_remove(&priv->tx_in_transmit, fid);
	  		spin_unlock_irqrestore(&(priv->queues_lock),flags);
	  		return fid;
	  	}
	  	fid = fid->next;
	//	printk("iT\n");
		if (cnt++ > 200) {
	//		printk("bbb in awc_fid_queue\n");
			spin_unlock_irqrestore(&(priv->queues_lock),flags);
	  		return 0;
		};
	};

	cnt=0;
	fid = priv->tx_post_process.head;
	while (fid){
	  	if (fid->u.tx.fid == fid_handle){
	  		awc_fid_queue_remove(&priv->tx_post_process, fid);
	  		spin_unlock_irqrestore(&(priv->queues_lock),flags);
	  		return fid;
	  	}
	  	fid = fid->next;
	//	printk("pp\n");
		if (cnt++ > 200) {
	//		printk("bbb in awc_fid_queue\n");
			spin_unlock_irqrestore(&(priv->queues_lock),flags);
	  		return 0;
		};

	};

	cnt=0;
	fid = priv->tx_large_ready.head;
	while (fid){
	  	if (fid->u.tx.fid == fid_handle){
	  		awc_fid_queue_remove(&priv->tx_large_ready, fid);
	  		spin_unlock_irqrestore(&(priv->queues_lock),flags);
	  		return fid;
	  	}
	  	fid = fid->next;
	//	printk("lr\n");
		if (cnt++ > 200) {
	//		printk("bbb in awc_fid_queue\n");
			spin_unlock_irqrestore(&(priv->queues_lock),flags);
	  		return 0;
		};

	};
	cnt=0;
	fid = priv->tx_small_ready.head;
	while (fid){
	  	if (fid->u.tx.fid == fid_handle){
	  		awc_fid_queue_remove(&priv->tx_small_ready, fid);
	  		spin_unlock_irqrestore(&(priv->queues_lock),flags);
	  		return fid;
	  	}
	  	fid = fid->next;
	//	printk("sr\n");
		if (cnt++ > 200) {
	//		printk("bbb in awc_fid_queue\n");
			spin_unlock_irqrestore(&(priv->queues_lock),flags);
	  		return 0;
		};

	};

	spin_unlock_irqrestore(&(priv->queues_lock),flags);
	
	printk(KERN_ERR "%s tx fid %x not found \n",dev->name, fid_handle);  
        AWC_ENTRY_EXIT_DEBUG(" BAD exit \n");   	  
	return NULL;
}





int 
awc_queues_init(struct net_device * dev){
 	struct awc_private * priv = (struct awc_private *)dev->priv;
	struct awc_fid * fid = NULL;
	int retv =0;
	int k = 0;

	awc_fid_queue_init(&priv->tx_in_transmit);
	awc_fid_queue_init(&priv->tx_post_process);
	awc_fid_queue_init(&priv->tx_large_ready);
	awc_fid_queue_init(&priv->tx_small_ready);
	awc_fid_queue_init(&priv->rx_ready);
	awc_fid_queue_init(&priv->rx_post_process);

	retv = awc_tx_alloc(dev);

	k = 0;
	while (k < AWC_RX_BUFFS){
		fid = kmalloc(sizeof(struct awc_fid),GFP_KERNEL);
		if (!fid) return -1;
		awc_fid_queue_push_tail(&priv->rx_ready,fid);
		k++;
	};

	if (retv) return retv;

	return 0;	
};


int 
awc_queues_destroy(struct net_device * dev){
 	struct awc_private * priv = (struct awc_private *)dev->priv;
	struct awc_fid * fid = NULL;
	int retv =0;
	


	while (NULL != (fid = awc_fid_queue_pop_head(&priv->rx_ready))){
		kfree(fid);
	}
	while (NULL != (fid = awc_fid_queue_pop_head(&priv->rx_post_process))){
		kfree(fid);
	}

	retv = awc_tx_dealloc(dev);

	return retv;	
};



/****************************** 	802.11router	******************/
inline int 
awc_802_11_copy_path_skb(struct net_device * dev, struct awc_fid * rx_buff){

	struct awc_private * priv = (struct awc_private * )dev->priv;

	AWC_ENTRY_EXIT_DEBUG("awc_802_11_copy_path_skb");

        if (rx_buff->pkt_len < 22 ) rx_buff->pkt_len = 22;
        
//	if (!rx_buff->skb)
		rx_buff->skb =  dev_alloc_skb(rx_buff->pkt_len + 12 +2);
	
	
	if (rx_buff->skb == NULL) {
		printk(KERN_CRIT "couldnt alloc rx_buff->skb in rx event \n");
		priv->stats.rx_dropped++;
		return -1;
	}
	rx_buff->type |= p80211copy_path_skb;
	
	rx_buff->skb->dev = dev;

//	skb_reserve(rx_buff->skb, rx_buff->pkt_len + 12 );

	rx_buff->u.rx.payload = skb_put(rx_buff->skb, rx_buff->pkt_len + 12 ) ;
	rx_buff->u.rx.payload  = ((char *)rx_buff->u.rx.payload ) +12;

	AWC_ENTRY_EXIT_DEBUG("exit\n");

	return 0;


};


int
awc_802_11_find_copy_path(struct net_device * dev, struct awc_fid * rx_buff){

//        struct awc_private * priv = (struct awc_private * )dev->priv;
//        u8 is_802_3 = 0;
//	int i = 0;

	rx_buff->type =0;

	return awc_802_11_copy_path_skb(dev,rx_buff);
};


/* 	called from INTERRUPT context,

	must deliver the packet to where it was meant by 
		awc_802_11_find_copy_path
	
	SHOULD be efficient and
	queue the packet if operations take longer

*/ 


int parse_not_8023;
       
void
awc_802_11_router_rx(struct net_device * dev,struct awc_fid * rx_buff){

        struct awc_private * priv = (struct awc_private * )dev->priv;
	struct sk_buff * skb = rx_buff->skb;
	u8 * payload = rx_buff->u.rx.payload;
//	u8 * p802_3_macs_place = payload -12;
	u16    pkt_len = rx_buff->pkt_len;	
	struct ieee_802_11_802_1H_header * bridge = NULL;
	struct ieee_802_11_snap_header * snap = NULL;
	struct ieee_802_11_802_1H_header * bridge_tmp;
	struct ieee_802_11_snap_header * snap_tmp;

	u16	ptr = 0;
	u16	len;

	AWC_ENTRY_EXIT_DEBUG("awc_802_11_router_rx");

//	if (rx_buff->type & p80211_8023)
		rx_buff->mac = rx_buff->u.rx.ieee_802_3.dst_mac;
//	else	
//		rx_buff->mac = rx_buff->u.rx.ieee_802_11.mac1;	

	if ( rx_buff->u.rx.ieee_802_11.frame_control == 0x8 )	
		memcpy(priv->bssid,rx_buff->u.rx.ieee_802_11.mac3,6);
	
	while ((ptr < pkt_len - 1 ) && payload && parse_not_8023){

		bridge_tmp 	= (struct ieee_802_11_802_1H_header*) &payload[ptr];
		snap_tmp 	= (struct ieee_802_11_snap_header*) &payload[ptr];
		len		= ntohs( *((u16*)&payload[ptr]) );

		

		if (  len < 0x5DC)	{ // not a protocol
			
			if ( len != pkt_len-2 - ptr){
				printk(KERN_ERR "%s bad encapsulation lenght %x at pkt offset %x \n",dev->name,len,ptr);
				goto bad_packet;
			}
			DEBUG(1,"parisng packet of size %x\n",len);
			ptr +=2;
			continue;
		}	
		
		DEBUG(1,"parisng packet of proto %x\n",len);
		
		if (snap_tmp->dsap == 0xaa &&  snap_tmp->ssap == 0xaa &&
		    pkt_len - ptr > sizeof(struct ieee_802_11_snap_header) ){
			
			DEBUG(0x200,"%s SNAP ",dev->name);
			if (snap_tmp->ctrl != 0x03){
				printk(KERN_ERR "%s unknown snap ctrl %x \n",dev->name,snap_tmp->ctrl);
				goto bad_packet;
			};
			if (snap_tmp->oui[0] == 0 && // LLC RFC1042
			    snap_tmp->oui[1] == 0 &&
			    snap_tmp->oui[2] == 0 ){
			    	snap = 	snap_tmp;
			    	ptr +=	sizeof(struct ieee_802_11_snap_header);
			    	DEBUG(0x200,"%s LLC RFC1042 \n",dev->name);
			    	continue;
			}
			if (snap_tmp->oui[0] == 0 && // LLC 802.1H
			    snap_tmp->oui[1] == 0 &&
			    snap_tmp->oui[2] == 0x78){
			    	snap = snap_tmp;
			    	DEBUG(0x200,"%s LLC 802.1H \n",dev->name);
			    	ptr +=	sizeof(struct ieee_802_11_snap_header);
			    	continue;  
			};
			if (snap_tmp->oui[0] == 0x00 && // 802.1H itself
			    snap_tmp->oui[1] == 0x40 &&
			    snap_tmp->oui[2] == 0x96){
			    	ptr +=	sizeof(struct ieee_802_11_802_1H_header);
			    	if (ptr >= pkt_len){
			    		goto bad_packet;
			    		DEBUG(1,"%s invalid packet len in 802.1H SNAP OUI check \n",dev->name);
			    	}
			    	DEBUG(0x200,"%s OUI 004096  \n",dev->name);
			    	DEBUG(0x200," 802.1H uknown1 %x  ",ntohs(bridge_tmp->unknown1));
			    	DEBUG(0x200," 802.1H uknw type %x  \n",0xf000 & ntohs(bridge_tmp->unknown2));
			    	DEBUG(0x200," 802.1H payloadsize %x  \n",0x0fff & ntohs(bridge_tmp->unknown2));
			    	
			    	//goto bad_packet; // TODO
			    	
			    	bridge = bridge_tmp;
			    	if (bridge_tmp->unknown1 == 0x0000 &&
			    	     ((ntohs(bridge_tmp->unknown2) & 0xf000) == 0x1000 ) ){
			    	     rx_buff->type |= p80211_8021H;
			    	     rx_buff->mac   = &payload[ptr];
			    	     DEBUG(0x200," 802.1H DATA packet of size %x\n",0xf000 & ntohs(bridge_tmp->unknown2) );
			    	     memcpy(priv->p2p,rx_buff->u.rx.ieee_802_11.mac2, 6);
			    	     ptr +=12;
			    	     continue;
			    	};
			    	DEBUG(0x200,"%s droping unknown  004096 packet \n ",dev->name);
			    	goto bad_packet;
			    	
			 
			}
			goto bad_packet;
		}
		if ( len > 0x5DC){ 
			// packet without linklevel header for us
		
			if (  len == 0x8000 ||  len == 0x8006){
			
				DEBUG(0x200,"Non IP packet %x \n",ntohs(len));
			
			};
			goto good_packet;
		
		};
		
		goto good_packet;
	}
	
   good_packet:

	if (ptr > pkt_len)	goto bad_packet;

	if ( rx_buff->mac != (payload + ptr -12) )
		memcpy( payload +ptr -12, rx_buff->mac , 12);

	
	
	if (!payload || !skb || !rx_buff->skb || !rx_buff->u.rx.payload)
		return ;
	//skb->ip_summed = CHECKSUM_NONE;
	skb->data = payload + ptr -12;
	skb->len += ptr ;
	
	rx_buff->skb->protocol = eth_type_trans(rx_buff->skb,dev);
	DEBUG(0x200,"eth_type_trans decided: %x\n",rx_buff->skb->protocol);
	rx_buff->skb = NULL;
	rx_buff->u.rx.payload = NULL;
	priv->stats.rx_packets++;
	priv->stats.rx_bytes += skb->len;
	
	netif_rx(skb);
	dev->last_rx = jiffies;
	AWC_ENTRY_EXIT_DEBUG("exit\n");
	return ;

   bad_packet:
   	DEBUG(0x200,"%s packet dropped in packet hdr parse \n ",dev->name);
	if (rx_buff->skb && (rx_buff->type & p80211copy_path_skb)){

		dev_kfree_skb_irq(rx_buff->skb);		
		rx_buff->skb = NULL;
		rx_buff->u.rx.payload = NULL;
	};

	AWC_ENTRY_EXIT_DEBUG("exit\n");	

};

void
awc_802_11_failed_rx_copy(struct net_device * dev,struct awc_fid * rx_buff){
	struct awc_private * priv = (struct awc_private * )dev->priv;


	AWC_ENTRY_EXIT_DEBUG("awc_802_11_failed_rx_copy");
	if (rx_buff->skb)
                dev_kfree_skb_irq(rx_buff->skb);
        rx_buff->skb = NULL;
        rx_buff->u.rx.payload = NULL;
	priv->stats.rx_errors++;


	AWC_ENTRY_EXIT_DEBUG("exit\n");
};

/*
	called from kernel->driver tx routine
	must decide where and how to post the packet 
	must post the packet to wherever it decides
	either copy to card or enqueue to destination queue

*/


int
awc_802_11_tx_find_path_and_post(struct net_device * dev,
				 struct sk_buff * skb){


	struct awc_private * priv = (struct awc_private * )dev->priv;
	int i;
	int len = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN; /* check min length*/
	struct awc_fid * fid = NULL;
//	u16 saved_fid ;
	u16 p2p_direct =priv->p2p_found;
	struct iphdr * ip_hdr;
	//buffer = skb->data;

	AWC_ENTRY_EXIT_DEBUG("awc_802_11_tx_find_path_and_post");	

	// netif_stop_queue(dev);
	DOWN(&priv->tx_buff_semaphore);
	if (len  > dev->mtu + 16 ) {
		printk(KERN_ERR "%s packet size too large %d \n",dev->name, len);
		goto final;
	}

	if (len + AWC_TX_HEAD_SIZE < AWC_TX_ALLOC_SMALL_SIZE  )
		fid = awc_fid_queue_pop_head(&priv->tx_small_ready);

	if (!fid)
		fid = awc_fid_queue_pop_head(&priv->tx_large_ready);
			
	if (!fid) {
		DEBUG(32,"%s buffs in use \n",dev->name);
		goto no_space;
	}		
/*
	if (fid->u.tx.fid_size < len + AWC_TX_HEAD_SIZE){
		awc_fid_queue_push_tail(&priv->tx_small_ready, fid);
		fid = awc_fid_queue_pop_head(&priv->tx_large_ready);
	}
*/
	if (!fid) {
	       DEBUG(32,"%s buffs in use \n",dev->name);
	       goto no_space;
	}
	
	if (fid->u.tx.fid_size < len + AWC_TX_HEAD_SIZE - 14){	
		printk(KERN_ERR "found too small tx fid size %d, pktlen %d \n",fid->u.tx.fid_size, len);
	}
	memset(&fid->u.tx.radio_tx,		0,sizeof(struct aironet4500_radio_tx_header));
	memset(&fid->u.tx.ieee_802_11,	0,sizeof(struct ieee_802_11_header));
	memset(&fid->u.tx.ieee_802_3,	0,sizeof(struct ieee_802_3_header));
	fid->u.tx.payload =NULL;
	fid->u.tx.gap_length =0;
	fid->busy = 1;
	
	
	priv->tx_buffs_in_use++;
	DEBUG(32,"found large buff %x \n",fid->u.tx.fid);

/*
	fid->type |= p80211_llc_snap;
	fid->snap.dsap = 0xaa; 
	fid->snap.ssap = 0xaa; 
	fid->snap.ctrl = 0x03;	
	fid->snap.oui[0] = 0x0;
	fid->snap.oui[1] = 0x0;
	fid->snap.oui[2] = 0x0;
*/
	fid->skb = skb;


	if (priv->p2p_uc && !priv->p2p_found){ // we go without encapsulation to neighbour;
	
		for (i=0; i < 6; i++)
			if (priv->p2p[i] != skb->data[i]){
				p2p_direct = 1;
				break;
			}	
	};

	if (priv->force_tx_rate == 2 || priv->force_tx_rate == 4 || 
		priv->force_tx_rate== 11 || priv->force_tx_rate == 22){
			fid->u.tx.radio_tx.tx_bit_rate  = priv->force_tx_rate;
	} else if (priv->force_tx_rate != 0 ) {
		printk(KERN_ERR "wrong force_tx_rate=%d changed to default \n",	priv->force_tx_rate);
		priv->force_tx_rate = 0;
	};
	fid->u.tx.radio_tx.TX_Control = 
		aironet4500_tx_control_tx_ok_event_enable |
		aironet4500_tx_control_tx_fail_event_enable |
		aironet4500_tx_control_no_release;

	if (len < priv->force_rts_on_shorter){
		fid->u.tx.radio_tx.TX_Control |=
			aironet4500_tx_control_use_rts;
	};

	ip_hdr = (struct iphdr * ) ((( char * ) skb->data) + 14);
	if (ip_hdr && skb->data[12] == 0x80 ){
		if (ip_hdr->tos & IPTOS_RELIABILITY && priv->ip_tos_reliability_rts)
			fid->u.tx.radio_tx.TX_Control |=
			    aironet4500_tx_control_use_rts;
		if (ip_hdr->tos & IPTOS_THROUGHPUT && priv->ip_tos_troughput_no_retries)
			fid->u.tx.radio_tx.TX_Control |=
			    aironet4500_tx_control_no_retries;
	};

	if (priv->p802_11_send ||  memcmp(dev->dev_addr, skb->data +6, 6)  ){
		fid->u.tx.radio_tx.TX_Control |=
			aironet4500_tx_control_header_type_802_11;	
		DEBUG(0x200,"%s bridging, forcing 802_11 send \n ",dev->name);
	}


	if (!priv->p2p_uc || p2p_direct) {
		if ((fid->u.tx.radio_tx.TX_Control &
		                 aironet4500_tx_control_header_type_802_11 )){

			// including 802.3 header into 802.11 packet
			fid->u.tx.radio_tx.PayloadLength 	= len -12;
			fid->u.tx.ieee_802_3.payload_length = len -12 ;
			fid->pkt_len = len -12;
			fid->u.tx.payload = skb->data +12;

			if (priv->simple_bridge){	
				memcpy(fid->u.tx.ieee_802_11.mac1,skb->data,6);
				memcpy(fid->u.tx.ieee_802_11.mac2,skb->data +6,6);
				memcpy(fid->u.tx.ieee_802_11.mac3,priv->status.CurrentBssid ,6);
				memset(fid->u.tx.ieee_802_11.mac4,0,6);
				fid->u.tx.ieee_802_11.frame_control = 0x8;
				fid->u.tx.ieee_802_11.gapLen=6;
			} else {
				memcpy(fid->u.tx.ieee_802_11.mac1,skb->data,6);
				memcpy(fid->u.tx.ieee_802_11.mac2,dev->dev_addr,6);
				memcpy(fid->u.tx.ieee_802_11.mac3,skb->data +6 ,6);
				memset(fid->u.tx.ieee_802_11.mac4,0 ,6);
				fid->u.tx.ieee_802_11.frame_control = 0x108;
				fid->u.tx.ieee_802_11.gapLen=6;                 
			}
		} else { // plain old 802.3, with hdr copied
			fid->u.tx.radio_tx.PayloadLength 	= len -12;
			fid->u.tx.ieee_802_3.payload_length = len -12;
			fid->pkt_len = len - 12;
			fid->u.tx.payload = skb->data +12;
		};	
		memcpy(fid->u.tx.ieee_802_3.dst_mac,skb->data, 12);
		DEBUG(0x200,"%s tx simply 802.3 type \n ",dev->name);		

	} else {// 802.1H bridgeing
		fid->type 		|= p80211_8021H;
		fid->bridge_size 	= len + sizeof(fid->bridge) ;
		fid->bridge.dsap 	= 0xaa;
		fid->bridge.ssap 	= 0xaa;
		fid->bridge.ctrl 	= 0x03;
		fid->bridge.oui[0] = 0x0;
		fid->bridge.oui[1] = 0x40;
		fid->bridge.oui[2] = 0x96;
		fid->bridge.unknown1= 0x0000;
		fid->bridge.unknown2= htons((len) & 0x1000);
		fid->u.tx.radio_tx.PayloadLength 	= fid->bridge_size + 2;
		fid->u.tx.ieee_802_3.payload_length = fid->u.tx.radio_tx.PayloadLength ;
		

		fid->u.tx.payload = skb->data +12;
		if ((fid->u.tx.radio_tx.TX_Control &
		                 aironet4500_tx_control_header_type_802_11 )){
	
			memcpy(fid->u.tx.ieee_802_11.mac1,priv->p2p,6);
			memcpy(fid->u.tx.ieee_802_11.mac2,skb->data +6,6);
			memcpy(fid->u.tx.ieee_802_11.mac3,priv->bssid ,6);
			memset(fid->u.tx.ieee_802_11.mac4,0,6);
			fid->u.tx.ieee_802_11.gapLen=6;

			fid->u.tx.ieee_802_11.frame_control = 0x8;                 
		}		
		memcpy(fid->u.tx.ieee_802_3.dst_mac,priv->p2p, 6);
		memcpy(fid->u.tx.ieee_802_3.src_mac,dev->dev_addr, 6);
		fid->u.tx.payload = skb->data + 2 + sizeof(fid->bridge);
		fid->pkt_len = len ;
	
		DEBUG(0x200,"%s tx simply 802.1H type \n ",dev->name);
		
	};
	
	priv->stats.tx_bytes += fid->u.tx.ieee_802_3.payload_length;
	priv->stats.tx_packets++;
	
	
	awc_fid_queue_push_tail(&priv->tx_in_transmit,fid);
	udelay(1);
	awc_transmit_packet(dev,fid);
	if (priv->tx_large_ready.size <= 2 || priv->tx_small_ready.size <= 2 ){
		if (netif_running(dev))
			netif_stop_queue(dev);
	} else {
	  	if (netif_running(dev)) 
			netif_wake_queue(dev);
	}
	UP(&priv->tx_buff_semaphore);
	AWC_ENTRY_EXIT_DEBUG("exit\n");
	return 0;

	
   no_space:
	DEBUG(32,"%s tx buffs not found \n ",dev->name);
	#ifdef AWC_DEBUG
//		printk("s");
	#endif
   	netif_stop_queue (dev); //weell, here it must be set anyway and before
   	//priv->stats.tx_fifo_errors++;
   	UP(&priv->tx_buff_semaphore);
	AWC_ENTRY_EXIT_DEBUG("NoSpaceExit\n");
   	return 1 ;
  final:
	priv->stats.tx_errors++;
	UP(&priv->tx_buff_semaphore);
	if (!netif_running(dev)) 
		netif_start_queue(dev);
	dev_kfree_skb(skb);
	AWC_ENTRY_EXIT_DEBUG("BADExit\n");
	return -1;
  
};

/*
	called from low level driver->card tx copy routine  
	probably wants to free skbuf if failed transmits won't be
	resubmitted to another device (if more than one path)
	or tried again (if tx buffer in card needs to be filled again)
*/  
  
  
void
awc_802_11_after_tx_packet_to_card_write(struct net_device * dev,
					 struct awc_fid * tx_buff){


	AWC_ENTRY_EXIT_DEBUG("awc_802_11_after_tx_packet_to_card_write");

	if (!tx_buff){
		DEBUG(1,"%s no damn tx_buff in awc_802_11_after_tx_packet_to_card_write \n",dev->name);
	};

	if(tx_buff->skb){
		dev_kfree_skb(tx_buff->skb);
		tx_buff->skb = NULL;
	}

	AWC_ENTRY_EXIT_DEBUG("exit\n");
};

/*
        called from low level driver->card tx copy routine
        probably wants to free skbuf if failed writes won't be
        resubmitted to another device (if more than one path) 
        or tried again (if tx buffer in card needs to be filled again)
*/
                        
void
awc_802_11_after_failed_tx_packet_to_card_write(struct net_device * dev,
                                         struct awc_fid * tx_buff){
        struct awc_private * priv = (struct awc_private *)dev->priv;


	AWC_ENTRY_EXIT_DEBUG("awc_802_11_after_failed_tx_packet_to_card_write");

	if (!tx_buff){
		DEBUG(1,"%s no damn tx_buff in awc_802_11_after_failed_tx_packet_to_card_write \n",dev->name);
	};

	if(tx_buff->skb){
		dev_kfree_skb(tx_buff->skb);
		tx_buff->skb = NULL;
		tx_buff->busy =0;
		printk(KERN_ERR "%s packet to card write failed \n",dev->name);
	}
	
	awc_fid_queue_remove(&priv->tx_in_transmit,tx_buff);
	
	if (tx_buff->u.tx.fid_size <= AWC_TX_ALLOC_SMALL_SIZE)
		awc_fid_queue_push_tail(&priv->tx_small_ready,tx_buff);
	else 
		awc_fid_queue_push_tail(&priv->tx_large_ready,tx_buff);

	AWC_ENTRY_EXIT_DEBUG("exit\n");

};
                                         
inline void 
awc_802_11_after_tx_complete(struct net_device * dev, struct awc_fid * tx_buff){

        struct awc_private * priv = (struct awc_private *)dev->priv;

	AWC_ENTRY_EXIT_DEBUG("awc_802_11_after_tx_complete");                

	DEBUG(32,"tx complete status %x \n ",tx_buff->u.tx.radio_tx.Status);

	#ifdef AWC_DEBUG
	 if (tx_buff->u.tx.radio_tx.Status)
	 	printk("tf%x ",tx_buff->u.tx.radio_tx.Status);
	#endif
	if (tx_buff->u.tx.fid_size <= AWC_TX_ALLOC_SMALL_SIZE){
		awc_fid_queue_push_tail(&priv->tx_small_ready,tx_buff);
		priv->tx_small_buffs_in_use--;
	} else { 
		awc_fid_queue_push_tail(&priv->tx_large_ready,tx_buff);
		priv->tx_buffs_in_use--;
	}

	tx_buff->busy = 0;
//	netif_wake_queue (dev);

	AWC_ENTRY_EXIT_DEBUG("exit\n");
};

                                                 


/********************************	R X	***********************/



inline int
awc_receive_packet(struct net_device * dev){
	
    struct awc_command cmd;
    u16	Fid;
//    struct sk_buff *skb = NULL;
    struct awc_fid * rx_buff;


    struct awc_private * priv ;
	int i;    
    
    	priv= (struct awc_private *)dev->priv;
      	rx_buff = priv->rx_ready.head        ;

     AWC_ENTRY_EXIT_DEBUG(" entry awc_receive_packet ");
     
	Fid = awc_Rx_Fid(dev->base_addr);
	
	DEBUG(128," RX FID  %x	\n",Fid);

	if (!Fid){
		printk(KERN_CRIT "No RxFid when rx event \n");
		return -1;
	}


	
	if (!rx_buff){
		printk(KERN_CRIT "No rx_buff in rx event \n");
		return -1;
	}

	rx_buff->type   = 0;

	                
	AWC_INIT_COMMAND(AWC_CLI,cmd,dev,0,0,
			Fid, 0, 0x14 , &(rx_buff->u.rx.radio_rx));


// header reading , order is important
	AWC_BAP_LOCK_UNDER_CLI(cmd);

	if (awc_bap_setup(&cmd))		goto final;
	if (awc_bap_read(&cmd))		goto final;

	DEBUG(128, "rx receive radio header, length %x \n",rx_buff->u.rx.radio_rx.PayloadLength);

	cmd.buff 	= &(rx_buff->u.rx.ieee_802_11);
	cmd.len		= 0x20;

	if (awc_bap_read(&cmd))		goto final;

	DEBUG(128, "rx receive 802_11 header, framecontrol %x \n",rx_buff->u.rx.ieee_802_11.frame_control);

	if (rx_buff->u.rx.ieee_802_11.gapLen > 8) {
		printk(KERN_ERR "%s: 802.11 gap lenght huge %d \n",dev->name,rx_buff->u.rx.ieee_802_11.gapLen);
		goto final;
	}
	DEBUG(128,"SeqCtl %x, 802_11 macs: ",rx_buff->u.rx.ieee_802_11.SeqCtl);
	if (awc_debug & 0x7000){
		DEBUG(0x7000, " %s mac1 ",dev->name); for (i = 0; i < 6; i++) DEBUG(0x7000, "%02x:",((unsigned char)rx_buff->u.rx.ieee_802_11.mac1[i] )) ;
		DEBUG(0x7000, " %s mac2 ",dev->name); for (i = 0; i < 6; i++) DEBUG(0x7000, "%02x:",((unsigned char)rx_buff->u.rx.ieee_802_11.mac2[i] )) ;
		DEBUG(0x7000, " %s mac3 ",dev->name); for (i = 0; i < 6; i++) DEBUG(0x7000, "%02x:",((unsigned char)rx_buff->u.rx.ieee_802_11.mac3[i] )) ;
		DEBUG(0x7000, " %s mac4 ",dev->name); for (i = 0; i < 6; i++) DEBUG(0x7000, "%02x:",((unsigned char)rx_buff->u.rx.ieee_802_11.mac4[i] )) ;
	}	
	DEBUG(128,"\n GapLen %d ",rx_buff->u.rx.ieee_802_11.gapLen );
	
	if (rx_buff->u.rx.ieee_802_11.gapLen > 0) {
		cmd.buff     = rx_buff->u.rx.ieee_802_11.gap;
		cmd.len      = rx_buff->u.rx.ieee_802_11.gapLen;
		if (awc_bap_read(&cmd))	     goto final;
		DEBUG(128, "rx receive gap header , gap length %x \n",rx_buff->u.rx.gap_length);
	}
	for (i = 0; i < rx_buff->u.rx.ieee_802_11.gapLen ; i++) DEBUG(128,"%x",((unsigned char)rx_buff->u.rx.ieee_802_11.gap[i] )) ;

        
	if ( !(priv->config.ReceiveMode & RXMODE_DISABLE_802_3_HEADER ) 
	     ){
		cmd.buff     	 = &(rx_buff->u.rx.ieee_802_3);
		cmd.len      	 = 0x10;
		rx_buff->type 	|= p80211_8023;
		if (awc_bap_read(&cmd))				goto final;
		DEBUG(128, "rx receive 802_3 header, payload length %x \n",rx_buff->u.rx.ieee_802_3.payload_length);
        	DEBUG(128,"\n 802_3 status %x ",rx_buff->u.rx.ieee_802_3.status );
		DEBUG(128," RX payloadLen %x, dst,src: ",rx_buff->u.rx.ieee_802_3.payload_length);
		if (awc_debug & 0x7000){
			for (i = 0; i < 6; i++) printk("%02x:",((unsigned char)rx_buff->u.rx.ieee_802_3.dst_mac[i] )) ;
			for (i = 0; i < 6; i++) printk("%02x:",((unsigned char)rx_buff->u.rx.ieee_802_3.src_mac[i] )) ;
		}
	};

	rx_buff->pkt_len = rx_buff->u.rx.radio_rx.PayloadLength;
	
	if (priv->config.OperatingMode & MODE_LLC_HOST)
		rx_buff->type   |= p80211_llc_snap;

	
	if (awc_802_11_find_copy_path(dev,rx_buff))		goto final;


	if (rx_buff->u.rx.payload ){	
		cmd.buff = rx_buff->u.rx.payload;
		cmd.len	 = rx_buff->pkt_len;
		if (awc_bap_read(&cmd))				goto final;
		DEBUG(128, "rx payload read %x \n",rx_buff->u.rx.ieee_802_3.payload_length);
	};
	
	AWC_RELEASE_COMMAND(cmd);

        DEBUG(128,"\n payload hdr %x ",rx_buff->u.rx.ieee_802_3.status );
        if (awc_debug && rx_buff->u.rx.payload)
		for (i = 0; i < 20; i++) DEBUG(128,"%x",((unsigned char)rx_buff->u.rx.payload[i] )) ;
	DEBUG(128,"%c",'\n');

	awc_802_11_router_rx(dev,rx_buff);

	AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	return 0;
     final:
     
        awc_802_11_failed_rx_copy(dev,rx_buff);
     	// if (skb) dev_kfree_skb(skb, FREE_WRITE);
     	AWC_RELEASE_COMMAND(cmd);
     	AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	return -1; ;
     	
};


int
awc_transmit_packet(struct net_device * dev, struct awc_fid * tx_buff) {
	
	struct awc_command cmd;
	u16 size ;
//	unsigned long flags;
	int i;
    struct awc_private * priv= (struct awc_private *)dev->priv;

     AWC_ENTRY_EXIT_DEBUG(" entry awc_transmit_packet ");
     
	if (priv->link_status_changed ){
		priv->link_status_changed =0;
  		awc_readrid_dir(dev,&priv->rid_dir[7]);
	}
	

        AWC_INIT_COMMAND(AWC_NOT_CLI,cmd,dev,0xB, tx_buff->u.tx.fid,
          	tx_buff->u.tx.fid, 0, 0x14 , &(tx_buff->u.tx.radio_tx));
	
	AWC_BAP_LOCK_NOT_CLI(cmd);

#ifdef AWC_BY_BOOK
#warning  By books is bad, AWC_BY_BOOK  
#error cli sti bad here
	if (    !(tx_buff->type &(p80211_llc_snap|p80211_8021H) ) 
	     && !(tx_buff->u.tx.radio_tx.TX_Control &
	                   aironet4500_tx_control_header_type_802_11 )){
	
		cmd.buff=&(tx_buff->u.tx.radio_tx.TX_Control);
		cmd.len = 0x2 ;
        	cmd.offset = 0x8;
        	save_flags(flags);
        	cli();
        	if (awc_bap_setup(&cmd))		goto final;        
        	if (awc_bap_write(&cmd))		goto final;

		cmd.buff=&(tx_buff->u.tx.ieee_802_3.payload_length);
		cmd.len = 14;
        	cmd.offset = 0x36;
        	if (awc_bap_setup(&cmd))		goto final;        
     		if (awc_bap_write(&cmd))		goto final;
     		restore_flags(flags);

	} else {
#endif
			
        	if (awc_bap_setup(&cmd))		goto final;        
        	if (awc_bap_write(&cmd))		goto final;
        
        	DEBUG(64," wrote radio tx header for fid %x \n",tx_buff->u.tx.fid);

		// 802.11
        	cmd.buff=&(tx_buff->u.tx.ieee_802_11);
        	cmd.len = 0x20;
        	if (awc_bap_write(&cmd))                goto final;

		// Gap
		if (tx_buff->u.tx.ieee_802_11.gapLen) {
        		cmd.buff=&(tx_buff->u.tx.ieee_802_11.gap);
        		cmd.len = tx_buff->u.tx.ieee_802_11.gapLen;
        		if (awc_bap_write(&cmd))	goto final;
        	}
	        // 802.3
	        if ( !	(tx_buff->u.tx.radio_tx.TX_Control & 
	      	 	aironet4500_tx_control_header_type_802_11 )){
	       		 
       			cmd.buff=&(tx_buff->u.tx.ieee_802_3);
			if (awc_debug & 0x7000){
				printk("%s TX dst ",dev->name);
				for (i=0; i < 6; i++) printk ("%02x:",(unsigned char) tx_buff->u.tx.ieee_802_3.dst_mac[i]);
				printk(" src ");
				for (i=0; i < 6; i++) printk ("%02x:",(unsigned char) tx_buff->u.tx.ieee_802_3.src_mac[i]);
				printk(" \n ");
			}
       			cmd.len = 0x10; 
       			if (awc_bap_write(&cmd))	goto final;
       		};
       	 	
       		if (tx_buff->type & p80211_llc_snap) {
       			cmd.buff=	& tx_buff->snap;
       			cmd.len =	sizeof(tx_buff->snap);
       			if (awc_bap_write(&cmd))		goto final;
        	};
	 
	  	if (tx_buff->type & p80211_8021H) {
       	 		size = htons(tx_buff->bridge_size);  
        	//	size = tx_buff->bridge_size;// to seasure raw speed of f** UC  
       			cmd.buff=	& size;
       			cmd.len =	2 ;
        		if (awc_bap_write(&cmd))                goto final;
        		
       			cmd.buff=	& tx_buff->bridge;
       			cmd.len =	sizeof(tx_buff->bridge);
       			if (awc_bap_write(&cmd))		goto final;
        	};
	       	 
#ifdef AWC_BY_BOOK	       	 
	       	 
	}
#endif
       	cmd.buff=	tx_buff->u.tx.payload;
       	cmd.len =	tx_buff->pkt_len;

       	if (awc_bap_write(&cmd))			goto final;
	AWC_RELEASE_COMMAND(cmd);
// locking probs,  these two lines below and above, swithc order 
	if (awc_issue_command_and_block(&cmd))		goto final_unlocked;      


	tx_buff->transmit_start_time = jiffies;
	awc_802_11_after_tx_packet_to_card_write(dev,tx_buff);         		
           // issue the transmit command


        AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
	return 0;
     final:
	awc_802_11_after_failed_tx_packet_to_card_write(dev,tx_buff);     	        
     	printk(KERN_CRIT "%s awc tx command failed \n",dev->name);
     	AWC_RELEASE_COMMAND(cmd);
     	AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	return -1; ;

     final_unlocked:
	awc_802_11_after_failed_tx_packet_to_card_write(dev,tx_buff);     	        
     	printk(KERN_CRIT "%s awc tx command failed \n",dev->name);
     	AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	return -1; ;

}


inline int
awc_tx_complete_check(struct net_device * dev){

	struct awc_fid	* fid;
	struct awc_command cmd;


     AWC_ENTRY_EXIT_DEBUG(" entry awc_tx_complete_check ");
     
		

	fid = awc_fid_queue_pop_head(&((struct awc_private *)dev->priv)->tx_post_process);
	
	if (!fid) {
		printk("awc_tx_complete_check with empty queue \n ");
		return -1;
	}

	DEBUG(64," tx_complete fid %x \n",fid->u.tx.fid);
	
        AWC_INIT_COMMAND(AWC_NOT_CLI,cmd,dev,0,0, fid->u.tx.fid,
          			0, 0x14 , &(fid->u.tx.radio_tx));

	fid->state  |= awc_tx_fid_complete_read;

	AWC_BAP_LOCK_NOT_CLI(cmd);
        if (awc_bap_setup(&cmd))		goto final;
        if (awc_bap_read(&cmd))			goto final;
	AWC_RELEASE_COMMAND(cmd);
        
	awc_802_11_after_tx_complete(dev,fid);         		

	
        AWC_ENTRY_EXIT_DEBUG(" exit \n"); 
 	return 0;
        
     final:
        awc_802_11_after_tx_complete(dev,fid);
        printk(KERN_ERR "%s awc_tx_complete_check failed \n",dev->name);
     	AWC_RELEASE_COMMAND(cmd);
     	AWC_ENTRY_EXIT_DEBUG("  BAD exit \n");
	return -1; ;
}


#define AWC_QUEUE_BH {\
	if (!priv->bh_active && !priv->bh_running){\
		priv->bh_active = 1;\
		queue_task(&priv->immediate_bh, &tq_immediate);\
		mark_bh(IMMEDIATE_BH);\
	}\
	}


void
awc_bh(struct net_device *dev){

        struct awc_private * priv = (struct awc_private *)dev->priv;
      	int  active_interrupts;
	int enabled_interrupts;
//	u16	tx_status;
	int 	multi_ints = 0;
//	u16	tx_fid = 0;
//	unsigned long flags;

	DEBUG(8, "awc_bh awoken on jiffie %ld \n",jiffies);

	priv->bh_running = 1;
	
	active_interrupts = awc_event_status(dev->base_addr);
	
        enabled_interrupts = awc_ints_enabled(dev->base_addr);

	DEBUG(8, "awc_bh active ints %x \n",active_interrupts);

        if (test_and_set_bit( 0, (void *) &priv->tx_chain_active) ) {
//		printk(KERN_ERR "tx chain active in bh \n");
//		queue_task(&priv->immediate_bh, &tq_immediate);
		goto bad_end;
	}
start:
	if (active_interrupts == 0xffff){
	
		printk(KERN_CRIT "%s device ejected in interrupt, disabling\n",dev->name);
		netif_device_detach (dev);
		if (priv->command_semaphore_on){
			priv->command_semaphore_on--;
			AWC_UNLOCK_COMMAND_ISSUING(priv);
		}
		priv->tx_chain_active =0;
		goto bad_end;
		    
	}

	if (priv->unlock_command_postponed ){
	
	   priv->unlock_command_postponed-- ;
	   if( priv->command_semaphore_on ){
		
     		awc_read_response((&priv->cmd));
     		priv->async_command_start = 0;
     		if (priv->command_semaphore_on){
     		
     			priv->command_semaphore_on--;
		        AWC_UNLOCK_COMMAND_ISSUING(priv);
		}
     	    }
     	};

/*        if ( active_interrupts & 0x1 ){
	       		awc_receive_packet(dev) ;
			awc_event_ack_Rx(dev->base_addr);
			priv->waiting_interrupts &= ~0x1;
	}
*/
	while (priv->tx_post_process.size)
		if (awc_tx_complete_check(dev)) break;
	
	active_interrupts = awc_event_status(dev->base_addr);

	if (priv->command_semaphore_on || priv->tx_post_process.size){
			if (multi_ints++ < 10000){
				goto start;
			}
		};
		priv->bh_active  = 0;
		priv->bh_running = 0;

        priv->tx_chain_active = 0;

  	

  bad_end:
//	if (!priv->tx_chain_active) 
//		wake_up(&priv->tx_chain_wait_queue);
  
  	priv->bh_running = 0;
	priv->bh_active = 0;
	return ;
};


inline int
awc_interrupt_process(struct net_device * dev){

	struct awc_private * priv ;
      	int  active_interrupts;
	int enabled_interrupts;
	u16	tx_status;
	int 	multi_ints = 0;
	u16	tx_fid = 0;
//	u16	ints_to_ack =0;
	struct awc_fid	* fid = NULL;
//	int interrupt_reenter = 0;
//	unsigned long flags;	

//	save_flags(flags);
//	cli();
	// here we need it, because on 2.3 SMP there are truly parallel irqs 	
	disable_irq(dev->irq);

	DEBUG(2," entering interrupt handler %s ",dev->name);

	if (!dev) {
		printk(KERN_ERR "No dev in interrupt   \n");
		goto bad_end;
	};

	priv = (struct awc_private *)dev->priv;

	if (!priv) {
		printk(KERN_ERR "No PRIV in interrupt \n");
		goto bad_end;
	};


        enabled_interrupts = awc_ints_enabled(dev->base_addr);
	active_interrupts = awc_event_status(dev->base_addr);

	DEBUG(2,"entry: processing interrupts waiting %x \n",priv->waiting_interrupts);
	DEBUG(2,"entry: processing interrupts active  %x \n",active_interrupts);
	DEBUG(2,"entry: processing interrupts enabled %x \n",enabled_interrupts);
//	printk("ikka interruptis\n");


	priv->interrupt_count++;
	if (priv->interrupt_count > 1 )
		printk(" interrupt count on\n ");



	if (priv->waiting_interrupts & active_interrupts)
		printk(KERN_ERR "double interrupt waiting %x active %x \n",
				priv->waiting_interrupts, active_interrupts);

 //       priv->waiting_interrupts |= active_interrupts;





start:
	DEBUG(2,"Start processing int, times %d\n",multi_ints);
	
	if (active_interrupts == 0xffff){
	
		printk(KERN_CRIT "%s device ejected, got interrupt, disabling\n",dev->name);
		//priv->
		netif_device_detach (dev);
		priv->ejected = 1;
		if (priv->bh_active || priv->bh_running){
			priv->interrupt_count--;
			goto bad_end;
		} else if (priv->command_semaphore_on){
			
			printk(KERN_ERR "ejected, last BH fired \n");
			
			 AWC_QUEUE_BH;    
		}
		priv->interrupt_count--;
		goto bad_end;
	}

	

	if (active_interrupts & 0x100 ){
		awc_event_ack_Awaken(dev->base_addr);
		udelay(10);
		DEBUG(1,"%s device awoke \n",dev->name);
		priv->waiting_interrupts &= ~0x100;
	};
	if (active_interrupts & 0x80 ){
	
		priv->link_status = awc_Link_Status(dev->base_addr);
		DEBUG(1,"link status changed %x \n",priv->link_status);
		awc_event_ack_Link(dev->base_addr);
		priv->waiting_interrupts &= ~0x80;
	  	if(priv->link_status == 0x400)
	  				printk(KERN_INFO "%s Associated\n",dev->name );
	  	else { 
	  		printk(KERN_INFO "%s Link status change : %s \n",dev->name, awc_print_string(awc_link_status_names, priv->link_status) );
	  		if (	priv->link_status & 0x8100  ||
	  			priv->link_status & 0x0100  ||
	  			priv->link_status & 0x8200  ||
	  			priv->link_status & 0x8400  ||
	  			priv->link_status & 0x0300  )
	  		printk(KERN_INFO "%s Link status change reason : %s \n",dev->name, awc_print_string(awc_link_failure_reason_names, priv->link_status & 0xff) );
	  			
	  	}
	};


	if (active_interrupts & 0x10 & enabled_interrupts ){

//		printk(KERN_ERR "cmd int shouldnt be active in interrupt routine\n");

     		awc_event_ack_Cmd(priv->cmd.port);
     
     		if ( priv->enabled_interrupts & 0x10) 
     			priv->enabled_interrupts &= ~0x10;
	     	
	     	enabled_interrupts = awc_ints_enabled(dev->base_addr);

		if (enabled_interrupts & 0x10){
			awc_ints_enable(dev->base_addr, enabled_interrupts & ~0x10);
 	    	}

     		if (priv->command_semaphore_on){
     			priv->unlock_command_postponed++;

			AWC_QUEUE_BH;
		}
	}
	
	if ((active_interrupts & 0x10) && !(0x10 & enabled_interrupts) ){

//		printk(KERN_ERR "%s: aironet4500: cmd int shouldnt be active in interrupt routine\n",dev->name);

     		//awc_event_ack_Cmd(priv->cmd.port);
	}     


//	active_interrupts = awc_event_status(dev->base_addr);

	tx_status = active_interrupts & 0x6 ;
	


	if (tx_status) {

		tx_fid = awc_Tx_Compl_Fid(dev->base_addr);
		if (!tx_fid){
			udelay(10);
			tx_fid = awc_Tx_Compl_Fid(dev->base_addr);
		}
		if (!tx_fid)
			printk(KERN_ERR "No tx fid when tx int active\n");
			
		fid = awc_tx_fid_lookup_and_remove(dev, tx_fid);

		if (fid) {
			if (priv->process_tx_results) {
				awc_fid_queue_push_tail(&priv->tx_post_process,fid);
				AWC_QUEUE_BH;
			}else {
				if (fid->u.tx.fid_size <= AWC_TX_ALLOC_SMALL_SIZE)
    					awc_fid_queue_push_tail(&priv->tx_small_ready,fid);
          			else
      					awc_fid_queue_push_tail(&priv->tx_large_ready,fid);
				netif_wake_queue (dev);
			}
		} else 
			printk(KERN_ERR "awc fid %x not found\n",tx_fid);
		

		if (tx_status & 2){
			awc_event_ack_Tx(dev->base_addr);
			priv->stats.tx_packets++;
			priv->waiting_interrupts  &= ~0x2;
		}
		if (tx_status & 4){
			priv->stats.tx_errors++;
			awc_event_ack_TxExc(dev->base_addr);
			priv->waiting_interrupts  &= ~0x4;
		}
		if ((tx_status&6) == 6)
			printk(KERN_NOTICE "%s: both tx and txExc up\n",dev->name);


	}

//	active_interrupts = awc_event_status(dev->base_addr);

        if ( active_interrupts & 0x1 ){
	       		awc_receive_packet(dev);
			awc_event_ack_Rx(dev->base_addr);
			priv->waiting_interrupts &= ~0x1;
	}

	active_interrupts = awc_event_status(dev->base_addr);

	if ((active_interrupts & 0x7) && 
	     !priv->bh_active && 
	     !priv->bh_running ){
		if (multi_ints++ < 5)
			goto start;
        }
	if (multi_ints >=5 )
		printk(KERN_ERR "%s multi_ints > 5 interrupts still active %x\n",dev->name,active_interrupts); 

 
	priv->interrupt_count--;

	awc_ints_enable(dev->base_addr, 0x0000);


	DEBUG(0x8, " enabling ints in interrupt_process %x \n",
		priv->enabled_interrupts & ~priv->waiting_interrupts);
 


        AWC_ENTRY_EXIT_DEBUG(" exit \n"); 

 	awc_ints_enable(dev->base_addr, 
 		priv->enabled_interrupts);

//end_here:

	enable_irq(dev->irq);
//  	restore_flags(flags);

        return 0;

bad_end:
        AWC_ENTRY_EXIT_DEBUG(" bad_end exit \n"); 	
	enable_irq(dev->irq);
//	restore_flags(flags);
	return -1;


};

static const char *aironet4500_core_version =
"aironet4500.c v0.1 1/1/99 Elmer Joandi, elmer@ylenurme.ee.\n";

struct net_device * aironet4500_devices[MAX_AWCS];

int awc_debug; //  0xffffff;
static int p802_11_send; // 1

static int awc_process_tx_results;
int tx_queue_len = 10;
int tx_rate;
int channel = 5;
//static int tx_full_rate;
int max_mtu = 2312;
int adhoc;
int large_buff_mem = 1700 * 10;
int small_buff_no	= 20;
int awc_full_stats;
char SSID[33];
int master;
int slave;
int awc_simple_bridge;
// int debug =0;

#if LINUX_VERSION_CODE >= 0x20100

MODULE_PARM(awc_debug,"i");
MODULE_PARM(tx_rate,"i");
MODULE_PARM(channel,"i");
//MODULE_PARM(tx_full_rate,"i");
MODULE_PARM(adhoc,"i");
MODULE_PARM(master,"i");
MODULE_PARM(slave,"i");
MODULE_PARM(awc_simple_bridge,"i");
MODULE_PARM(max_mtu,"i");
MODULE_PARM(large_buff_mem,"i");
MODULE_PARM(small_buff_no,"i");
MODULE_PARM(SSID,"c33");
MODULE_PARM_DESC(awc_debug,"Aironet debug mask");
MODULE_PARM_DESC(channel,"Aironet ");
MODULE_PARM_DESC(adhoc,"Aironet Access Points not available (0-1)");
MODULE_PARM_DESC(master,"Aironet is Adhoc master (creates network sync) (0-1)");
MODULE_PARM_DESC(slave,"Aironet is Adhoc slave (0-1)");
MODULE_PARM_DESC(max_mtu,"Aironet MTU limit (256-2312)");
#endif
MODULE_LICENSE("GPL");


/*EXPORT_SYMBOL(tx_queue_len);
EXPORT_SYMBOL(awc_debug);
 */
EXPORT_SYMBOL(awc_init);
EXPORT_SYMBOL(awc_open);
EXPORT_SYMBOL(awc_close);
EXPORT_SYMBOL(awc_reset);
EXPORT_SYMBOL(awc_config);

EXPORT_SYMBOL(aironet4500_devices);
EXPORT_SYMBOL(awc_debug);
//EXPORT_SYMBOL();

EXPORT_SYMBOL(awc_private_init);
EXPORT_SYMBOL(awc_tx_timeout);
EXPORT_SYMBOL(awc_start_xmit);
EXPORT_SYMBOL(awc_interrupt);
EXPORT_SYMBOL(awc_get_stats);
EXPORT_SYMBOL(awc_change_mtu);
EXPORT_SYMBOL(awc_set_multicast_list);

EXPORT_SYMBOL(awc_proc_set_fun);
EXPORT_SYMBOL(awc_proc_unset_fun);
EXPORT_SYMBOL(awc_register_proc);
EXPORT_SYMBOL(awc_unregister_proc);


/***************************  RESET INIT CONFIG ***********************/


 void awc_reset(struct net_device *dev)
{

	long long jiff;

	DEBUG(2, " awc_reset dev %p \n", dev);
	DEBUG(2, "%s: awc_reset \n",  dev->name);
	
	awc_issue_soft_reset(dev);
	
	jiff = jiffies;
	udelay(1000);
	while (awc_command_read(dev->base_addr)){
		udelay(1000);
		if (jiffies - jiff > 5*HZ){
			printk(KERN_CRIT "%s bad reset\n",dev->name);
			break;
		}
	};

}

 int awc_config(struct net_device *dev)
{
//	struct awc_private *priv = (struct awc_private *)dev->priv;

	DEBUG(2, "%s: awc_config \n",  dev->name);

	
        if( awc_disable_MAC(dev))		goto final;
	udelay(100);        
	if( awc_write_all_rids(dev) )		goto final;
	udelay(100);
        if( awc_enable_MAC(dev))		goto final;

	return 0;
   final: 
   	return -1;
}


char name[] = "ElmerLinux";

 int awc_init(struct net_device *dev){
        struct awc_private *priv = (struct awc_private *)dev->priv;
	int i;        
	const char * radioType;
 
	DEBUG(2, "%s: awc_init \n",  dev->name);

	/* both_bap_lock decreases performance about 15% 
	 * but without it card gets screwed up 
	 */ 
#ifdef CONFIG_SMP
	if(smp_num_cpus > 1){
		both_bap_lock = 1;
		bap_setup_spinlock = 1;
	}
#endif
	//awc_dump_registers(dev);

	if (adhoc & !max_mtu)
		max_mtu= 2250;
	else if (!max_mtu)
		max_mtu= 1500;
			
        priv->sleeping_bap = 1;
        	

	priv->enabled_interrupts = awc_ints_enabled(dev->base_addr);
        
 	if( awc_issue_noop(dev) ) 	goto final;

	awc_ints_enable(dev->base_addr,0);

 	if( awc_disable_MAC(dev) )	goto final;

		
//	awc_rids_setup(dev);
	i=0;
	while ( i < AWC_NOF_RIDS){
		if (awc_rids_temp[i].selector)
			memcpy(&priv->rid_dir[i],&awc_rids_temp[i],sizeof(priv->rid_dir[0]) );
		else priv->rid_dir[i].selector = NULL;
		i++;
	}
	
	// following MUST be consistent with awc_rids in count and ordrering !!!
 	priv->rid_dir[0].buff = &priv->config; // card RID mirrors
	priv->rid_dir[1].buff = &priv->SSIDs;
	priv->rid_dir[2].buff = &priv->fixed_APs;
     	priv->rid_dir[3].buff = &priv->driver_name;
      	priv->rid_dir[4].buff = &priv->enc_trans;
	priv->rid_dir[5].buff = &priv->general_config; //      	
	priv->rid_dir[6].buff = &priv->capabilities;
 	priv->rid_dir[7].buff = &priv->status;
  	priv->rid_dir[8].buff = &priv->AP;
   	priv->rid_dir[9].buff = &priv->statistics;
    	priv->rid_dir[10].buff = &priv->statistics_delta;
     	priv->rid_dir[11].buff = &priv->statistics_delta_clear;
	priv->rid_dir[12].buff = &priv->wep_volatile;
	priv->rid_dir[13].buff = &priv->wep_nonvolatile;
	priv->rid_dir[14].buff = &priv->modulation;

      	priv->rid_dir[15].buff = &priv->statistics16;
	priv->rid_dir[16].buff = &priv->statistics16_delta;
 	priv->rid_dir[17].buff = &priv->statistics16_delta_clear;
                       	
 	priv->rid_dir[0].bufflen = sizeof(priv->config); // card RID mirrors
	priv->rid_dir[1].bufflen = sizeof(priv->SSIDs);
	priv->rid_dir[2].bufflen = sizeof(priv->fixed_APs);
     	priv->rid_dir[3].bufflen = sizeof(priv->driver_name);
      	priv->rid_dir[4].bufflen = sizeof(priv->enc_trans);
	priv->rid_dir[5].bufflen = sizeof(priv->general_config); //
	priv->rid_dir[6].bufflen = sizeof(priv->capabilities);
 	priv->rid_dir[7].bufflen = sizeof(priv->status);
  	priv->rid_dir[8].bufflen = sizeof(priv->AP);
   	priv->rid_dir[9].bufflen = sizeof(priv->statistics);
    	priv->rid_dir[10].bufflen = sizeof(priv->statistics_delta);
     	priv->rid_dir[11].bufflen = sizeof(priv->statistics_delta_clear);
	priv->rid_dir[12].bufflen = sizeof(priv->wep_volatile);
	priv->rid_dir[13].bufflen = sizeof(priv->wep_nonvolatile);
	priv->rid_dir[14].bufflen = sizeof(priv->modulation);

      	priv->rid_dir[15].bufflen = sizeof(priv->statistics16);
	priv->rid_dir[16].bufflen = sizeof(priv->statistics16_delta);
 	priv->rid_dir[17].bufflen = sizeof(priv->statistics16_delta_clear);


 	if( awc_read_all_rids(dev) )	goto final;
 

 	priv->config.OperatingMode = 0;// MODE_LLC_HOST;
 	DEBUG(1,"ReceiveMode %x \n",priv->config.ReceiveMode);
 //	priv->config.ReceiveMode	=  RXMODE_DISABLE_802_3_HEADER;
 	
	if (!adhoc)
	        priv->config.OperatingMode = MODE_STA_ESS;
//        priv->config.OperatingMode = MODE_AP;
// Setting rates does not work with new hardware, use force_tx_rate via proc
//	priv->config.Rates[0]	=0x82;
//	priv->config.Rates[1]	=0x4;
//	priv->config.Rates[2]	=tx_full_rate;
//	priv->config.Rates[3]	=0;
//	priv->config.Rates[4]	=0;
//	priv->config.Rates[5]	=0;
//	priv->config.Rates[6]	=0;
//	priv->config.Rates[7]	=0;
	priv->config.Channel	= channel;
	if (adhoc && master){
		priv->config.JoinNetTimeout	= 0x1;//0 is facotry default
	} else if (adhoc && slave){
		// by spec 0xffff, but, this causes immediate bad behaviour
		// firmware behvaiour changed somehere around ver 2??
		priv->config.JoinNetTimeout	= 0x7fff;
	};	
//	priv->config.AuthenticationType = 1;
	priv->config.Stationary	=1;
//	priv->config.ScanMode	= 1;
//	priv->config.LinkLossDelay	= 100;
	priv->config.FragmentThreshold = 1700;
	priv->config.RtsThreshold	= 1700;
	memcpy(priv->config.NodeName, name, 10);

	DEBUG(1,"%s supported Rates \n",dev->name);
	for (i=0; i< 8; i++)
		DEBUG(1,"%x ",priv->capabilities.SupportedRates[i]);
	DEBUG(1,"%c",'\n');
	DEBUG(1,"%s default Rates \n",dev->name);
	for (i=0; i< 8; i++)
		DEBUG(1,"%x ",priv->config.Rates[i]);
	DEBUG(1,"%c",'\n');
			

	// here we go, bad aironet
	memset(&priv->SSIDs,0,sizeof(priv->SSIDs));

	spin_lock_init(&priv->queues_lock);
        priv->SSIDs.ridLen		=0;
        if (!SSID) {
	        priv->SSIDs.SSID[0].SSID[0] 	='a';
	        priv->SSIDs.SSID[0].SSID[1] 	='b';
	        priv->SSIDs.SSID[0].SSID[2] 	='c';
        	priv->SSIDs.SSID[0].lenght 	=3;
        } else {
        	int sidlen = strlen(SSID);
        	memcpy(priv->SSIDs.SSID[0].SSID,SSID,sidlen);
        	priv->SSIDs.SSID[0].lenght = sidlen;
        };
        
        priv->SSIDs.SSID[1].lenght 	=0;
        priv->SSIDs.SSID[1].SSID[0] 	=0;
        priv->SSIDs.SSID[1].SSID[1] 	=0;        
        priv->SSIDs.SSID[2].lenght 	=0;
        priv->SSIDs.SSID[2].SSID[0] 	=0;
        priv->SSIDs.SSID[2].SSID[1] 	=0;


//	priv->enc_trans.rules[0].etherType= 0x0008;
//	priv->enc_trans.rules[0].Action   = 1;        
         	
 	memcpy(	priv->config.StationMacAddress,
 		priv->capabilities.FactoryAddress,	6	);

        memcpy(dev->dev_addr, priv->config.StationMacAddress, 6);

	DEBUG(2, "%s: awc_init success \n",  dev->name);
	
	if (priv->capabilities.RadioType == 1) radioType = "802.11 Frequency Hoping";
	else if (priv->capabilities.RadioType == 2) radioType = "802.11 Direct Sequence";
	else if (priv->capabilities.RadioType == 4) radioType = "LM2000";
	else radioType = "Multiple Radio Types";

	printk("%s: %s %s found @ 0x%lx irq %d firmwareVersion %d \n",dev->name,
		priv->capabilities.ProductName,radioType,
		dev->base_addr,dev->irq,
		priv->capabilities.SoftwareVersion);
 
 	return 0; 
   final:
   	printk(KERN_ERR "aironet init failed \n");
   	return NODEV;
   	
 };

int awc_private_init(struct net_device * dev){
	struct awc_private * priv = (struct awc_private *) dev->priv;
	int i = 0;

	DEBUG(2, "%s: awc_private_init \n",  dev->name);
	
	
	memset(priv, 0, sizeof(struct awc_private)); 
	
	spin_lock_init(&priv->queues_lock);
	
	priv->bap0.select 	= dev->base_addr + awc_Select0_register;
	priv->bap0.offset 	= dev->base_addr + awc_Offset0_register;
	priv->bap0.data		= dev->base_addr + awc_Data0_register;
	priv->bap0.lock 	= 0;
	priv->bap0.status	= 0;
	spin_lock_init(&priv->bap0.spinlock);
	init_MUTEX(&priv->bap0.sem);
	priv->bap1.select 	= dev->base_addr + awc_Select1_register;
	priv->bap1.offset 	= dev->base_addr + awc_Offset1_register;
	priv->bap1.data		= dev->base_addr + awc_Data1_register;
	priv->bap1.lock 	= 0;
	priv->bap1.status	= 0;
	spin_lock_init(&priv->bap1.spinlock);
	init_MUTEX(&priv->bap1.sem);
	priv->sleeping_bap	= 1;
	
//spinlock now	init_MUTEX(&priv->command_semaphore);
	spin_lock_init(&priv->command_issuing_spinlock);
	spin_lock_init(&priv->both_bap_spinlock);
	spin_lock_init(&priv->bap_setup_spinlock);
	spin_lock_init(&priv->interrupt_spinlock);
	
	priv->command_semaphore_on = 0;
	priv->unlock_command_postponed = 0;
	INIT_LIST_HEAD(&priv->immediate_bh.list);
	priv->immediate_bh.sync 	= 0;
	priv->immediate_bh.routine 	= (void *)(void *)awc_bh;
	priv->immediate_bh.data 	= dev;
	priv->bh_running	= 0;
	priv->bh_active		= 0;
	priv->tx_chain_active	= 0;
	priv->enabled_interrupts= 0x00;
	priv->waiting_interrupts= 0x00;
	
	
	init_MUTEX(&priv->tx_buff_semaphore);
	priv->tx_buffs_in_use	= 0;
	priv->tx_small_buffs_in_use = 0;
	priv->mac_enabled 	=0;
	priv->link_status	=0;
	priv->large_buff_mem	= large_buff_mem;
	if (priv->large_buff_mem < max_mtu + AWC_TX_HEAD_SIZE + 10 )
		priv->large_buff_mem = max_mtu + AWC_TX_HEAD_SIZE + 10;
	priv->small_buff_no	= small_buff_no;
	if (priv->small_buff_no  < 1 )
		priv->small_buff_no = 1 ;

	priv->process_tx_results = awc_process_tx_results;

	//init_waitqueue(&priv->tx_chain_wait_queue);

	for (i=0; i< 6 ; i++ ) {
		priv->p2p[i] = 0xff;
		priv->bssid[i] =0;
	}
//	priv->p2p_uc 		=1;
	priv->p2p_found		=0;

	priv->p802_11_send	=p802_11_send;
	priv->full_stats	= awc_full_stats;
	priv->simple_bridge	= awc_simple_bridge;
	priv->force_rts_on_shorter = 0;
	priv->force_tx_rate	= tx_rate;
	priv->ip_tos_reliability_rts = 0;
	priv->ip_tos_troughput_no_retries = 0 ;	

	priv->ejected		=0;	
	priv->interrupt_count	=0;
	
	return 0;
	
};

/****************************	OPEN	CLOSE	**********************/

 
 int awc_open(struct net_device *dev)
{
	struct awc_private *priv = (struct awc_private *)dev->priv;



	DEBUG(2, "%s: awc_open \n",  dev->name);

	if( awc_queues_init(dev) )		goto final;
	if( awc_config(dev) )		goto final;
	
	memcpy(dev->dev_addr, priv->config.StationMacAddress, 6);
	 
	priv->enabled_interrupts = 0x87;
	awc_ints_enable(dev->base_addr,priv->enabled_interrupts);

//	priv->p8022_client 	= register_8022_client;
//	priv->snap_client	= register_snap_client;	
	DEBUG(2, "%s: opened \n", dev->name);

	priv->sleeping_bap  = 0;
	
	
	MOD_INC_USE_COUNT;
//	kernel_thread(awc_thread,dev,0);

	netif_start_queue (dev);
	return 0;					/* Always succeed */

   final:
   	netif_device_detach (dev);
   	printk(KERN_ERR "aironet open failed \n");
   	return -1;
}


 int awc_close(struct net_device *dev)
{
	struct awc_private * priv = (struct awc_private *) dev->priv;

	DEBUG(2, "%s: closing device.\n", dev->name);

	netif_stop_queue (dev);
	
	awc_disable_MAC(dev);
	awc_queues_destroy(dev);
	
	awc_reset(dev);
	
	mdelay(10);
	
	AWC_LOCK_COMMAND_ISSUING(priv);

	MOD_DEC_USE_COUNT;

	AWC_UNLOCK_COMMAND_ISSUING(priv);
	 
	return 0;
}



/******************************		TX  RX STUFF	******************/



void awc_tx_timeout (struct net_device *dev)
{
	struct awc_private *priv = (struct awc_private *) dev->priv;
	struct awc_fid * fid;
	int cnt;
	unsigned long flags;

	DEBUG (2, "%s: awc_tx_timeout \n", dev->name);

	printk (KERN_NOTICE "%s: Transmit timed out , buffs %d %d, queues tx %d pp %d lrg %d sm %d  \n ",
	     dev->name, priv->tx_small_buffs_total, priv->tx_buffs_total,
		priv->tx_in_transmit.size, priv->tx_post_process.size,
		priv->tx_large_ready.size, priv->tx_small_ready.size);
	priv->stats.tx_errors++;

	save_flags(flags);
	cli();
	fid = priv->tx_in_transmit.head;
	cnt = 0;
	while (fid) { // removing all fids older that that
		if (jiffies - fid->transmit_start_time > (HZ)) {
			//      printk(KERN_ERR "%s staled tx_buff found, age %uld jiffies\n",dev->name,
			//              jiffies - fid->transmit_start_time );
			awc_fid_queue_remove (&priv->tx_in_transmit, fid);
			if (fid->u.tx.fid_size <= AWC_TX_ALLOC_SMALL_SIZE)
				awc_fid_queue_push_tail (&priv->tx_small_ready, fid);
			else
				awc_fid_queue_push_tail (&priv->tx_large_ready, fid);
		}
		fid = fid->next;
		if (cnt++ > 200) {
			printk ("bbb in awc_fid_queue\n");
			restore_flags(flags);
			return;
		};

	}
	restore_flags(flags);
	dev->trans_start = jiffies;
	netif_wake_queue (dev);
}


long long last_tx_q_hack;
int direction = 1;

int awc_start_xmit(struct sk_buff *skb, struct net_device *dev) {

	struct awc_private *priv = (struct awc_private *)dev->priv;
	int retval = 0;
//	unsigned long flags;

	DEBUG(2, "%s: awc_start_xmit \n",  dev->name);


	if (!dev) {
		DEBUG(1, " xmit dev=NULL, jiffie %ld \n",jiffies);
		return -1;
	};

	if (!skb) {
		DEBUG(1, " xmit skb=NULL, jiffie %ld \n",jiffies);
		return -1;
	};
	
//	if (test_and_set_bit( 0, (void *) &priv->tx_chain_active) ) {
//		netif_start_queue (dev);
//		return 1;
//	}

	dev->trans_start = jiffies;
	retval = awc_802_11_tx_find_path_and_post(dev,skb);
	priv->tx_chain_active = 0;
//	wake_up_interruptible(&priv->tx_chain_wait_queue);	

//	if (!dev->tbusy) dev_tint(dev);
	return retval;
}

void awc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	struct awc_private *priv;
	unsigned long flags;

//	if ((dev == NULL)) return;

	priv = (struct awc_private *)dev->priv;


	

	DEBUG(2, "%s: awc_interrupt \n",  dev->name);
	spin_lock_irqsave(&priv->interrupt_spinlock, flags);	  

	awc_interrupt_process(dev);

	spin_unlock_irqrestore(&priv->interrupt_spinlock, flags);	  
}



/************************	STATS, MULTICAST & STUFF  ****************/



 struct net_device_stats *awc_get_stats(struct net_device *dev)
{
	struct awc_private *priv = (struct awc_private *)dev->priv;
//        unsigned long flags;
//	int cnt = 0;
//	int unlocked_stats_in_interrupt=0;
	
	DEBUG(2, "%s: awc_get_stats \n",  dev->name);

	if (!netif_running(dev)) {
		return 0;			
	}
//	save_flags(flags);
//	cli();
	if (awc_full_stats)
		awc_readrid_dir(dev, &priv->rid_dir[9]);
//	restore_flags(flags);

	// the very following is the very wrong very probably
	if (awc_full_stats){
		priv->stats.rx_bytes		= priv->statistics.HostRxBytes;
		priv->stats.tx_bytes		= priv->statistics.HostTxBytes;
		priv->stats.rx_fifo_errors 	= priv->statistics.RxOverrunErr ;
		priv->stats.rx_crc_errors 	= priv->statistics.RxPlcpCrcErr + priv->statistics.RxMacCrcErr ;
		priv->stats.rx_frame_errors 	= priv->statistics.RxPlcpFormat ;
		priv->stats.rx_length_errors	=  priv->statistics.RxPlcpLength   ;
		priv->stats.rx_missed_errors	= priv->statistics.RxAged ;
		priv->stats.rx_over_errors	= priv->statistics.RxOverrunErr ;
			
		priv->stats.collisions 		= priv->statistics.TxSinColl;
		priv->stats.tx_aborted_errors 	= priv->statistics.TxAged ;
		priv->stats.tx_fifo_errors	= priv->statistics.HostTxFail ;
		priv->stats.tx_window_errors 	= priv->statistics.TxMulColl  ;
		priv->stats.tx_heartbeat_errors	= priv->statistics.DefersProt +priv->statistics.DefersEngy ;
		priv->stats.tx_carrier_errors	= priv->statistics.RetryLong +priv->statistics.RetryShort  ;
		priv->stats.multicast		= priv->statistics.HostRxMc;
	}


//	printk("rx_packets %d\n",priv->stats.rx_packets);
	return &(priv->stats);
}


int awc_change_mtu(struct net_device *dev, int new_mtu){

//	struct awc_private *priv = (struct awc_private *)dev->priv;
        unsigned long flags;

       if ((new_mtu < 256 ) || (new_mtu > 2312) || (max_mtu && new_mtu > max_mtu) )
                return -EINVAL;

	if (netif_running(dev)) {
		printk("PLEASE, ifconfig %s down for mtu change\n",dev->name);

	};
	if (dev->mtu != new_mtu) {
		save_flags(flags);
		cli();
		 netif_stop_queue(dev);
		 awc_disable_MAC(dev);
		restore_flags(flags); 
				
		awc_tx_dealloc(dev);
		dev->mtu = new_mtu;
		awc_tx_alloc(dev);
		awc_enable_MAC(dev);
		netif_start_queue(dev);

		printk("%s mtu has been changed to %d \n ",dev->name,dev->mtu);

	}

	return 0;

};


 void
awc_set_multicast_list(struct net_device *dev) {
//	int ioaddr = dev->base_addr;

/*	if (dev->flags & IFF_PROMISC)
		promisc
	else if (dev->mc_count || (dev->flags & IFF_ALLMULTI))
		allmulti
	else
		normal
		*/


}



int (* awc_proc_set_fun) (int);
int (* awc_proc_unset_fun) (int);


int awc_register_proc(int (*awc_proc_set_device)(int),int (*awc_proc_unset_device)(int)){

	AWC_ENTRY_EXIT_DEBUG("awc_register_proc");
	awc_proc_set_fun 	= awc_proc_set_device;
	awc_proc_unset_fun 	= awc_proc_unset_device;
	AWC_ENTRY_EXIT_DEBUG("exit");
	return 0;
};

int awc_unregister_proc(void){

	AWC_ENTRY_EXIT_DEBUG("awc_unregister_proc");
	
	awc_proc_set_fun        = NULL;
	awc_proc_unset_fun      = NULL;	                
	AWC_ENTRY_EXIT_DEBUG("exit");
	return 0;
};

static int aironet_core_init(void)
{
//	unsigned long flags;

		
	printk(KERN_INFO"%s", aironet4500_core_version);
	return 0;
	

}

static void aironet_core_exit(void)
{
	printk(KERN_INFO "aironet4500 unloading core module \n");

}
        
module_init(aironet_core_init);
module_exit(aironet_core_exit);

