/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt) "ion: " fmt

#include <linux/module.h>
#include <linux/err.h>
#include <linux/hisi_ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include "../ion_priv.h"

struct hisi_ion_name_id_table {
	const char *name;
	unsigned int id;
};

static struct hisi_ion_name_id_table name_id_table[] = {
	{"fb", ION_FB_HEAP_ID},
	{"vpu", ION_VPU_HEAP_ID},
	{"jpu", ION_JPU_HEAP_ID},
	{"gralloc-carveout", ION_GRALLOC_HEAP_ID},
	{"overlay", ION_OVERLAY_HEAP_ID},
	{"sys_user", ION_SYSTEM_HEAP_ID},
	{"sys_contig", ION_SYSTEM_CONTIG_HEAP_ID},
	{"cma", ION_HEAP_TYPE_DMA},
};

struct hisi_ion_type_id_table {
	const char *name;
	enum ion_heap_type type;
};

static struct hisi_ion_type_id_table type_id_table[] = {
	{"ion_system_contig", ION_HEAP_TYPE_SYSTEM_CONTIG},
	{"ion_system", ION_HEAP_TYPE_SYSTEM},
	{"ion_carveout", ION_HEAP_TYPE_CARVEOUT},
	{"ion_chunk", ION_HEAP_TYPE_CHUNK},
	{"ion_dma", ION_HEAP_TYPE_DMA},
	{"ion_custom", ION_HEAP_TYPE_CUSTOM},
	{"ion_cma", ION_HEAP_TYPE_DMA},
};

#define HISI_ION_HEAP_NUM 16

static struct ion_platform_data hisi_ion_platform_data = {0};
static struct ion_platform_heap hisi_ion_platform_heap[HISI_ION_HEAP_NUM] = {{0} };

static struct ion_device *hisi_ion_device;
static struct ion_heap *hisi_ion_heap[HISI_ION_HEAP_NUM] = {NULL};

int hisi_ion_get_heap_info(unsigned int id, struct ion_heap_info_data *data)
{
	int i;

	BUG_ON(!data);

	for (i = 0; i < hisi_ion_platform_data.nr; i++) {
		if (hisi_ion_platform_heap[i].id == id) {
			data->heap_phy  = hisi_ion_platform_heap[i].base;
			data->heap_size = hisi_ion_platform_heap[i].size;
			strncpy((void *)data->name, (void *)hisi_ion_platform_heap[i].name, HISI_ION_NAME_LEN);
			pr_info("heap info : id %d name %s phy 0x%llx size %u\n",
					id, data->name, data->heap_phy, data->heap_size);
			return 0;
		}
	}
	pr_err("in %s please check the id %d\n", __func__, id);

	return -EINVAL;
}
EXPORT_SYMBOL(hisi_ion_get_heap_info);

struct ion_device *get_ion_device(void)
{
	return hisi_ion_device;
}
EXPORT_SYMBOL(get_ion_device);

static int get_id_by_name(const char *name, unsigned int *id)
{
	int i, n;

	n = sizeof(name_id_table)/sizeof(name_id_table[0]);
	for (i = 0; i < n; i++) {
		if (strncmp(name, name_id_table[i].name, HISI_ION_NAME_LEN))
			continue;

		*id = name_id_table[i].id;
		return 0;
	}
	return -1;
}

static int get_type_by_name(const char *name, enum ion_heap_type *type)
{
	int i, n;

	n = sizeof(type_id_table)/sizeof(type_id_table[0]);
	for (i = 0; i < n; i++) {
		if (strncmp(name, type_id_table[i].name, HISI_ION_NAME_LEN))
			continue;

		*type = type_id_table[i].type;
		return 0;
	}

	return -1;
}

static u64 hisi_dmamask = DMA_BIT_MASK(32);

static struct platform_device ion_cma_device = {
	.name = "ion-cma-device",
	.id = -1,
	.dev = {
		.dma_mask = &hisi_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	}
};

