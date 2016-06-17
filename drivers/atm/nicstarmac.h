/******************************************************************************
 *
 * nicstarmac.h
 *
 * Header file for nicstarmac.c
 *
 ******************************************************************************/


typedef unsigned int virt_addr_t;

u_int32_t nicstar_read_eprom_status( virt_addr_t base );
void nicstar_init_eprom( virt_addr_t base );
void nicstar_read_eprom( virt_addr_t, u_int8_t, u_int8_t *, u_int32_t);
