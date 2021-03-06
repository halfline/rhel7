/*
 *    Copyright IBM Corp. 2006
 *    Author(s): Heiko Carstens <heiko.carstens@de.ibm.com>
 */

#include <linux/bootmem.h>
#include <linux/pfn.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/hugetlb.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/setup.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>

static DEFINE_MUTEX(vmem_mutex);

struct memory_segment {
	struct list_head list;
	unsigned long start;
	unsigned long size;
};

static LIST_HEAD(mem_segs);

static void __ref *vmem_alloc_pages(unsigned int order)
{
	unsigned long size = PAGE_SIZE << order;

	if (slab_is_available())
		return (void *)__get_free_pages(GFP_KERNEL, order);
	return alloc_bootmem_align(size, size);
}

static inline pud_t *vmem_pud_alloc(void)
{
	pud_t *pud = NULL;

#ifdef CONFIG_64BIT
	pud = vmem_alloc_pages(2);
	if (!pud)
		return NULL;
	clear_table((unsigned long *) pud, _REGION3_ENTRY_EMPTY, PAGE_SIZE * 4);
#endif
	return pud;
}

pmd_t *vmem_pmd_alloc(unsigned long address)
{
	pmd_t *pmd = NULL;

#ifdef CONFIG_64BIT
	pmd = vmem_alloc_pages(2);
	if (!pmd)
		return NULL;
	clear_table((unsigned long *) pmd, _SEGMENT_ENTRY_EMPTY, PAGE_SIZE * 4);
#endif
	return pmd;
}

pte_t __ref *vmem_pte_alloc(unsigned long address)
{
	pte_t *pte;

	if (slab_is_available())
		pte = (pte_t *) page_table_alloc(&init_mm, address);
	else
		pte = alloc_bootmem(PTRS_PER_PTE * sizeof(pte_t));
	if (!pte)
		return NULL;
	clear_table((unsigned long *) pte, _PAGE_INVALID,
		    PTRS_PER_PTE * sizeof(pte_t));
	return pte;
}

/*
 * Add a physical memory range to the 1:1 mapping.
 */
static int vmem_add_mem(unsigned long start, unsigned long size)
{
	unsigned long pgt_prot, sgt_prot, r3_prot;
	unsigned long end = start + size;
	unsigned long address = start;
	pgd_t *pg_dir;
	pud_t *pu_dir;
	pmd_t *pm_dir;
	pte_t *pt_dir;
	int ret = -ENOMEM;

	pgt_prot = pgprot_val(PAGE_KERNEL);
	sgt_prot = pgprot_val(SEGMENT_KERNEL);
	r3_prot = pgprot_val(REGION3_KERNEL);
	if (!MACHINE_HAS_NX) {
		pgt_prot &= ~_PAGE_NOEXEC;
		sgt_prot &= ~_SEGMENT_ENTRY_NOEXEC;
		r3_prot &= ~_REGION_ENTRY_NOEXEC;
	}
	while (address < end) {
		pg_dir = pgd_offset_k(address);
		if (pgd_none(*pg_dir)) {
			pu_dir = vmem_pud_alloc();
			if (!pu_dir)
				goto out;
			pgd_populate(&init_mm, pg_dir, pu_dir);
		}
		pu_dir = pud_offset(pg_dir, address);
#ifdef CONFIG_64BIT
		if (MACHINE_HAS_EDAT2 && pud_none(*pu_dir) && address &&
		    !(address & ~PUD_MASK) && (address + PUD_SIZE <= end) &&
		    !debug_pagealloc_enabled()) {
			pud_val(*pu_dir) = address | r3_prot;
			address += PUD_SIZE;
			continue;
		}
#endif
		if (pud_none(*pu_dir)) {
			pm_dir = vmem_pmd_alloc(address);
			if (!pm_dir)
				goto out;
			pud_populate(&init_mm, pu_dir, pm_dir);
		}
		pm_dir = pmd_offset(pu_dir, address);
#ifdef CONFIG_64BIT
		if (MACHINE_HAS_EDAT1 && pmd_none(*pm_dir) && address &&
		    !(address & ~PMD_MASK) && (address + PMD_SIZE <= end) &&
		    !debug_pagealloc_enabled()) {
			pmd_val(*pm_dir) = address | sgt_prot;
			address += PMD_SIZE;
			continue;
		}
#endif
		if (pmd_none(*pm_dir)) {
			pt_dir = vmem_pte_alloc(address);
			if (!pt_dir)
				goto out;
			pmd_populate(&init_mm, pm_dir, pt_dir);
		}

		pt_dir = pte_offset_kernel(pm_dir, address);
		pte_val(*pt_dir) = address | pgt_prot;
		address += PAGE_SIZE;
	}
	ret = 0;
out:
	flush_tlb_kernel_range(start, end);
	return ret;
}

