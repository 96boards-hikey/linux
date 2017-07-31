
/*
 *
 * Copyright (C) 2016 hisilicon, Inc.
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

#define pr_fmt(fmt) "smartpool: " fmt

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/hisi/ion-iommu.h>
#include <linux/sizes.h>
#include <linux/module.h>
#include <linux/kthread.h>

#include "ion.h"
#include "hisi_ion_smart_pool.h"

/*for pclin*/
/*lint -save -e846 -e514 -e866 -e30 -e84 -e712 -e701 -e40 -e578 -e528*/
/*lint -save -e522 -e838 -e737 -e84 -e774 -e845 -e527 -e531 -e702 -e753*/
/*lint -save -e713 -e732 -e438 -e778 -e708 -e21 -e528 -e756*/

static bool smart_pool_enable = true;
static int smart_pool_alloc_size;

struct task_struct *smart_pool_thread;
static wait_queue_head_t smart_pool_wait;
static unsigned int smart_pool_wait_flag;

int smart_pool_water_mark = 24 * 64 * 4;

static unsigned int smart_pool_orders[] = {9, 8, 4, 2, 0};

static const int smart_pool_num_orders = ARRAY_SIZE(smart_pool_orders);

#define SMART_POOL_MIN(x, y) (((x) < (y)) ? (x) : (y))

bool ion_smart_is_graphic_buffer(struct ion_buffer *buffer)
{
	if (NULL == buffer) {
		pr_err("%s: buffer is NULL!\n", __func__);
		return false;
	}
	return !!(buffer->flags & ION_FLAG_GRAPHIC_BUFFER);
}

void ion_smart_set_water_mark(int water_mark)
{
	smart_pool_water_mark = water_mark;
}

static int sp_order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < smart_pool_num_orders; i++) {
		if (order == smart_pool_orders[i])
			return i;
	}

	BUG();
	return -1;
}

static inline unsigned long sp_order_to_size(int order)
{
	return PAGE_SIZE << order;
}

static int sp_ion_page_pool_total(struct ion_page_pool *pool)
{
	int count;

	if (NULL == pool) {
		pr_err("%s: pool is NULL!\n", __func__);
		return 0;
	}

	count = pool->low_count + pool->high_count;

	return count << pool->order;
}

static int sp_pool_total_pages(struct ion_smart_pool *pool)
{
	int i;
	int count = 0;

	if (NULL == pool) {
		pr_err("%s: pool is NULL!\n", __func__);
		return 0;
	}

	for (i = 0; i < smart_pool_num_orders; i++)
		count += sp_ion_page_pool_total(pool->pools[i]);

	return count;
}

void ion_smart_sp_init_page(struct page *page)
{
	unsigned long len;

	if (NULL == page) {
		pr_err("%s: page is NULL!\n", __func__);
		return;
	}
	len = PAGE_SIZE << compound_order(page);
	memset(page_address(page), 0, len);
	__flush_dcache_area(page_address(page), len);
}

static int sp_fill_pool_once(struct ion_page_pool *pool)
{
	struct page *page;

	if (NULL == pool) {
		pr_err("%s: pool is NULL!\n", __func__);
		return -ENOENT;
	}
	page = ion_page_pool_alloc_pages(pool);
	if (NULL == page)
		return -ENOMEM;
	ion_smart_sp_init_page(page);
	ion_page_pool_free(pool, page);

	return 0;
}

static int ion_smart_pool_kworkthread(void *p)
{
	int i;
	struct ion_smart_pool *pool;
	int ret;

	if (NULL == p) {
		pr_err("%s: p is NULL!\n", __func__);
		return 0;
	}

	pool = (struct ion_smart_pool *)p;
	while (true) {
		ret = wait_event_interruptible(smart_pool_wait,
			(smart_pool_wait_flag == 1));
		if (ret < 0)
			continue;

		smart_pool_wait_flag = 0;
		for (i = 0; i < smart_pool_num_orders; i++) {
			while (sp_pool_total_pages(pool) <
			       smart_pool_water_mark) {
				if (sp_fill_pool_once(pool->pools[i]) < 0)
					break;
			}
		}

		for (i = 2; i < smart_pool_num_orders; i++) {
			while (sp_ion_page_pool_total(pool->pools[i]) <
			       LOWORDER_WATER_MASK) {
				if (sp_fill_pool_once(pool->pools[i]) < 0)
					break;
			}
		}
	}

	return 0;
}

