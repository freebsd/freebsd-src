extern nasid_t master_nasid;

extern cnodeid_t get_compact_nodeid(void);
extern void hub_rtc_init(cnodeid_t);
extern void cpu_time_init(void);
extern void per_cpu_init(void);
extern void install_cpuintr(int cpu);
extern void install_tlbintr(int cpu);
extern void setup_replication_mask(int);
extern void replicate_kernel_text(int);
extern pfn_t node_getfirstfree(cnodeid_t);