/*
 * Remove a physical memory range from the 1:1 mapping.
 * Currently only invalidates page table entries.
 */
static void vmem_remove_range(unsigned long start, unsigned long size)
{
	unsigned long end = start + size;
	unsigned long address = start;
	pgd_t *pg_dir;
	pud_t *pu_dir;
	pmd_t *pm_dir;
	pte_t *pt_dir;
	pte_t  pte;

	pte_val(pte) = _PAGE_INVALID;
	while (address < end) {
		pg_dir = pgd_offset_k(address);
		if (pgd_none(*pg_dir)) {
			address += PGDIR_SIZE;
			continue;
		}
		pu_dir = pud_offset(pg_dir, address);
		if (pud_none(*pu_dir)) {
			address += PUD_SIZE;
			continue;
		}
		if (pud_large(*pu_dir)) {
			pud_clear(pu_dir);
			address += PUD_SIZE;
			continue;
		}
		pm_dir = pmd_offset(pu_dir, address);
		if (pmd_none(*pm_dir)) {
			address += PMD_SIZE;
			continue;
		}
		if (pmd_large(*pm_dir)) {
			pmd_clear(pm_dir);
			address += PMD_SIZE;
			continue;
		}
		pt_dir = pte_offset_kernel(pm_dir, address);
		*pt_dir = pte;
		address += PAGE_SIZE;
	}
	flush_tlb_kernel_range(start, end);
}

/*
 * Add a backed mem_map array to the virtual mem_map array.
 */
int __meminit vmemmap_populate(unsigned long start, unsigned long end, int node)
{
	unsigned long pgt_prot, sgt_prot;
	unsigned long address = start;
	pgd_t *pg_dir;
	pud_t *pu_dir;
	pmd_t *pm_dir;
	pte_t *pt_dir;
	int ret = -ENOMEM;

	pgt_prot = pgprot_val(PAGE_KERNEL);
	sgt_prot = pgprot_val(SEGMENT_KERNEL);
	if (!MACHINE_HAS_NX) {
		pgt_prot &= ~_PAGE_NOEXEC;
		sgt_prot &= ~_SEGMENT_ENTRY_NOEXEC;
	}
	for (address = start; address < end;) {
		pg_dir = pgd_offset_k(address);
		if (pgd_none(*pg_dir)) {
			pu_dir = vmem_pud_alloc();
			if (!pu_dir)
				goto out;
			pgd_populate(&init_mm, pg_dir, pu_dir);
		}

		pu_dir = pud_offset(pg_dir, address);
		if (pud_none(*pu_dir)) {
			pm_dir = vmem_pmd_alloc(address);
			if (!pm_dir)
				goto out;
			pud_populate(&init_mm, pu_dir, pm_dir);
		}

		pm_dir = pmd_offset(pu_dir, address);
		if (pmd_none(*pm_dir)) {
#ifdef CONFIG_64BIT
			/* Use 1MB frames for vmemmap if available. We always
			 * use large frames even if they are only partially
			 * used.
			 * Otherwise we would have also page tables since
			 * vmemmap_populate gets called for each section
			 * separately. */
			if (MACHINE_HAS_EDAT1) {
				void *new_page;

				new_page = vmemmap_alloc_block(PMD_SIZE, node);
				if (!new_page)
					goto out;
				pmd_val(*pm_dir) = __pa(new_page) | sgt_prot;
				address = (address + PMD_SIZE) & PMD_MASK;
				continue;
			}
#endif
			pt_dir = vmem_pte_alloc(address);
			if (!pt_dir)
				goto out;
			pmd_populate(&init_mm, pm_dir, pt_dir);
		} else if (pmd_large(*pm_dir)) {
			address = (address + PMD_SIZE) & PMD_MASK;
			continue;
		}

		pt_dir = pte_offset_kernel(pm_dir, address);
		if (pte_none(*pt_dir)) {
			unsigned long new_page;

			new_page =__pa(vmem_alloc_pages(0));
			if (!new_page)
				goto out;
			pte_val(*pt_dir) = __pa(new_page) | pgt_prot;
		}
		address += PAGE_SIZE;
	}
	memset((void *)start, 0, end - start);
	ret = 0;
out:
	flush_tlb_kernel_range(start, end);
	return ret;
}

