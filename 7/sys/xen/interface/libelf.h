/******************************************************************************
 * libelf.h
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __XC_LIBELF__
#define __XC_LIBELF__ 1

#if defined(__i386__) || defined(__x86_64__) || defined(__ia64__)
#define XEN_ELF_LITTLE_ENDIAN
#else
#error define architectural endianness
#endif

#undef ELFSIZE
#include "elfnote.h"
#include "elfstructs.h"
#include "features.h"

/* ------------------------------------------------------------------------ */

typedef union {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
} elf_ehdr;

typedef union {
    Elf32_Phdr e32;
    Elf64_Phdr e64;
} elf_phdr;

typedef union {
    Elf32_Shdr e32;
    Elf64_Shdr e64;
} elf_shdr;

typedef union {
    Elf32_Sym e32;
    Elf64_Sym e64;
} elf_sym;

typedef union {
    Elf32_Rel e32;
    Elf64_Rel e64;
} elf_rel;

typedef union {
    Elf32_Rela e32;
    Elf64_Rela e64;
} elf_rela;

typedef union {
    Elf32_Note e32;
    Elf64_Note e64;
} elf_note;

struct elf_binary {
    /* elf binary */
    const char *image;
    size_t size;
    char class;
    char data;

    const elf_ehdr *ehdr;
    const char *sec_strtab;
    const elf_shdr *sym_tab;
    const char *sym_strtab;

    /* loaded to */
    char *dest;
    uint64_t pstart;
    uint64_t pend;
    uint64_t reloc_offset;

    uint64_t bsd_symtab_pstart;
    uint64_t bsd_symtab_pend;

#ifndef __XEN__
    /* misc */
    FILE *log;
#endif
    int verbose;
};

/* ------------------------------------------------------------------------ */
/* accessing elf header fields                                              */

#ifdef XEN_ELF_BIG_ENDIAN
# define NATIVE_ELFDATA ELFDATA2MSB
#else
# define NATIVE_ELFDATA ELFDATA2LSB
#endif

#define elf_32bit(elf) (ELFCLASS32 == (elf)->class)
#define elf_64bit(elf) (ELFCLASS64 == (elf)->class)
#define elf_msb(elf)   (ELFDATA2MSB == (elf)->data)
#define elf_lsb(elf)   (ELFDATA2LSB == (elf)->data)
#define elf_swap(elf)  (NATIVE_ELFDATA != (elf)->data)

#define elf_uval(elf, str, elem)                                        \
    ((ELFCLASS64 == (elf)->class)                                       \
     ? elf_access_unsigned((elf), (str),                                \
                           offsetof(typeof(*(str)),e64.elem),           \
                           sizeof((str)->e64.elem))                     \
     : elf_access_unsigned((elf), (str),                                \
                           offsetof(typeof(*(str)),e32.elem),           \
                           sizeof((str)->e32.elem)))

#define elf_sval(elf, str, elem)                                        \
    ((ELFCLASS64 == (elf)->class)                                       \
     ? elf_access_signed((elf), (str),                                  \
                         offsetof(typeof(*(str)),e64.elem),             \
                         sizeof((str)->e64.elem))                       \
     : elf_access_signed((elf), (str),                                  \
                         offsetof(typeof(*(str)),e32.elem),             \
                         sizeof((str)->e32.elem)))

#define elf_size(elf, str)                              \
    ((ELFCLASS64 == (elf)->class)                       \
     ? sizeof((str)->e64) : sizeof((str)->e32))

uint64_t elf_access_unsigned(struct elf_binary *elf, const void *ptr,
                             uint64_t offset, size_t size);
int64_t elf_access_signed(struct elf_binary *elf, const void *ptr,
                          uint64_t offset, size_t size);

uint64_t elf_round_up(struct elf_binary *elf, uint64_t addr);

/* ------------------------------------------------------------------------ */
/* xc_libelf_tools.c                                                        */

int elf_shdr_count(struct elf_binary *elf);
int elf_phdr_count(struct elf_binary *elf);

const elf_shdr *elf_shdr_by_name(struct elf_binary *elf, const char *name);
const elf_shdr *elf_shdr_by_index(struct elf_binary *elf, int index);
const elf_phdr *elf_phdr_by_index(struct elf_binary *elf, int index);

