#ifndef __P2SB_H__
#define __P2SB_H__

void p2sb_lock(device_t dev);
void p2sb_unlock(device_t dev);

uint32_t p2sb_port_read_4(device_t dev, uint8_t port, uint32_t reg);
void p2sb_port_write_4(device_t dev, uint8_t port, uint32_t reg, uint32_t val);
int p2sb_get_port(device_t dev, int unit);

#endif /* __P2SB_H__ */