struct page *ion_smart_pool_allocate(struct ion_smart_pool *pool,
				     unsigned long size, unsigned int max_order)
{
	int i;
	struct page *page;

	if (NULL == pool) {
		pr_err("%s: pool is NULL!\n", __func__);
		return NULL;
	}

	for (i = 0; i < smart_pool_num_orders; i++) {
		if (size < sp_order_to_size(smart_pool_orders[i]))
			continue;
		if (max_order < smart_pool_orders[i])
			continue;

		page = ion_page_pool_alloc(pool->pools[i]);
		if (!page)
			continue;
		if (smart_pool_alloc_size) {
			smart_pool_alloc_size +=
				PAGE_SIZE << compound_order(page);
		}
		return page;
	}

	return NULL;
}

void ion_smart_pool_wakeup_process(void)
{
	if (!smart_pool_enable)
		return;
	smart_pool_wait_flag = 1;
	wake_up_interruptible(&smart_pool_wait);
}

void ion_smart_pool_all_free(struct ion_smart_pool *pool, gfp_t gfp_mask,
			     int nr_to_scan)
{
	int i;

	if (NULL == pool) {
		pr_err("%s: smart_pool is NULL!\n", __func__);
		return;
	}

	for (i = 0; i < smart_pool_num_orders; i++)
		ion_page_pool_shrink(pool->pools[i], gfp_mask, nr_to_scan);
}

int ion_smart_pool_free(struct ion_smart_pool *pool, struct page *page)
{
	int order;

	if (!smart_pool_enable) {
		ion_smart_pool_all_free(pool, __GFP_HIGHMEM, MAX_POOL_SIZE);
		return -1;
	}
	if ((NULL == pool) || (NULL == page)) {
		pr_err("%s: pool/page is NULL!\n", __func__);
		return -1;
	}

	order = compound_order(page);

	if (sp_pool_total_pages(pool) < MAX_POOL_SIZE) {
		ion_smart_sp_init_page(page);
		ion_page_pool_free(pool->pools[sp_order_to_index(order)], page);
		return 0;
	}

	return -1;
}

int ion_smart_pool_shrink(struct ion_smart_pool *smart_pool,
			  struct ion_page_pool *pool, gfp_t gfp_mask,
			  int nr_to_scan)
{
	int nr_max_free;
	int nr_to_free;
	int nr_total = 0;

	if ((NULL == smart_pool) || (NULL == pool)) {
		pr_err("%s: smartpool/pool is NULL!\n", __func__);
		return 0;
	}

	if (nr_to_scan == 0)
		return ion_page_pool_shrink(pool, gfp_mask, 0);

	nr_max_free = sp_pool_total_pages(smart_pool) -
	    (smart_pool_water_mark + LOWORDER_WATER_MASK);
	nr_to_free = SMART_POOL_MIN(nr_max_free, nr_to_scan);

	if (nr_to_free <= 0)
		return 0;

	nr_total = ion_page_pool_shrink(pool, gfp_mask, nr_to_free);
	return nr_total;
}

void ion_smart_pool_debug_show_total(struct seq_file *s,
				     struct ion_smart_pool *smart_pool)
{
	if ((NULL == s) || (NULL == smart_pool)) {
		pr_err("%s: s/smart_pool is NULL!\n", __func__);
		return;
	}
	seq_puts(s, "----------------------------------------------------\n");
	seq_printf(s, "in smart pool =  %d total\n",
		   sp_pool_total_pages(smart_pool) * 4 / 1024);
}

struct ion_smart_pool *ion_smart_pool_create(void)
{
	struct ion_smart_pool *smart_pool =
	    kzalloc(sizeof(struct ion_smart_pool) +
		    sizeof(struct ion_page_pool *) * smart_pool_num_orders,
		    GFP_KERNEL);
	bool graphic_buffer_flag = true;

	if (NULL == smart_pool) {
		pr_err("%s: smart_pool is NULL!\n", __func__);
		return NULL;
	}

	if (ion_system_heap_create_pools(smart_pool->pools,
				graphic_buffer_flag))
		goto free_heap;

	init_waitqueue_head(&smart_pool_wait);
	smart_pool_thread = kthread_run(ion_smart_pool_kworkthread, smart_pool,
					"%s", "smartpool");
	if (IS_ERR(smart_pool_thread)) {
		pr_err("%s: kthread_create failed!\n", __func__);
		goto destroy_pools;
	}

	ion_smart_pool_wakeup_process();

	return smart_pool;

destroy_pools:
	ion_system_heap_destroy_pools(smart_pool->pools);

free_heap:
	kfree(smart_pool);
	smart_pool = NULL;
	return NULL;
}

module_param_named(debug_smart_pool_enable, smart_pool_enable, bool, 0644);
MODULE_PARM_DESC(debug_smart_pool_enable, "enable smart pool");

module_param_named(debug_smart_pool_alloc_size, smart_pool_alloc_size, int,
		0644);
MODULE_PARM_DESC(debug_smart_pool_alloc_size, "alloc size from smartpool");
/*lint -restore*/
