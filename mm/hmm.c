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
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/hmm.h>

static int hmm_init(struct hmm *hmm, struct mm_struct *mm)
{
	hmm->mm = mm;
	kref_init(&hmm->kref);
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
