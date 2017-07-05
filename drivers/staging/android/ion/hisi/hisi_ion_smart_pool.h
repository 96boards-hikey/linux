/*
 * Copyright (C) 2016 Hislicon, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ION_SMART_POOL_H
#define _ION_SMART_POOL_H

#include <linux/kthread.h>
#include <linux/types.h>
#include "ion_priv.h"

#define LOWORDER_WATER_MASK (64*4)
#define MAX_POOL_SIZE (128*64*4)

struct ion_smart_pool {
	struct ion_page_pool *pools[0];
};
bool ion_smart_is_graphic_buffer(struct ion_buffer *buffer);
void ion_smart_pool_debug_show_total(struct seq_file *s,
				     struct ion_smart_pool *smart_pool);
void ion_smart_sp_init_page(struct page *page);
struct page *ion_smart_pool_allocate(struct ion_smart_pool *pool,
				     unsigned long size,
				     unsigned int max_order);
int ion_smart_pool_free(struct ion_smart_pool *pool, struct page *page);
int ion_smart_pool_shrink(struct ion_smart_pool *smart_pool,
			  struct ion_page_pool *pool, gfp_t gfp_mask,
			  int nr_to_scan);
struct ion_smart_pool *ion_smart_pool_create(void);
void ion_smart_pool_wakeup_process(void);
void ion_smart_set_water_mark(int water_mark);
#endif /* _ION_SMART_POOL_H */
