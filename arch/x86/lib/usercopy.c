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
 * We rely on the nested NMI work to allow atomic faults from the NMI path; the
 * nested NMI paths are careful to preserve CR2.
 */
unsigned long
copy_from_user_nmi(void *to, const void __user *from, unsigned long n)
{
	unsigned long ret;

	if (__range_not_ok(from, n, TASK_SIZE))
		return 0;

	/*
	 * Even though this function is typically called from NMI/IRQ context
	 * disable pagefaults so that its behaviour is consistent even when
	 * called form other contexts.
	 */
	pagefault_disable();
	ret = __copy_from_user_inatomic(to, from, n);
	pagefault_enable();

	/*
	 * RHEL7: upstream has changed this to return bytes not copied.
	 * Future perf changes will change this function.  For now,
	 * just return bytes not copied.  See upstream commit
	 * 0a196848 ("perf: Fix arch_perf_out_copy_user default")
	 */
	return n - ret;
}
EXPORT_SYMBOL_GPL(copy_from_user_nmi);
