#include "mailbox.h"

#include <sys/kernel.h>
#include <sys/malloc.h>

#define is_power_of_2(x)    ((x) != 0 && (((x) & ((x) - 1)) == 0))

MALLOC_DEFINE(M_MB, "pspat_mailbox", "IFFQ Mailbox Implementation");

int
pspat_mb_new(const char *name, unsigned long entries, unsigned long line_size, struct pspat_mailbox **m) {
    int err;
    *m = malloc(pspat_mb_size(entries), M_MB, M_WAITOK | M_ZERO);
    if (*m == NULL) {
        return -ENOMEM;
    }

    err = pspat_mb_init(*m, name, entries, line_size);
    if (err) {
        free(*m, M_MB);
        return err;
    }

    return 0;
}

int
pspat_mb_init(struct pspat_mailbox *m, const char *name, unsigned long entries, unsigned long line_size)
{
    unsigned long entries_per_line = line_size / sizeof(void*);

    if(!is_power_of_2(entries) ||
       !is_power_of_2(line_size) ||
       entries * sizeof(void *) <= 2 * line_size ||
       line_size < sizeof(void *)) {
        return -EINVAL;
    }

    snprintf(m->name, PSPAT_MB_NAMSZ, "%s", name);
    m->name[PSPAT_MB_NAMSZ - 1] = '\0';

    m->entries_per_line = entries_per_line;
    m->line_mask = ~(entries_per_line - 1);
    m->entry_mask = entries - 1;

#ifdef PSPAT_MB_DEBUG
    printf("PSPAT: mb %p %s: entries per line %lu, line mask 0x%016lx, entry mask 0x%016lx\n",
            m, m->name, m->entries_per_line, m->line_mask, m->entry_mask);
#endif

    m->cons_clear = 0;
    m->cons_read = m->entries_per_line;
    m->prod_write = m->entries_per_line;
    m->prod_check = m->entries_per_line;

    unsigned int initial_clear = m->cons_clear;

    // Fill the first cacheline with a non-null garbage value;
    while(initial_clear != m->cons_read) {
        m->q[initial_clear & m->entry_mask] = (void *)1;
        initial_clear ++;
    }

    entry_init(&m->entry);
    m->entry.mb = m;

    return 0;
}

void
pspat_mb_delete(struct pspat_mailbox *m)
{
#ifdef PSPAT_MB_DEBUG
    printf("PSPAT: Deleting mailbox %s\n", m->name);
#endif
    free(m, M_MB);
}

void
pspat_mb_dump_state(struct pspat_mailbox *m) {
    printf("PSPAT MB %s: Clear: %lu, Read: %lu, Write: %lu, Check: %lu\n",
            m->name, m->cons_clear, m->cons_read, m->prod_write, m->prod_check);
}
