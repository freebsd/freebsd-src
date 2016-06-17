#ifndef _ASM_GENERIC_PGTABLE_H
#define _ASM_GENERIC_PGTABLE_H

static inline int ptep_test_and_clear_young(pte_t *ptep)
{
	pte_t pte = *ptep;
	if (!pte_young(pte))
		return 0;
	set_pte(ptep, pte_mkold(pte));
	return 1;
}

static inline int ptep_test_and_clear_dirty(pte_t *ptep)
{
	pte_t pte = *ptep;
	if (!pte_dirty(pte))
		return 0;
	set_pte(ptep, pte_mkclean(pte));
	return 1;
}

static inline pte_t ptep_get_and_clear(pte_t *ptep)
{
	pte_t pte = *ptep;
	pte_clear(ptep);
	return pte;
}

static inline void ptep_set_wrprotect(pte_t *ptep)
{
	pte_t old_pte = *ptep;
	set_pte(ptep, pte_wrprotect(old_pte));
}

static inline void ptep_mkdirty(pte_t *ptep)
{
	pte_t old_pte = *ptep;
	set_pte(ptep, pte_mkdirty(old_pte));
}

#define pte_same(A,B)	(pte_val(A) == pte_val(B))

#endif /* _ASM_GENERIC_PGTABLE_H */
