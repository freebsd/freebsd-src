/* tdfx_pci.h -- Prototypes for tdfx device methods */
/* Copyright (C) 2000 by Coleman Kane <cokane@pohl.ececs.uc.edu>*/
#include <sys/proc.h>
#include <sys/conf.h>

/* Driver functions */
static int tdfx_probe(device_t dev);
static int tdfx_attach(device_t dev);
static int tdfx_setmtrr(device_t dev);
static int tdfx_clrmtrr(device_t dev);
static int tdfx_detach(device_t dev);
static int tdfx_shutdown(device_t dev);

/* CDEV file ops */
static d_open_t tdfx_open;
static d_close_t tdfx_close;
static d_mmap_t tdfx_mmap;
static d_ioctl_t tdfx_ioctl;

/* Card Queries */
static int tdfx_do_query(u_int cmd, struct tdfx_pio_data *piod);
static int tdfx_query_boards(void);
static int tdfx_query_fetch(u_int cmd, struct tdfx_pio_data *piod);
static int tdfx_query_update(u_int cmd, struct tdfx_pio_data *piod);

/* Card PIO funcs */
static int tdfx_do_pio(u_int cmd, struct tdfx_pio_data *piod);
static int tdfx_do_pio_wt(struct tdfx_pio_data *piod);
static int tdfx_do_pio_rd(struct tdfx_pio_data *piod);
