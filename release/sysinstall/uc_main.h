/***************************************************
 * file: userconfig/uc_main.h
 *
 * Copyright (c) 1996 Eric L. Hernes (erich@rrnet.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: uc_main.h,v 1.1 1996/10/03 06:01:42 jkh Exp $
 */

#define ISA_BIOTAB 0  
#define ISA_TTYTAB 1
#define ISA_NETTAB 2
#define ISA_NULLTAB 3
#define ISA_WDCTAB 4  
#define ISA_FDCTAB 5
#define EISA_SET 6
#define EISA_LIST 7
#define PCI_SET 8
#define SCSI_LIST 9
#define SCSI_BUSSES 10
#define SCSI_CINIT 11
#define SCSI_DINIT 12
#define SCSI_TINIT 13
/* symbols + the null terminator */
#define NSYMBOLS 15

struct kernel {
  int     fd;    /* file descriptor for the kernel image, either a binary or /dev/kmem */
  caddr_t core;  /* either the mmap()ed kernel image, or a scratch area */
  u_int   size;  /* size of the object at ->core */
  int    incore; /* true if the kernel is running */
#ifdef UC_PRIVATE
  struct nlist *nl; /* the symbol table */
#else
  void *nl;
#endif
  struct uc_isa *isa_devp; /* pointer to the isa devices (if any) */
  struct uc_eisa *eisa_devp; /* pointer to the eisa devices (if any) */
  struct uc_pci *pci_devp; /* pointer to the pci devices (if any) */
  struct uc_scsi *scsi_devp; /* pointer to the scsi devices (if any) */
  struct uc_scsibus *scsibus_devp; /* internal pointer to scsibus wirings */
};

struct uc_isa {
  char   *device;
  u_short	port;
  u_short	irq;
  short	drq;
  u_int iomem;
  int	iosize;
  int	flags;
  int   alive;
  int	enabled;
#ifdef UC_PRIVATE
  struct isa_device *idp;
#else
  void *idp;
#endif
  int modified;
};

struct uc_pci {
  char   *device;
};

struct uc_eisa {
  char *device;
  char *full_name;
};

struct uc_scsibus {
  int bus_no;
  int unit;
  char *driver;
#ifdef UC_PRIVATE
  struct scsi_ctlr_config *config;
#else
  void *config;
#endif
};

struct uc_scsi {
  char *device;
  char *adapter;
  u_short target;
  u_short lun;
  char *desc;
#ifdef UC_PRIVATE
  struct scsi_device_config *config;
#else
  void *config;
#endif
  int modified;
};

/* nearly everything useful returns a list */

struct list {
  int ac;
  char **av;
};

/* prototypes */

/* uc_main.c */
/* these are really the only public ones */
struct kernel *uc_open(char *name);
int uc_close(struct kernel *kern, int writeback);
struct list *uc_getdev(struct kernel *kern, char *dev);

/* uc_isa.c */
void get_isa_info(struct kernel *kp);
struct list *get_isa_devlist(struct kernel *kp);
struct list *get_isa_device(struct uc_isa *ip);
int isa_setdev(struct kernel *kp, struct list *list);
void isa_free(struct kernel *kp, int writeback);

/* uc_eisa.c */
void get_eisa_info(struct kernel *kp);
struct list *get_eisa_devlist(struct kernel *kp);
struct list *get_eisa_device(struct uc_eisa *ep);
void eisa_free(struct kernel *kp, int writeback);

/* uc_pci.c */
void get_pci_info(struct kernel *kp);
struct list *get_pci_devlist(struct kernel *kp);
struct list *get_pci_device(struct uc_pci *pp);
void pci_free(struct kernel *kp, int writeback);

/* uc_scsi.c */
void get_scsi_info(struct kernel *kp);
struct list *get_scsi_devlist(struct kernel *kp);
struct list *get_scsi_device(struct uc_scsi *sp);
int scsi_setdev(struct kernel *kp, struct list *list);
void scsi_free(struct kernel *kp, int writeback);

/* uc_kmem.c */
u_int kv_to_u(struct kernel *kp, u_int adr, u_int size);
u_int kv_dref_p(struct kernel *kp, u_int adr);
u_int kv_dref_t(struct kernel *kp, u_int adr);

/* uc_list.c */
struct list *list_new(void);
void list_append(struct list *list, char *item);
void list_print(struct list *list, char *separator);
void list_destroy(struct list *list);

/* end of userconfig/uc_main.h */