void vmemmap_free(unsigned long start, unsigned long end)
{
}

/*
 * Add memory segment to the segment list if it doesn't overlap with
 * an already present segment.
 */
static int insert_memory_segment(struct memory_segment *seg)
{
	struct memory_segment *tmp;

	if (seg->start + seg->size > VMEM_MAX_PHYS ||
	    seg->start + seg->size < seg->start)
		return -ERANGE;

	list_for_each_entry(tmp, &mem_segs, list) {
		if (seg->start >= tmp->start + tmp->size)
			continue;
		if (seg->start + seg->size <= tmp->start)
			continue;
		return -ENOSPC;
	}
	list_add(&seg->list, &mem_segs);
	return 0;
}

/*
 * Remove memory segment from the segment list.
 */
static void remove_memory_segment(struct memory_segment *seg)
{
	list_del(&seg->list);
}

static void __remove_shared_memory(struct memory_segment *seg)
{
	remove_memory_segment(seg);
	vmem_remove_range(seg->start, seg->size);
}

int vmem_remove_mapping(unsigned long start, unsigned long size)
{
	struct memory_segment *seg;
	int ret;

	mutex_lock(&vmem_mutex);

	ret = -ENOENT;
	list_for_each_entry(seg, &mem_segs, list) {
		if (seg->start == start && seg->size == size)
			break;
	}

	if (seg->start != start || seg->size != size)
		goto out;

	ret = 0;
	__remove_shared_memory(seg);
	kfree(seg);
out:
	mutex_unlock(&vmem_mutex);
	return ret;
}

int vmem_add_mapping(unsigned long start, unsigned long size)
{
	struct memory_segment *seg;
	int ret;

	mutex_lock(&vmem_mutex);
	ret = -ENOMEM;
	seg = kzalloc(sizeof(*seg), GFP_KERNEL);
	if (!seg)
		goto out;
	seg->start = start;
	seg->size = size;

	ret = insert_memory_segment(seg);
	if (ret)
		goto out_free;

	ret = vmem_add_mem(start, size);
	if (ret)
		goto out_remove;
	goto out;

out_remove:
	__remove_shared_memory(seg);
out_free:
	kfree(seg);
out:
	mutex_unlock(&vmem_mutex);
	return ret;
}

/*
 * map whole physical memory to virtual memory (identity mapping)
 * we reserve enough space in the vmalloc area for vmemmap to hotplug
 * additional memory segments.
 */
void __init vmem_map_init(void)
{
	int i;

	for (i = 0; i < MEMORY_CHUNKS; i++) {
		if (!memory_chunk[i].size)
			continue;
		vmem_add_mem(memory_chunk[i].addr, memory_chunk[i].size);
	}
	__set_memory((unsigned long) _stext,
		     (_etext - _stext) >> PAGE_SHIFT,
		     SET_MEMORY_RO | SET_MEMORY_X);
	__set_memory((unsigned long) _etext,
		     (_eshared - _etext) >> PAGE_SHIFT,
		     SET_MEMORY_RO);
	__set_memory((unsigned long) _sinittext,
		     (_einittext - _sinittext) >> PAGE_SHIFT,
		     SET_MEMORY_RO | SET_MEMORY_X);
	pr_info("Write protected kernel read-only data: %luk\n",
		(_eshared - _stext) >> 10);
}

/*
 * Convert memory chunk array to a memory segment list so there is a single
 * list that contains both r/w memory and shared memory segments.
 */
static int __init vmem_convert_memory_chunk(void)
{
	struct memory_segment *seg;
	int i;

	mutex_lock(&vmem_mutex);
	for (i = 0; i < MEMORY_CHUNKS; i++) {
		if (!memory_chunk[i].size)
			continue;
		seg = kzalloc(sizeof(*seg), GFP_KERNEL);
		if (!seg)
			panic("Out of memory...\n");
		seg->start = memory_chunk[i].addr;
		seg->size = memory_chunk[i].size;
		insert_memory_segment(seg);
	}
	mutex_unlock(&vmem_mutex);
	return 0;
}

core_initcall(vmem_convert_memory_chunk);
