/*
 *  ncpsign_kernel.h
 *
 *  Arne de Bruijn (arne@knoware.nl), 1997
 *
 */
 
#ifndef _NCPSIGN_KERNEL_H
#define _NCPSIGN_KERNEL_H

#include <linux/ncp_fs.h>

void sign_packet(struct ncp_server *server, int *size);

#endif
