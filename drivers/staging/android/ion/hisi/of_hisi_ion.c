/*
 * Hisilicon hisi ION Driver
 *
 * Copyright (c) 2015 Hisilicon Limited.
 *
 * Author: Chen Feng <puck.chen@hisilicon.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "Ion: " fmt

#include <linux/export.h>
#include <linux/err.h>
#include <linux/hisi/hisi_ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/rwsem.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <asm/cacheflush.h>
#include <asm/cpu.h>
#include <linux/compat.h>
#include <linux/sizes.h>
#include <ion_priv.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_fdt.h>
#include <asm/cputype.h>


#define MAX_HISI_ION_DYNAMIC_AREA_NAME_LEN  64
struct hisi_ion_dynamic_area {
	phys_addr_t    base;
	unsigned long  size;
	char           name[MAX_HISI_ION_DYNAMIC_AREA_NAME_LEN];
};

struct hisi_ion_type_table {
	const char *name;
	enum ion_heap_type type;
};

static const struct hisi_ion_type_table ion_type_table[] = {
	{"ion_system", ION_HEAP_TYPE_SYSTEM},
	{"ion_system_contig", ION_HEAP_TYPE_SYSTEM_CONTIG},
	{"ion_carveout", ION_HEAP_TYPE_CARVEOUT},
	{"ion_chunk", ION_HEAP_TYPE_CHUNK},
	{"ion_dma", ION_HEAP_TYPE_DMA},
	{"ion_custom", ION_HEAP_TYPE_CUSTOM},
};

static struct ion_device *idev;
static int num_heaps;
static struct ion_heap **heaps;
static struct ion_platform_heap **heaps_data;


#define MAX_HISI_ION_DYNAMIC_AREA_NUM  5
static struct hisi_ion_dynamic_area  ion_dynamic_area_table[MAX_HISI_ION_DYNAMIC_AREA_NUM];
static int ion_dynamic_area_count = 0;

static  int  add_dynamic_area(phys_addr_t base, unsigned long  len, const char* name)
{
	int ret = 0;
	int i = ion_dynamic_area_count;

	if (i < MAX_HISI_ION_DYNAMIC_AREA_NUM) {
		ion_dynamic_area_table[i].base = base;
		ion_dynamic_area_table[i].size  = len;
		strncpy(ion_dynamic_area_table[i].name, name,
				MAX_HISI_ION_DYNAMIC_AREA_NAME_LEN-1);
		ion_dynamic_area_table[i].name[MAX_HISI_ION_DYNAMIC_AREA_NAME_LEN-1] = '\0';
		pr_err("insert heap-name %s \n", ion_dynamic_area_table[i].name);

		ion_dynamic_area_count ++;

		return ret;

	}

	return -EFAULT;
}

static struct hisi_ion_dynamic_area* find_dynamic_area_by_name(const char* name)
{
	int i = 0;

	if (!name) {
		return NULL;
	}

	for (; i < MAX_HISI_ION_DYNAMIC_AREA_NUM; i++) {
		pr_err("name = %s, table name %s \n", name, ion_dynamic_area_table[i].name);
		if (!strcmp(name, ion_dynamic_area_table[i].name)) {
			return &ion_dynamic_area_table[i];
		}
	}

	return NULL;
}

static int __init hisi_ion_reserve_area(struct reserved_mem *rmem)
{
	char *status   = NULL;
	int  namesize = 0;
	const char* heapname;

	status = (char *)of_get_flat_dt_prop(rmem->fdt_node, "status", NULL);
	if (status && (strncmp(status, "ok", strlen("ok")) != 0))
		return 0;

	heapname = of_get_flat_dt_prop(rmem->fdt_node, "heap-name", &namesize);
	if (!heapname || (namesize <= 0)) {
		pr_err("no 'heap-name' property namesize=%d\n", namesize);
		return -EFAULT;
	}

	pr_info("base 0x%llx, size is 0x%llx, node name %s, heap-name %s namesize %d,"
			"[%d][%d][%d][%d]\n",
			rmem->base, rmem->size, rmem->name, heapname, namesize,
			heapname[0],heapname[1],heapname[2],heapname[3] );

	if (add_dynamic_area(rmem->base, rmem->size, heapname)) {
		pr_err("fail to add to dynamic area \n");
		return -EFAULT;
	}

	return 0;
}
RESERVEDMEM_OF_DECLARE(hisi_ion, "hisi_ion", hisi_ion_reserve_area);

struct ion_device *get_ion_device(void) {
	return idev;
}

static void ion_pm_init(void)
{
	return;
}

void ion_flush_all_cpus_caches(void)
{
	return;
}

struct ion_client *hisi_ion_client_create(const char *name)
{
	return ion_client_create(idev, name);
}
EXPORT_SYMBOL(hisi_ion_client_create);

static long hisi_ion_custom_ioctl(struct ion_client *client,
				unsigned int cmd,
				unsigned long arg)
{
	int ret = 0;

	return ret;
}

extern int hisi_ion_enable_iommu(struct platform_device *pdev);

static int get_type_by_name(const char *name, enum ion_heap_type *type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ion_type_table); i++) {
		if (strcmp(name, ion_type_table[i].name))
			continue;

		*type = ion_type_table[i].type;
		return 0;
	}

	return -EINVAL;
}

static int hisi_set_platform_data(struct platform_device *pdev)
{
	unsigned int base = 0;
	unsigned int size = 0;
	unsigned int id = 0;
	const char *heap_name;
	const char *type_name;
	const char *status;
	enum ion_heap_type type = 0;
	int ret = 0;
	struct device_node *np;
	struct device_node *phandle_node;
	struct property *prop;
	struct ion_platform_heap *p_data;
	const struct device_node *dt_node = pdev->dev.of_node;
	int index = 0;

	for_each_child_of_node(dt_node, np)
		num_heaps++;

	heaps_data = devm_kzalloc(&pdev->dev,
				  sizeof(struct ion_platform_heap *) *
				  num_heaps,
				  GFP_KERNEL);
	if (!heaps_data)
		return -ENOMEM;

	for_each_child_of_node(dt_node, np) {
		ret = of_property_read_string(np, "status", &status);
		if (!ret) {
			if (strncmp("ok", status, strlen("ok")))
				continue;
		}

		phandle_node = of_parse_phandle(np, "heap-name", 0);
		if (phandle_node) {
			int len;

			ret = of_property_read_string(phandle_node, "status", &status);
			if (!ret) {
				if (strncmp("ok", status, strlen("ok")))
					continue;
			}

			prop = of_find_property(phandle_node, "heap-name", &len);
			if (!prop) {
				pr_err("no heap-name in phandle of node %s\n", np->name);
				continue;
			}

			if (!prop->value || !prop->length) {
				pr_err("%s %s %d, node %s, invalid phandle, value=%p,length=%d\n",
						__FILE__,__FUNCTION__,__LINE__,
						np->name, prop->value, prop->length );
				continue;
			} else {
				heap_name = prop->value;
			}
		} else {
			ret = of_property_read_string(np, "heap-name", &heap_name);
			if (ret < 0) {
				pr_err("invalid heap-name in node %s, please check the name \n", np->name);
				continue;
			}

		}

		pr_err("node name [%s], heap-name [%s]\n", np->name, heap_name);

		ret = of_property_read_u32(np, "heap-id", &id);
		if (ret < 0) {
			pr_err("check the id %s\n", np->name);
			continue;
		}

		ret = of_property_read_u32(np, "heap-base", &base);
		if (ret < 0) {
			pr_err("check the base of node %s\n", np->name);
			continue;
		}

		ret = of_property_read_u32(np, "heap-size", &size);
		if (ret < 0) {
			pr_err("check the size of node %s\n", np->name);
			continue;
		}

		ret = of_property_read_string(np, "heap-type", &type_name);
		if (ret < 0) {
			pr_err("check the type of node %s\n", np->name);
			continue;
		}

		ret = get_type_by_name(type_name, &type);
		if (ret < 0) {
			pr_err("type name error %s!\n", type_name);
			continue;
		}
		pr_err("heap index %d : name %s base 0x%x size 0x%x id %d type %d\n",
			index, heap_name, base, size, id, type);

		p_data = devm_kzalloc(&pdev->dev,
				      sizeof(struct ion_platform_heap),
				      GFP_KERNEL);
		if (!p_data)
			return -ENOMEM;

		p_data->name = heap_name;
		p_data->base = base;
		p_data->size = size;
		p_data->id = id;
		p_data->type = type;
		p_data->priv = (void *)&pdev->dev;

		if (!p_data->base && !p_data->size) {
			struct hisi_ion_dynamic_area* area = NULL;
			pr_err("heap %s base =0, try to find dynamic area \n", p_data->name);
			area = find_dynamic_area_by_name(p_data->name);
			if (area) {
				p_data->base = area->base;
				p_data->size = area->size;
				pr_err("have found heap name %s base = 0x%lx, size %zu\n",
						p_data->name,
						p_data->base, p_data->size);
			}
		}

		heaps_data[index] = p_data;
		index++;
	}
	num_heaps = index;
	return 0;
}

static int hisi_ion_probe(struct platform_device *pdev)
{
	int i;
	int err;
	static struct ion_platform_heap *p_heap;

	idev = ion_device_create(hisi_ion_custom_ioctl);
	err = hisi_set_platform_data(pdev);
	if (err) {
		pr_err("ion set platform data error!\n");
		goto err_free_idev;
	}
	heaps = devm_kzalloc(&pdev->dev,
			     sizeof(struct ion_heap *) * num_heaps,
			     GFP_KERNEL);
	if (!heaps) {
		err = -ENOMEM;
		goto err_free_idev;
	}

	/* FIXME will move to iommu driver*/
	if (hisi_ion_enable_iommu(pdev)) {
		dev_info(&pdev->dev, "enable iommu fail \n");
		err = -EINVAL;
		goto err_free_idev;
	}
	ion_pm_init();

	/*
	 * create the heaps as specified in the dts file
	 */
	for (i = 0; i < num_heaps; i++) {
		p_heap = heaps_data[i];

		pr_info("id %d  name %s base %lu size %lu\n",
				i, p_heap->name, p_heap->base, p_heap->size);

		heaps[i] = ion_heap_create(p_heap);
		if (IS_ERR_OR_NULL(heaps[i])) {
			pr_err("error add %s of type %d with %lx@%lx\n",
			       p_heap->name, p_heap->type,
			       p_heap->base, (unsigned long)p_heap->size);
			continue;
		}

		ion_device_add_heap(idev, heaps[i]);

		pr_info("adding heap %s of type %d with %lx@%lx\n",
			p_heap->name, p_heap->type,
			p_heap->base, (unsigned long)p_heap->size);
	}
	return 0;

err_free_idev:
	ion_device_destroy(idev);

	return err;
}

static int hisi_ion_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < num_heaps; i++) {
		ion_heap_destroy(heaps[i]);
		heaps[i] = NULL;
	}
	ion_device_destroy(idev);

	return 0;
}

static const struct of_device_id hisi_ion_match_table[] = {
	{.compatible = "hisilicon,hisi-ion"},
	{},
};

static struct platform_driver hisi_ion_driver = {
	.probe = hisi_ion_probe,
	.remove = hisi_ion_remove,
	.driver = {
		.name = "ion-hisi",
		.of_match_table = hisi_ion_match_table,
	},
};

static int __init hisi_ion_init(void)
{
	int ret;

	ret = platform_driver_register(&hisi_ion_driver);
	return ret;
}

subsys_initcall(hisi_ion_init);
