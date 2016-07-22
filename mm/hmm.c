/*
 * Copyright 2013 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Authors: Jérôme Glisse <jglisse@redhat.com>
 */
/*
 * Refer to include/linux/hmm.h for informations about heterogeneous memory
 * management or HMM for short.
 */
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/hmm.h>

static int hmm_init(struct hmm *hmm, struct mm_struct *mm)
{
	hmm->mm = mm;
	kref_init(&hmm->kref);
	spin_lock_init(&hmm->lock);
	INIT_LIST_HEAD(&hmm->migrates);
	return 0;
}

struct hmm *hmm_register(struct mm_struct *mm)
{
	struct hmm *hmm;

	spin_lock(&mm->page_table_lock);
again:
	if (!mm->hmm || !kref_get_unless_zero(&mm->hmm->kref)) {
		struct hmm *old;

		old = mm->hmm;
		spin_unlock(&mm->page_table_lock);

		hmm = kmalloc(sizeof(*hmm), GFP_KERNEL);
		if (!hmm)
			return NULL;
		if (hmm_init(hmm, mm)) {
			kfree(hmm);
			return NULL;
		}

		spin_lock(&mm->page_table_lock);
		if (old != mm->hmm) {
			kfree(hmm);
			goto again;
		}
		mm->hmm = hmm;
	} else
		hmm = mm->hmm;
	spin_unlock(&mm->page_table_lock);

	return hmm;
}

static void hmm_release(struct kref *kref)
{
	struct hmm *hmm;

	hmm = container_of(kref, struct hmm, kref);
	spin_lock(&hmm->mm->page_table_lock);
	if (hmm->mm->hmm == hmm)
		hmm->mm->hmm = NULL;
	spin_unlock(&hmm->mm->page_table_lock);
	kfree(hmm);
}

void hmm_put(struct hmm *hmm)
{
	kref_put(&hmm->kref, &hmm_release);
}


static int hmm_walk_pmd(struct vm_area_struct *vma,
			hmm_walk_hole_t walk_hole,
			hmm_walk_huge_t walk_huge,
			hmm_walk_pte_t walk_pte,
			struct gpt_walk *walk,
			unsigned long addr,
			unsigned long end,
			void *private,
			pud_t *pudp)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long next;
	pmd_t *pmdp;

	/*
	 * As we are holding mmap_sem in read mode we know pmd can't morph into
	 * a huge one so it is safe to map pte and go over them.
	 */
	pmdp = pmd_offset(pudp, addr);
	do {
		spinlock_t *gtl, *ptl;
		unsigned long cend;
		pte_t *ptep;
		gte_t *gtep;
		int ret;

again:
		next = pmd_addr_end(addr, end);

		if (pmd_none(*pmdp)) {
			if (walk_hole) {
				ret = walk_hole(vma, walk, addr,
						next, private);
				if (ret)
					return ret;
			}
			continue;
		}

		/*
		 * TODO support THP, issue lie with mapcount and refcount to
		 * determine if page is pin or not.
		 */
		if (pmd_trans_huge(*pmdp)) {
			if (!pmd_trans_splitting(*pmdp))
				split_huge_page_pmd_mm(mm, addr, pmdp);
			goto again;
		}

		if (pmd_none_or_trans_huge_or_clear_bad(pmdp))
			goto again;

		do {
			gtep = gpt_walk_populate(walk, addr);
			if (!gtep)
				return -ENOMEM;
			gtl = gpt_walk_gtd_lock_ptr(walk, 0);
			cend = min(next, walk->end);

			ptl = pte_lockptr(mm, pmdp);
			ptep = pte_offset_map(pmdp, addr);
			ret = walk_pte(vma, walk, addr, cend, ptl,
				       gtl, ptep, gtep, private);
			pte_unmap(ptep);
			if (ret)
				return ret;

			addr = cend;
			cend = next;
		} while (addr < next);

	} while (pmdp++, addr = next, addr != end);

	return 0;
}

static int hmm_walk_pud(struct vm_area_struct *vma,
			hmm_walk_hole_t walk_hole,
			hmm_walk_huge_t walk_huge,
			hmm_walk_pte_t walk_pte,
			struct gpt_walk *walk,
			unsigned long addr,
			unsigned long end,
			void *private,
			pgd_t *pgdp)
{
	unsigned long next;
	pud_t *pudp;

	pudp = pud_offset(pgdp, addr);
	do {
		int ret;

		next = pud_addr_end(addr, end);
		if (pud_none_or_clear_bad(pudp)) {
			if (walk_hole) {
				ret = walk_hole(vma, walk, addr,
						next, private);
				if (ret)
					return ret;
			}
			continue;
		}

		ret = hmm_walk_pmd(vma, walk_hole, walk_huge, walk_pte,
				   walk, addr, next, private, pudp);
		if (ret)
			return ret;

	} while (pudp++, addr = next, addr != end);

	return 0;
}

int hmm_walk(struct vm_area_struct *vma,
	     hmm_walk_hole_t walk_hole,
	     hmm_walk_huge_t walk_huge,
	     hmm_walk_pte_t walk_pte,
	     struct gpt_walk *walk,
	     unsigned long start,
	     unsigned long end,
	     void *private)
{
	unsigned long addr = start, next;
	pgd_t *pgdp;

	pgdp = pgd_offset(vma->vm_mm, addr);
	/*
	 * Flush cache even if we don't flush tlb, this for weird CPU arch for
	 * which HMM probably will never be use.
	 */
	flush_cache_range(vma, addr, end);
	do {
		int ret;

		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(pgdp)) {
			if (walk_hole) {
				ret = walk_hole(vma, walk, addr,
						next, private);
				if (ret)
					return ret;
			}
			continue;
		}

		ret = hmm_walk_pud(vma, walk_hole, walk_huge, walk_pte,
				   walk, addr, next, private, pgdp);
		if (ret)
			return ret;

	} while (pgdp++, addr = next, addr != end);

	return 0;
}
EXPORT_SYMBOL(hmm_walk);
