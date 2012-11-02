/*
 * Copyright (c) 2009 Felix Obenhuber
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sockettrace sniffing API implementation for Linux platform
 * By Felix Obenhuber <felix@obenhuber.de>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libusb-1.0/libusb.h>

#include "pcap-int.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


#define CANUSB_IFACE "canusb"

#define CANUSB_VID 0x0403
#define CANUSB_PID 0x8990

#define USE_THREAD 1

#if USE_THREAD == 0
#include <signal.h>
#endif


/* forward declaration */
static int canusb_activate(pcap_t *);
static int canusb_read_linux(pcap_t *, int , pcap_handler , u_char *);
static int canusb_inject_linux(pcap_t *, const void *, size_t);
static int canusb_setfilter_linux(pcap_t *, struct bpf_program *);
static int canusb_setdirection_linux(pcap_t *, pcap_direction_t);
static int canusb_stats_linux(pcap_t *, struct pcap_stat *);

struct CAN_Msg
{
    uint32_t timestamp;
    uint32_t id;
    uint32_t length;
    uint8_t data[8];
};

struct canusb_t
{
  libusb_context *ctx;
  libusb_device_handle *dev;
  char* src;
  pthread_t worker;
  int rdpipe, wrpipe;
  volatile int* loop;
};

static struct canusb_t canusb;
static volatile int loop;



int canusb_platform_finddevs(pcap_if_t **alldevsp, char *err_str)
{
    libusb_context *fdctx;
    libusb_device** devs;
    unsigned char sernum[65];
    unsigned char buf[96];
    int cnt, i;
    
    libusb_init(&fdctx);
        
    cnt = libusb_get_device_list(fdctx,&devs);

    for(i=0;i<cnt;i++)
    {
        int ret;
        // Check if this device is interesting.
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(devs[i],&desc);

        if ((desc.idVendor != CANUSB_VID) || (desc.idProduct != CANUSB_PID)) 
          continue; //It is not, check next device
          
        //It is!
        libusb_device_handle *dh = NULL;

        if (ret = libusb_open(devs[i],&dh) == 0)
        {
          	char dev_name[30];
	          char dev_descr[50]; 
            int n = libusb_get_string_descriptor_ascii(dh,desc.iSerialNumber,sernum,64);
            sernum[n] = 0;

          	snprintf(dev_name, 30, CANUSB_IFACE"%s", sernum);
          	snprintf(dev_descr, 50, "CanUSB [%s]", sernum);
            
            libusb_close(dh);
            
            if (pcap_add_if(alldevsp, dev_name, 0, dev_descr, err_str) < 0)
            {
              libusb_free_device_list(devs,1);
              return -1;
            }
        }
    }

    libusb_free_device_list(devs,1);
    libusb_exit(fdctx);
    return 0;
}

static libusb_device_handle* canusb_opendevice(struct libusb_context *ctx, char* devserial)
{
    libusb_device_handle* dh;
    libusb_device** devs;
    unsigned char serial[65];
    int cnt,i,n;
    
    cnt = libusb_get_device_list(ctx,&devs);

    for(i=0;i<cnt;i++)
    {    
        // Check if this device is interesting.
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(devs[i],&desc);

        if ((desc.idVendor != CANUSB_VID) || (desc.idProduct != CANUSB_PID))
          continue;
          
        //Found one!
        libusb_device_handle *dh = NULL;

        if (libusb_open(devs[i],&dh) != 0) continue;

        n = libusb_get_string_descriptor_ascii(dh,desc.iSerialNumber,serial,64);
        serial[n] = 0;

        if ((devserial) && (strcmp(serial,devserial) != 0))
        {
            libusb_close(dh);
            continue;
        }

        if ((libusb_kernel_driver_active(dh,0)) && (libusb_detach_kernel_driver(dh,0) != 0))
        {
            libusb_close(dh);
            continue;
        }

        if (libusb_set_configuration(dh,1) != 0)
        {
            libusb_close(dh);
            continue;
        }

        if (libusb_claim_interface(dh,0) != 0)
        {
            libusb_close(dh);
            continue;
        }
        
        //Fount it!
        libusb_free_device_list(devs,1);        
        return dh;
    }

    libusb_free_device_list(devs,1);
    return NULL;
}


pcap_t *
canusb_create(const char *device, char *ebuf)
{ 
  pcap_t* p;
  		
  libusb_init(&canusb.ctx);
  
	p = pcap_create_common(device, ebuf);
	if (p == NULL)
		return (NULL);
		
  memset(&canusb, 0x00, sizeof(canusb));
		
	
	p->activate_op = canusb_activate;
	
	canusb.src = strdup(p->opt.source);
	return (p);
}