static int hisi_ion_setup_platform_data(struct platform_device *dev)
{
	struct device_node *node, *np;
	const char *heap_name;
	const char *type_name;
	unsigned int id;
	unsigned int range[2] = {0, 0};
	enum ion_heap_type type;
	int ret;
	int index = 0;

	node = dev->dev.of_node;
	for_each_child_of_node(node, np) {
		ret = of_property_read_string(np, "heap-name", &heap_name);
		if (ret < 0) {
			pr_err("in node %s please check the name property of node %s\n", __func__, np->name);
			continue;
		}

		ret = get_id_by_name(heap_name, &id);
		if (ret < 0) {
			pr_err("in node %s please check the name %s\n", __func__, heap_name);
			continue;
		}

		ret = of_property_read_u32_array(np, "heap-range", range, ARRAY_SIZE(range));
		if (ret < 0) {
			pr_err("in node %s please check the range property of node %s\n", __func__, np->name);
			continue;
		}


		ret = of_property_read_string(np, "heap-type", &type_name);
		if (ret < 0) {
			pr_err("in node %s please check the type property of node %s\n", __func__, np->name);
			continue;
		}

		ret = get_type_by_name(type_name, &type);
		if (ret < 0) {
			pr_err("in node %s please check the type %s\n", __func__, type_name);
			continue;
		}

		hisi_ion_platform_heap[index].name = heap_name;
		hisi_ion_platform_heap[index].base = range[0];
		hisi_ion_platform_heap[index].size = range[1];
		hisi_ion_platform_heap[index].id = id;
		hisi_ion_platform_heap[index].type = type;
		if (type == ION_HEAP_TYPE_DMA) {
		//	ion_cma_device.dev.archdata.dma_ops = swiotlb_dma_ops;
			hisi_ion_platform_heap[index].priv =
				(void *)&ion_cma_device.dev;
		}
		index++;
	}

	hisi_ion_platform_data.nr = index;
	hisi_ion_platform_data.heaps = hisi_ion_platform_heap;

	return 0;
}

static int hisi_ion_probe(struct platform_device *pdev)
{
	int i, err;
	struct ion_heap *heap;
	struct ion_platform_heap *heap_data;

	if (hisi_ion_setup_platform_data(pdev)) {
		pr_err("hisi_ion_setup_platform_data is failed\n");
		return -EINVAL;
	}

	hisi_ion_device = ion_device_create(NULL);
	if (IS_ERR_OR_NULL(hisi_ion_device))
		return PTR_ERR(hisi_ion_device);
	/*
	 * create the heaps as specified in the board file
	 */
	for (i = 0; i < hisi_ion_platform_data.nr; i++) {
		heap_data = &hisi_ion_platform_data.heaps[i];
		heap = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heap)) {
			err = PTR_ERR(heap);
			goto out;
		}

		ion_device_add_heap(hisi_ion_device, heap);
		hisi_ion_heap[i] = heap;
	}
	platform_set_drvdata(pdev, hisi_ion_device);

	return 0;
out:
	for (i = 0; i < HISI_ION_HEAP_NUM; i++) {
		if (!hisi_ion_heap[i])
			continue;
		ion_heap_destroy(hisi_ion_heap[i]);
		hisi_ion_heap[i] = NULL;
	}
	return err;
}

static int hisi_ion_remove(struct platform_device *pdev)
{
	int i;

	ion_device_destroy(hisi_ion_device);
	for (i = 0; i < HISI_ION_HEAP_NUM; i++) {
		if (!hisi_ion_heap[i])
			continue;
		ion_heap_destroy(hisi_ion_heap[i]);
		hisi_ion_heap[i] = NULL;
	}

	return 0;
}

static struct of_device_id hisi_ion_match_table[] = {
	{.compatible = "hisilicon,ion"},
	{},
};

static struct platform_driver hisi_ion_driver = {
	.probe = hisi_ion_probe,
	.remove = hisi_ion_remove,
	.driver = {
		.name = "ion",
		.of_match_table = hisi_ion_match_table,
	},
};

module_platform_driver(hisi_ion_driver);
