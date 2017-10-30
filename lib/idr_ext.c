/*
 * lib/idr_ext.c		IDR extended
 *
 * Copyright 2017 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * An extension to IDR that allows to store u32 indices used by flower and
 * net-sched actions.
 * The current IDR implementation supports ID from range <0, INT_MAX> and
 * unfortunately classifiers and actions use u32's <0, UINT_MAX>. The IDR
 * in upstream was extended by commit 388f79fda74f ("idr: Add new APIs to
 * support unsigned long") that is not backportable currently because IDR
 * in upstream is based on radix-tree and RHEL uses an older implementation.
 * To backport this commit the IDR needs to be rebased and this is out of
 * scope of this series.
 * Instead of this this extension that uses two IDR ranges for requested
 * range has been created. The 1st is for <0, INT_MAX> and the 2nd is for
 * <INT_MAX+1, UINT_MAX>.
 *
 * Differences between this extension and upstream:
 *
 * - The API in RHEL uses 'struct idr_ext' instead of 'struct idr'
 * - To initialize and destroy idr_init_ext() and idr_destroy_ext() are
 *   used instead of idr_init() and idr_destroy()
 * - RHEL support only range <0, UINT_MAX> that is enough for net-sched
 *
 * The rest of API introduced by 388f79fda74f should be identical.
 */

#include <linux/idr_ext.h>

int idr_alloc_ext(struct idr_ext *idrext, void *ptr, unsigned long *index,
		  unsigned long start, unsigned long end, gfp_t gfp)
{
	int istart, iend, result;

	/* Sanity check */
	if (unlikely(start > UINT_MAX || end > UINT_MAX))
		return -EINVAL;

	if (start <= (unsigned long)INT_MAX) {
		/* The start belongs to interval <0, INT_MAX> */
		istart = (int)start;

		/*
		 * If 'end' belongs to interval <INT_MAX+1, UINT_MAX> then
		 * clip it to the maximum of first range. This means zero
		 * because idr_alloc() uses this value for 'end' as maximum.
		 */
		if (end <= (unsigned long)INT_MAX)
			iend = (int)end;
		else
			iend = 0;

		/* Try to allocate from the first block */
		result = idr_alloc(&idrext->idr_lo, ptr, istart, iend, gfp);
		if (result >= 0) {
			if (index)
				*index = result;
			return 0;
		}

		/*
		 * In case of error return it to the caller except the
		 * situation that the part of the requested range is full
		 * in lower block.
		 */
		if (result != -ENOSPC)
			return result;

		/*
		 * There is no space in the lower block, move the start
		 * to the beginning of the higher block and continue.
		 * The higher range will be searched only if requested
		 * one touches it -> this means (end > INT_MAX || end == 0)
		 */
		start = (unsigned long)INT_MAX + 1;
	}

	/*
	 * Continue with higher block only if 'end' belongs to high interval
	 * and is higher than 'start'.
	 * Note that 'start' is always higher than INT_MAX at this place and
	 * we need to zero value of 'end' that means maximum.
	 */
	if (!end || end > start) {
		/* Adjust 'start' and 'end' by higher block offset */
		istart = (int)(start - INT_MAX - 1);

		if (end)
			iend = (int)(end - INT_MAX - 1);
		else
			iend = 0;

		/* Try to allocate from higher block */
		result = idr_alloc(&idrext->idr_hi, ptr, istart, iend, gfp);
		if (result >= 0) {
			if (index)
				/* Move the 'result' to higher range */
				*index = (unsigned long)result + INT_MAX + 1;
			return 0;
		}

		return result;
	}

	return -ENOSPC;
}
EXPORT_SYMBOL(idr_alloc_ext);

void *idr_get_next_ext(struct idr_ext *idrext, unsigned long *nextidp)
{
	void *ptr;
	int idp;

	if (*nextidp > UINT_MAX)
		return NULL;

	if (*nextidp <= (unsigned long)INT_MAX) {
		idp = (int)*nextidp;
		ptr = idr_get_next(&idrext->idr_lo, &idp);
		if (ptr) {
			*nextidp = (unsigned long)idp;
			return ptr;
		}
		/* Not found - continue with higher range */
		idp = 0;
	} else {
		/* Subtract higher range offset */
		idp = (int)(*nextidp - INT_MAX - 1);
	}

	ptr = idr_get_next(&idrext->idr_hi, &idp);
	if (ptr) {
		/* Add higher range offset */
		*nextidp = (unsigned long)idp + INT_MAX + 1;
		return ptr;
	}

	return NULL;
}
EXPORT_SYMBOL(idr_get_next_ext);