static void* canusb_capture_thread(struct canusb_t *canusb)
{
  struct libusb_context *ctx;
  libusb_device_handle *dev;

  int i, n;  
  struct 
  {
    uint8_t rxsz, txsz;
  } status;
  
  libusb_init(&ctx);
  
  char *serial = canusb->src + strlen(CANUSB_IFACE);  
  dev = canusb_opendevice(ctx, serial);
  
  fcntl(canusb->wrpipe, F_SETFL, O_NONBLOCK);  

  while(*canusb->loop)
  {
    int sz, ret;
    struct CAN_Msg msg;
    
    libusb_interrupt_transfer(dev, 0x81, (unsigned char*)&status, sizeof(status), &sz, 100);
    //HACK!!!!! -> drop buffered data, read new one by reading twice.        
    ret = libusb_interrupt_transfer(dev, 0x81, (unsigned char*)&status, sizeof(status), &sz, 100);                                   

    for(i = 0; i<status.rxsz; i++)
    {
      libusb_bulk_transfer(dev, 0x85, (unsigned char*)&msg, sizeof(msg), &sz, 100);      
      n = write(canusb->wrpipe, &msg, sizeof(msg));
    }

  }
  
  libusb_close(dev);
  libusb_exit(ctx);
  
  return NULL;
}

static int canusb_startcapture(struct canusb_t* this)
{
  int pipefd[2];

  if (pipe(pipefd) == -1) return -1;

  canusb.rdpipe = pipefd[0];
  canusb.wrpipe = pipefd[1];
  canusb.loop = &loop;

  loop = 1;  
  pthread_create(&this->worker, NULL, canusb_capture_thread, &canusb);

  return canusb.rdpipe;
}

static void canusb_clearbufs(struct canusb_t* this)
{
        unsigned char cmd[16];
        int al;

        cmd[0] = 1;  //Empty incoming buffer
        cmd[1] = 1;  //Empty outgoing buffer
        cmd[3] = 0;  //Not a write to serial number
        memset(&cmd[4],0,16-4);
        
        libusb_interrupt_transfer(this->dev, 0x1,cmd,16,&al,100);
}


static void canusb_close(pcap_t* handle)
{
  loop = 0;
  pthread_join(canusb.worker, NULL);

  if (canusb.dev)
  {
    libusb_close(canusb.dev);
    canusb.dev = NULL;    
  }    
}



static int canusb_activate(pcap_t* handle)
{
	handle->read_op = canusb_read_linux;

	handle->inject_op = canusb_inject_linux;
	handle->setfilter_op = canusb_setfilter_linux;
	handle->setdirection_op = canusb_setdirection_linux;
	handle->getnonblock_op = pcap_getnonblock_fd;
	handle->setnonblock_op = pcap_setnonblock_fd;
	handle->stats_op = canusb_stats_linux;
	handle->cleanup_op = canusb_close;

	/* Initialize some components of the pcap structure. */
	handle->bufsize = 32;
	handle->offset = 8;
	handle->linktype = DLT_CAN_SOCKETCAN;
	handle->set_datalink_op = NULL;

  char* serial = handle->opt.source + strlen("canusb");

  canusb.dev = canusb_opendevice(canusb.ctx,serial);
  if (!canusb.dev)
  {
  	snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "Can't open USB Device:");  
   	return PCAP_ERROR;
 	}

  canusb_clearbufs(&canusb);

  handle->fd = canusb_startcapture(&canusb);
	handle->selectable_fd = handle->fd;
			
	return 0;
}




static int
canusb_read_linux(pcap_t *handle, int max_packets, pcap_handler callback, u_char *user)
{
  static struct timeval firstpacket = { -1, -1};
  
  int msgsent = 0;
  int i = 0;
  struct CAN_Msg msg;
	struct pcap_pkthdr pkth;
  
  while(i < max_packets)
  {
    usleep(10 * 1000);
    int n = read(handle->fd, &msg, sizeof(msg));
    if (n <= 0) break;
    pkth.caplen = pkth.len = n;
    pkth.caplen -= 4;
    pkth.caplen -= 8 - msg.length;
    
    if ((firstpacket.tv_sec == -1) && (firstpacket.tv_usec == -1))
      gettimeofday(&firstpacket, NULL);
      
    pkth.ts.tv_usec = firstpacket.tv_usec + (msg.timestamp % 100) * 10000;
    pkth.ts.tv_sec = firstpacket.tv_usec + (msg.timestamp / 100);
    if (pkth.ts.tv_usec > 1000000)
    {
      pkth.ts.tv_usec -= 1000000;
      pkth.ts.tv_sec++;
    }

    callback(user, &pkth, (void*)&msg.id);
    i++;
  }
  
  return i;
}


static int
canusb_inject_linux(pcap_t *handle, const void *buf, size_t size)
{
	/* not yet implemented */
	snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "inject not supported on canusb devices");
	return (-1);
}


static int
canusb_stats_linux(pcap_t *handle, struct pcap_stat *stats)
{
	/* not yet implemented */
	stats->ps_recv = 0;			 /* number of packets received */
	stats->ps_drop = 0;			 /* number of packets dropped */
	stats->ps_ifdrop = 0;		 /* drops by interface -- only supported on some platforms */
	return 0;
}


static int
canusb_setfilter_linux(pcap_t *p, struct bpf_program *fp)
{
	/* not yet implemented */
	return 0;
}


static int
canusb_setdirection_linux(pcap_t *p, pcap_direction_t d)
{
	/* no support for PCAP_D_OUT */
	if (d == PCAP_D_OUT)
	{
		snprintf(p->errbuf, sizeof(p->errbuf),
			"Setting direction to PCAP_D_OUT is not supported on this interface");
		return -1;
	}

	p->direction = d;

	return 0;
}


/* eof */
