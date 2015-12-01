/*
 * User address space access functions.
 *
 *  For licencing details see kernel-base/COPYING
 */

#include <linux/highmem.h>
#include <linux/module.h>

#include <asm/word-at-a-time.h>
#include <linux/sched.h>

/*
 * best effort, GUP based copy_from_user() that is NMI-safe
 */
unsigned long
copy_from_user_nmi(void *to, const void __user *from, unsigned long n)
{
	unsigned long offset, addr = (unsigned long)from;
	unsigned long size, len = 0;
	struct page *page;
	void *map;
	int ret;

	if (__range_not_ok(from, n, TASK_SIZE))
		return 0;

	do {
		ret = __get_user_pages_fast(addr, 1, 0, &page);
		if (!ret)
			break;

		offset = addr & (PAGE_SIZE - 1);
		size = min(PAGE_SIZE - offset, n - len);

		map = kmap_atomic(page);
		memcpy(to, map+offset, size);
		kunmap_atomic(map);
		put_page(page);

		len  += size;
		to   += size;
		addr += size;

	} while (len < n);

	/*
	 * RHEL7: upstream has changed this to return bytes not copied.
	 * Future perf changes will change this function.  For now,
	 * just return bytes not copied.  See upstream commit
	 * 0a196848 ("perf: Fix arch_perf_out_copy_user default")
	 */
	return (n - len);
}
EXPORT_SYMBOL_GPL(copy_from_user_nmi);
