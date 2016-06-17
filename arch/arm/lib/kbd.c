#include <linux/config.h>
#include <linux/kd.h>

int  (*k_setkeycode)(unsigned int, unsigned int);
int  (*k_getkeycode)(unsigned int);
int  (*k_translate)(unsigned char, unsigned char *, char);
char (*k_unexpected_up)(unsigned char);
void (*k_leds)(unsigned char);

#ifdef CONFIG_MAGIC_SYSRQ
int k_sysrq_key;
unsigned char *k_sysrq_xlate;
#endif