const char *elf_section_name(struct elf_binary *elf, const elf_shdr * shdr);
const void *elf_section_start(struct elf_binary *elf, const elf_shdr * shdr);
const void *elf_section_end(struct elf_binary *elf, const elf_shdr * shdr);

const void *elf_segment_start(struct elf_binary *elf, const elf_phdr * phdr);
const void *elf_segment_end(struct elf_binary *elf, const elf_phdr * phdr);

const elf_sym *elf_sym_by_name(struct elf_binary *elf, const char *symbol);
const elf_sym *elf_sym_by_index(struct elf_binary *elf, int index);

const char *elf_note_name(struct elf_binary *elf, const elf_note * note);
const void *elf_note_desc(struct elf_binary *elf, const elf_note * note);
uint64_t elf_note_numeric(struct elf_binary *elf, const elf_note * note);
const elf_note *elf_note_next(struct elf_binary *elf, const elf_note * note);

int elf_is_elfbinary(const void *image);
int elf_phdr_is_loadable(struct elf_binary *elf, const elf_phdr * phdr);

/* ------------------------------------------------------------------------ */
/* xc_libelf_loader.c                                                       */

int elf_init(struct elf_binary *elf, const char *image, size_t size);
#ifdef __XEN__
void elf_set_verbose(struct elf_binary *elf);
#else
void elf_set_logfile(struct elf_binary *elf, FILE * log, int verbose);
#endif

void elf_parse_binary(struct elf_binary *elf);
void elf_load_binary(struct elf_binary *elf);

void *elf_get_ptr(struct elf_binary *elf, unsigned long addr);
uint64_t elf_lookup_addr(struct elf_binary *elf, const char *symbol);

void elf_parse_bsdsyms(struct elf_binary *elf, uint64_t pstart); /* private */

/* ------------------------------------------------------------------------ */
/* xc_libelf_relocate.c                                                     */

int elf_reloc(struct elf_binary *elf);

/* ------------------------------------------------------------------------ */
/* xc_libelf_dominfo.c                                                      */

#define UNSET_ADDR          ((uint64_t)-1)

enum xen_elfnote_type {
    XEN_ENT_NONE = 0,
    XEN_ENT_LONG = 1,
    XEN_ENT_STR  = 2
};

struct xen_elfnote {
    enum xen_elfnote_type type;
    const char *name;
    union {
        const char *str;
        uint64_t num;
    } data;
};

struct elf_dom_parms {
    /* raw */
    const char *guest_info;
    const void *elf_note_start;
    const void *elf_note_end;
    struct xen_elfnote elf_notes[XEN_ELFNOTE_MAX + 1];
  
    /* parsed */
    char guest_os[16];
    char guest_ver[16];
    char xen_ver[16];
    char loader[16];
    int pae;
    int bsd_symtab;
    uint64_t virt_base;
    uint64_t virt_entry;
    uint64_t virt_hypercall;
    uint64_t virt_hv_start_low;
    uint64_t elf_paddr_offset;
    uint32_t f_supported[XENFEAT_NR_SUBMAPS];
    uint32_t f_required[XENFEAT_NR_SUBMAPS];

    /* calculated */
    uint64_t virt_offset;
    uint64_t virt_kstart;
    uint64_t virt_kend;
};

static inline void elf_xen_feature_set(int nr, uint32_t * addr)
{
    addr[nr >> 5] |= 1 << (nr & 31);
}
static inline int elf_xen_feature_get(int nr, uint32_t * addr)
{
    return !!(addr[nr >> 5] & (1 << (nr & 31)));
}

int elf_xen_parse_features(const char *features,
                           uint32_t *supported,
                           uint32_t *required);
int elf_xen_parse_note(struct elf_binary *elf,
                       struct elf_dom_parms *parms,
                       const elf_note *note);
int elf_xen_parse_guest_info(struct elf_binary *elf,
                             struct elf_dom_parms *parms);
int elf_xen_parse(struct elf_binary *elf,
                  struct elf_dom_parms *parms);

#endif /* __XC_LIBELF__ */
