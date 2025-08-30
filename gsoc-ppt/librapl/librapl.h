#ifndef _LIBRAPL_H_
#define _LIBRAPL_H_

__BEGIN_DECLS

#define NUM_RAPL_DOMAINS 5
char rapl_domain_names[NUM_RAPL_DOMAINS][30];
int detect_cpu(void);
int detect_packages(void);
//int rapl_msr(int core, int cpu_model, int delay, struct energy_outputs *eo);
__END_DECLS

#endif
