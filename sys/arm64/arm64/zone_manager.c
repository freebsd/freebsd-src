#include <machine/zone_manager.h>
#include <vm/uma.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <sys/secure_memory_heap.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/pcpu.h>
#include <machine/zone_manager_routines_priv.h>
#include <machine/armreg.h>

#define WATCHPOINT_MAX_SZ_POWER (31)
#define WATCHPOINT_MAX_SZ_BYTES (1LU << WATCHPOINT_MAX_SZ_POWER)
#define TAG "[zone_manager] "

extern void *__zm_ro_begin;
extern void *__zm_ro_end;

static void * dispatch_test(void *aux);

ZM_RO zm_globals_s *zm_globals = NULL;
ZM_RO zm_pcpu_s *zm_pcpus = NULL;
ZM_RO u_int32_t zm_pcpu_count;
const zm_zone_dispatch_fcn_t zm_dispatch_functions[] = {
    /* ZONE_STATE_PMAP */           dispatch_test,
    /* ZONE_STATE_CAPABILITIES */   dispatch_test
};

_Static_assert(
    sizeof(zm_dispatch_functions) / sizeof(*zm_dispatch_functions) 
        == ZONE_STATE_POS_COUNT, 
    "Dispatch function count"
);

struct secure_memory_heap pmap_heap;

static void
configure_watchpoints_smp(void *aux) {
    printf(TAG "go for watchpoint config on CPU %d\n", curcpu);
    sched_pin();
    zm_init_watchpoint_config_s configs[4] = {
        /* ZONE_STATE_PMAP */
        {
            .wcr = DBGWCR_MASK_X(0b11111LU)
                    | DBGWCR_LSC_ST
                    | DBGWCR_PAC_EL1
                    | DBGWCR_EN,
            /* 
            We stuff the secure PCPU pointer in the breakpoint because it
            keeps us from having to locate and authenticate it manually (yay
            speed!). This is fine because the mask for the watchpoint ignores
            all of these bits as the PCPU pointer is obviously under the
            watchpoint. Since the wvr write gets unmapped later, these registers
            are safe and durable.
            */
            .wvr = (u_int64_t)(zm_pcpus + curcpu)
        },
        {
            .wcr = 0,
            .wvr = 0
        },
        {
            .wcr = 0,
            .wvr = 0
        },
        {
            .wcr = 0,
            .wvr = 0
        },
    };
    
    zm_init_debug(configs);

    sched_unpin();
}

static void
init_startup(void *arg __unused) {
    int i;
    vm_offset_t pmap_zone_base = 0;
    printf(TAG "awake :)\n");
    printf(TAG "zm_ro %p->%p\n", &__zm_ro_begin, &__zm_ro_end);

    pmap_zone_base = 
        kva_xalloc(WATCHPOINT_MAX_SZ_BYTES, WATCHPOINT_MAX_SZ_BYTES);
    printf(
        TAG "pmap_zone: %lx->%lx\n", 
        pmap_zone_base, pmap_zone_base + WATCHPOINT_MAX_SZ_BYTES
    );

    smh_init(
        &pmap_heap, "pmap_heap", 
        pmap_zone_base, WATCHPOINT_MAX_SZ_BYTES
    );

    zm_pcpu_count = mp_ncpus;
    zm_globals = smh_calloc(&pmap_heap, sizeof(zm_globals_s), 1);
    zm_pcpus = smh_calloc(&pmap_heap, sizeof(zm_pcpu_s), mp_ncpus);

    printf(TAG "zm_globals = %p, zm_pcpus = %p\n", zm_globals, zm_pcpus);
    CPU_FOREACH(i) {
        struct pcpu *pcpu = NULL;
        zm_pcpu_s *zm_pcpu = NULL;

        pcpu = pcpu_find(i);
        zm_pcpu = zm_pcpus + i;

        zm_pcpu->state = ZONE_STATE_NONE;
        zm_pcpu->mpidr = pcpu->pc_mpidr;
        zm_pcpu->zone_stacks[ZONE_STATE_PMAP] = 
            (uintptr_t)smh_page_alloc(&pmap_heap, 1) + PAGE_SIZE;
        printf(TAG "Register CPU nr=%d as MPIDR=%u\n", i, pcpu->pc_mpidr);
    }

    /* 
    Configure the debug registers on all CPUs
    TODO: Probably need to move this elsewhere (cpu init?) since if a CPU power
    gates it will not re-execute this.
    */
    smp_rendezvous(NULL, configure_watchpoints_smp, NULL, NULL);

    printf("Entering pmap zone...\n");
    sched_pin();
    void *result = zm_zone_enter(ZONE_STATE_PMAP, (void *)0x41414141);
    sched_unpin();
    printf("pmap returned %p\n", result);  
}

/* Start after SMP is up so that we can rendezvous. */
SYSINIT(init_startup, SI_SUB_SMP, SI_ORDER_SECOND, init_startup, NULL);

static void * dispatch_test(void *aux) {
    printf("[pmap] Hello from pmap! Got aux=%p\n", aux);
    return (void *)0x42424242;
}