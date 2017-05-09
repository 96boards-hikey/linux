/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */

#include "hisi_fb.h"

#define MAX_ITEM_OFFSET	(0x3F)
#define CMDLIST_ADDR_OFFSET	(0x3FFFF)

#define CMDLIST_HEADER_LEN	(SZ_1K)
#define CMDLIST_ITEM_LEN	(SZ_8K)
#define MAX_ITEM_INDEX	(SZ_1K)

dss_cmdlist_data_t *g_cmdlist_data = NULL;
uint32_t g_online_cmdlist_idxs = 0;
uint32_t g_offline_cmdlist_idxs = 0;

/* get cmdlist indexs */
int hisi_cmdlist_get_cmdlist_idxs(dss_overlay_t *pov_req,
				  uint32_t *cmdlist_pre_idxs,
				  uint32_t *cmdlist_idxs)
{
	uint32_t cmdlist_idxs_temp = 0;
	int i = 0;
	int k = 0;
	int m = 0;
	dss_layer_t *layer = NULL;
	dss_wb_layer_t *wb_layer = NULL;
	dss_overlay_block_t *pov_h_block_infos = NULL;
	dss_overlay_block_t *pov_h_block = NULL;
	bool no_ovl_idx = false;

	BUG_ON(pov_req == NULL);

	pov_h_block_infos = (dss_overlay_block_t *) pov_req->ov_block_infos_ptr;
	for (m = 0; m < pov_req->ov_block_nums; m++) {
		pov_h_block = &(pov_h_block_infos[m]);
		for (i = 0; i < pov_h_block->layer_nums; i++) {
			layer = &(pov_h_block->layer_infos[i]);

			if (layer->need_cap & (CAP_BASE | CAP_DIM | CAP_PURE_COLOR))
				continue;

			if (layer->chn_idx == DSS_RCHN_V2) {
				cmdlist_idxs_temp |= (1 << DSS_CMDLIST_V2);
			} else {
				cmdlist_idxs_temp |= (1 << layer->chn_idx);
			}
		}
	}

	if (pov_req->wb_enable == 1) {
		for (k = 0; k < pov_req->wb_layer_nums; k++) {
			wb_layer = &(pov_req->wb_layer_infos[k]);

			if (wb_layer->chn_idx == DSS_WCHN_W2) {
				no_ovl_idx = true;
				cmdlist_idxs_temp |= (1 << DSS_CMDLIST_W2);
			} else {
				cmdlist_idxs_temp |= (1 << wb_layer->chn_idx);
			}
		}
	}

	if (no_ovl_idx == false) {
		cmdlist_idxs_temp |=
		    (1 << (DSS_CMDLIST_OV0 + pov_req->ovl_idx));
	}

	if (cmdlist_idxs_temp & (~HISI_DSS_CMDLIST_IDXS_MAX)) {
		HISI_FB_ERR("cmdlist_idxs_temp(0x%x) is invalid!\n",
			    cmdlist_idxs_temp);
		return -EINVAL;
	}

	if (cmdlist_idxs && cmdlist_pre_idxs) {
		*cmdlist_idxs = cmdlist_idxs_temp;
		*cmdlist_pre_idxs &= (~(*cmdlist_idxs));
	} else if (cmdlist_idxs) {
		*cmdlist_idxs = cmdlist_idxs_temp;
	} else if (cmdlist_pre_idxs) {
		*cmdlist_pre_idxs = cmdlist_idxs_temp;
	} else {
		HISI_FB_ERR("cmdlist_idxs && cmdlist_pre_idxs is NULL!\n");
		return -EINVAL;
	}

	if (g_debug_ovl_cmdlist) {
		HISI_FB_INFO("cmdlist_pre_idxs(0x%x), cmdlist_idxs(0x%x).\n",
			     (cmdlist_pre_idxs ? *cmdlist_pre_idxs : 0),
			     (cmdlist_idxs ? *cmdlist_idxs : 0));
	}

	return 0;
}

uint32_t hisi_cmdlist_get_cmdlist_need_start(struct hisi_fb_data_type *hisifd,
					     uint32_t cmdlist_idxs)
{
	uint32_t cmdlist_idxs_temp = 0;

	BUG_ON(hisifd == NULL);

	cmdlist_idxs_temp = g_offline_cmdlist_idxs;
	g_offline_cmdlist_idxs |= cmdlist_idxs;
	cmdlist_idxs_temp = (g_offline_cmdlist_idxs & (~cmdlist_idxs_temp));

	cmdlist_idxs_temp |= (cmdlist_idxs & g_online_cmdlist_idxs);
	g_online_cmdlist_idxs &= (~cmdlist_idxs_temp);

	if (g_debug_ovl_cmdlist) {
		HISI_FB_INFO
		    ("g_online_cmdlist_idxs=0x%x, cmdlist_idxs_need_start=0x%x\n",
		     g_online_cmdlist_idxs, cmdlist_idxs_temp);
	}

	return cmdlist_idxs_temp;
}

/*
 ** data0: addr0[17:0]
 ** data1: addr0[17:0] + addr1[5:0]
 ** data2: addr0[17:0] + addr2[5:0]
 **
 ** cnt[1:0]:
 ** 2'b00:	reg0
 ** 2'b01: reg0, reg1
 ** 2'b10: reg0, reg1, reg2
 ** 2'b11: ((inp32(addr0) & data1) | data2) -> addr0
 */
void hisi_cmdlist_set_reg(struct hisi_fb_data_type *hisifd, char __iomem *addr,
			  uint32_t value, uint8_t bw, uint8_t bs)
{
	uint32_t mask = (1 << bw) - 1;
	dss_cmdlist_node_t *node = NULL;
	int cmdlist_idx = -1;
	int index = 0;
	uint32_t new_addr = 0;
	uint32_t old_addr = 0;
	int condition = 0;

	BUG_ON(addr == NULL);
	BUG_ON(hisifd == NULL);

	cmdlist_idx = hisifd->cmdlist_idx;
	BUG_ON((cmdlist_idx < 0) || (cmdlist_idx >= HISI_DSS_CMDLIST_MAX));

	node =
	    list_entry(hisifd->cmdlist_data->cmdlist_head_temp[cmdlist_idx].prev,
			   dss_cmdlist_node_t, list_node);
	BUG_ON(node == NULL);

	if (node->node_type == CMDLIST_NODE_NOP) {
		HISI_FB_ERR("can't set register value to NOP node!");
		return;
	}

	index = node->item_index;
	new_addr = (uint32_t) (addr - hisifd->dss_base + hisifd->dss_base_phy);
	new_addr = (new_addr >> 2) & CMDLIST_ADDR_OFFSET;
	old_addr = node->list_item[index].reg_addr.ul32 & CMDLIST_ADDR_OFFSET;
	condition = (((new_addr - old_addr) < MAX_ITEM_OFFSET)
		     && (new_addr >= old_addr));

	if (bw != 32) {
		if (node->item_flag != 0)
			index++;

		node->list_item[index].reg_addr.bits.add0 = new_addr;
		node->list_item[index].data0 = value;
		node->list_item[index].data1 = ~(mask << bs);
		node->list_item[index].data2 = (mask & value) << bs;
		node->list_item[index].reg_addr.bits.cnt = 3;
		node->item_flag = 3;
	} else {
		if (node->item_flag == 0) {
			node->list_item[index].reg_addr.bits.add0 = new_addr;
			node->list_item[index].data0 = value;
			node->list_item[index].reg_addr.bits.cnt = 0;
			node->item_flag = 1;
		} else if (node->item_flag == 1 && condition) {
			node->list_item[index].reg_addr.bits.add1 =
			    new_addr - old_addr;
			node->list_item[index].data1 = value;
			node->list_item[index].reg_addr.bits.cnt = 1;
			node->item_flag = 2;
		} else if (node->item_flag == 2 && condition) {
			node->list_item[index].reg_addr.bits.add2 =
			    new_addr - old_addr;
			node->list_item[index].data2 = value;
			node->list_item[index].reg_addr.bits.cnt = 2;
			node->item_flag = 3;
		} else {
			index++;
			node->list_item[index].reg_addr.bits.add0 = new_addr;
			node->list_item[index].data0 = value;
			node->list_item[index].reg_addr.bits.cnt = 0;
			node->item_flag = 1;
		}
	}

	BUG_ON(index >= MAX_ITEM_INDEX);

	node->item_index = index;
	node->list_header->total_items.bits.count = node->item_index + 1;
}

/*
 ** flush cache for cmdlist, make sure that
 ** cmdlist has writen through to memory before config register
 */
void hisi_cmdlist_flush_cache(struct hisi_fb_data_type *hisifd,
			      struct ion_client *ion_client,
			      uint32_t cmdlist_idxs)
{
	uint32_t i = 0;
	uint32_t cmdlist_idxs_temp = 0;
	dss_cmdlist_node_t *node = NULL;
	dss_cmdlist_node_t *_node_ = NULL;
	struct sg_table *table = NULL;
	struct list_head *cmdlist_heads = NULL;

	BUG_ON(hisifd == NULL);

	cmdlist_idxs_temp = cmdlist_idxs;

	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if ((cmdlist_idxs_temp & 0x1) == 0x1) {
			cmdlist_heads =
			    &(hisifd->cmdlist_data->cmdlist_head_temp[i]);
			if (!cmdlist_heads) {
				HISI_FB_ERR("cmdlist_data is NULL!\n");
				continue;
			}

			list_for_each_entry_safe_reverse(node, _node_,
							 cmdlist_heads,
							 list_node) {

				if (!node->header_ion_handle) {
					HISI_FB_ERR
					    ("header_ion_handle is NULL!\n");
				} else {
					table = ion_sg_table(ion_client,
							 node->header_ion_handle);
					BUG_ON(table == NULL);
					dma_sync_sg_for_device(NULL, table->sgl,
							       table->nents,
							       DMA_TO_DEVICE);
				}

				if (!node->item_ion_handle) {
					HISI_FB_ERR("item_ion_handle is NULL!\n");
				} else {
					table = ion_sg_table(ion_client,
							 node->item_ion_handle);
					BUG_ON(table == NULL);
					dma_sync_sg_for_device(NULL, table->sgl,
							       table->nents,
							       DMA_TO_DEVICE);
				}
			}
		}

		cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
	}
}

dss_cmdlist_node_t *hisi_cmdlist_node_alloc(struct ion_client *ion_client)
{
	int ret = 0;
	dss_cmdlist_node_t *node = NULL;
	size_t header_len = CMDLIST_HEADER_LEN;
	size_t item_len = CMDLIST_ITEM_LEN;

	BUG_ON(ion_client == NULL);

	node =
	    (dss_cmdlist_node_t *) kzalloc(sizeof(dss_cmdlist_node_t),
					   GFP_KERNEL);
	if (IS_ERR(node)) {
		HISI_FB_ERR("failed to alloc dss_cmdlist_node_t!");
		goto err_alloc_cmdlist_node;
	}

	memset(node, 0, sizeof(dss_cmdlist_node_t));

	/*alloc buffer for header */
	node->header_ion_handle =
	    ion_alloc(ion_client, header_len, 0, ION_HEAP(ION_GRALLOC_HEAP_ID), 0);
	if (IS_ERR(node->header_ion_handle)) {
		HISI_FB_ERR("failed to ion_alloc node->header_ion_handle!");
		goto err_header_ion_handle;
	}

	node->list_header =
	    (cmd_header_t *) ion_map_kernel(ion_client,
					    node->header_ion_handle);
	if (!node->list_header) {
		HISI_FB_ERR("failed to ion_map_kernel node->list_header!");
		goto err_header_ion_map;
	}
	memset(node->list_header, 0, header_len);

	ret =
	    ion_phys(ion_client, node->header_ion_handle, &node->header_phys,
		     &header_len);
	if (ret < 0) {
		HISI_FB_ERR("failed to ion_phys node->header_phys!");
		goto err_header_ion_phys;
	}

	/*alloc buffer for items */
	node->item_ion_handle =
	    ion_alloc(ion_client, item_len, 0, ION_HEAP(ION_GRALLOC_HEAP_ID), 0);
	if (!node->item_ion_handle) {
		HISI_FB_ERR("failed to ion_alloc node->item_ion_handle!");
		goto err_item_ion_handle;
	}

	node->list_item =
	    (cmd_item_t *) ion_map_kernel(ion_client, node->item_ion_handle);
	if (!node->list_item) {
		HISI_FB_ERR("failed to ion_map_kernel node->list_item!");
		goto err_item_ion_map;
	}

	memset(node->list_item, 0, item_len);
	ret =
	    ion_phys(ion_client, node->item_ion_handle, &node->item_phys,
		     &item_len);
	if (ret < 0) {
		HISI_FB_ERR("failed to ion_phys node->item_phys!");
		goto err_item_ion_phys;
	}

	/* fill node info */
	node->item_flag = 0;
	node->item_index = 0;

	node->is_used = 0;
	node->node_type = CMDLIST_NODE_NONE;
	return node;

 err_item_ion_phys:
	if (node->item_ion_handle)
		ion_unmap_kernel(ion_client, node->item_ion_handle);
 err_item_ion_map:
	if (node->item_ion_handle)
		ion_free(ion_client, node->item_ion_handle);
 err_item_ion_handle:
 err_header_ion_phys:
	if (node->header_ion_handle)
		ion_unmap_kernel(ion_client, node->header_ion_handle);
 err_header_ion_map:
	if (node->header_ion_handle)
		ion_free(ion_client, node->header_ion_handle);
 err_header_ion_handle:
	if (node)
		kfree(node);
 err_alloc_cmdlist_node:
	return NULL;
}

void hisi_cmdlist_node_free(struct ion_client *ion_client,
			    dss_cmdlist_node_t *node)
{
	BUG_ON(ion_client == NULL);
	BUG_ON(node == NULL);

	if (node->header_ion_handle) {
		ion_unmap_kernel(ion_client, node->header_ion_handle);
		ion_free(ion_client, node->header_ion_handle);
	}

	if (node->item_ion_handle) {
		ion_unmap_kernel(ion_client, node->item_ion_handle);
		ion_free(ion_client, node->item_ion_handle);
	}

	kfree(node);
	node = NULL;
}

static dss_cmdlist_node_t *hisi_cmdlist_get_free_node(dss_cmdlist_node_t *
						      node[], int *id)
{
	int i = 0;

	for (i = 0; i < HISI_DSS_CMDLIST_NODE_MAX; i++) {
		if (node[i] && (node[i]->is_used == 0)) {
			node[i]->is_used = 1;
			*id = i + 1;
			return node[i];
		}
	}

	return NULL;
}

int hisi_cmdlist_add_nop_node(struct hisi_fb_data_type *hisifd,
			      uint32_t cmdlist_idxs, int pending, int reserved)
{
	dss_cmdlist_node_t *node = NULL;
	uint32_t cmdlist_idxs_temp = 0;
	int i = 0;
	int id = 0;

	BUG_ON(hisifd == NULL);

	cmdlist_idxs_temp = cmdlist_idxs;

	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if ((cmdlist_idxs_temp & 0x1) == 0x1) {
			node =
			    hisi_cmdlist_get_free_node(hisifd->cmdlist_data->
						       cmdlist_nodes_temp[i],
						       &id);
			if (!node) {
				HISI_FB_ERR
				    ("failed to hisi_get_free_cmdlist_node!\n");
				return -EINVAL;
			}

			node->list_header->flag.bits.id = id;
			node->list_header->flag.bits.nop = 0x1;
			node->list_header->flag.bits.pending =
			    pending ? 0x1 : 0x0;
			node->list_header->flag.bits.valid_flag =
			    CMDLIST_NODE_VALID;
			node->list_header->next_list = node->header_phys;

			node->is_used = 1;
			node->node_type = CMDLIST_NODE_NOP;
			node->reserved = reserved ? 0x1 : 0x0;

			/*add this nop to list */
			list_add_tail(&(node->list_node),
				      &(hisifd->cmdlist_data->cmdlist_head_temp[i]));

			if (node->list_node.prev !=
			    &(hisifd->cmdlist_data->cmdlist_head_temp[i])) {
				dss_cmdlist_node_t *pre_node = NULL;
				pre_node =
				    list_entry(node->list_node.prev,
					       dss_cmdlist_node_t, list_node);
				pre_node->list_header->next_list =
				    node->header_phys;
				if (node->list_header->flag.bits.pending == 0x1) {
					pre_node->reserved = 0x0;
				}

				pre_node->list_header->flag.bits.task_end = 0x1;

				if (g_debug_ovl_cmdlist) {
					HISI_FB_DEBUG
					    ("i = %d, next_list = 0x%x\n", i,
					     (uint32_t) (node->header_phys));
				}
			}
		}

		cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
	}

	return 0;
}

int hisi_cmdlist_add_new_node(struct hisi_fb_data_type *hisifd,
			      uint32_t cmdlist_idxs, int pending, int task_end,
			      int remove, int last, uint32_t wb_type)
{
	char __iomem *cmdlist_base = NULL;
	dss_cmdlist_node_t *node = NULL;
	uint32_t cmdlist_idxs_temp = 0;
	int i = 0;
	int id = 0;

	BUG_ON(hisifd == NULL);

	cmdlist_base = hisifd->dss_base + DSS_CMDLIST_OFFSET;
	cmdlist_idxs_temp = cmdlist_idxs;

	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if ((cmdlist_idxs_temp & 0x1) == 0x1) {
			node =
			    hisi_cmdlist_get_free_node(hisifd->cmdlist_data->
						       cmdlist_nodes_temp[i],
						       &id);
			if (!node) {
				HISI_FB_ERR
				    ("failed to hisi_get_free_cmdnode!\n");
				return -EINVAL;
			}

			/*fill the header and item info */
			node->list_header->flag.bits.id = id;
			node->list_header->flag.bits.pending =
			    pending ? 0x1 : 0x0;

			if (i < DSS_CMDLIST_W0) {
				node->list_header->flag.bits.event_list =
				    remove ? 0x8 : (0xE + i);
			} else if (i < DSS_CMDLIST_OV0) {
				node->list_header->flag.bits.event_list =
				    remove ? 0x8 : (0x16 + i);
			} else if (i == DSS_CMDLIST_V2) {
				node->list_header->flag.bits.event_list =
				    remove ? 0x8 : 0x16;
			} else if (i == DSS_CMDLIST_W2) {
				node->list_header->flag.bits.event_list =
				    remove ? 0x8 : 0x20;
			} else {
				node->list_header->flag.bits.event_list =
				    remove ? 0x8 : (0xE + i);
			}

			node->list_header->flag.bits.task_end =
			    task_end ? 0x1 : 0x0;
			node->list_header->flag.bits.last = last ? 0x1 : 0x0;

			node->list_header->flag.bits.valid_flag =
			    CMDLIST_NODE_VALID;
			node->list_header->flag.bits.exec = 0x1;
			node->list_header->list_addr = node->item_phys;
			node->list_header->next_list = node->item_phys;

			node->is_used = 1;
			node->node_type = CMDLIST_NODE_FRAME;
			node->item_flag = 0;
			node->reserved = 0;

			/* add this nop to list */
			list_add_tail(&(node->list_node),
				      &(hisifd->cmdlist_data->cmdlist_head_temp[i]));

			if (node->list_node.prev !=
			    &(hisifd->cmdlist_data->cmdlist_head_temp[i])) {
				dss_cmdlist_node_t *pre_node = NULL;
				pre_node =
				    list_entry(node->list_node.prev,
					       dss_cmdlist_node_t, list_node);
				pre_node->list_header->next_list =
				    node->header_phys;
				pre_node->reserved = 0x0;
				if (g_debug_ovl_cmdlist) {
					HISI_FB_DEBUG
					    ("i = %d, next_list = 0x%x\n", i,
					     (uint32_t) node->header_phys);
				}
			}
		}

		cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
	}

	return 0;
}

int hisi_cmdlist_del_all_node(struct list_head *cmdlist_heads)
{
	dss_cmdlist_node_t *node = NULL;
	dss_cmdlist_node_t *_node_ = NULL;

	BUG_ON(cmdlist_heads == NULL);

	list_for_each_entry_safe(node, _node_, cmdlist_heads, list_node) {
		if (node->reserved != 0x1) {
			list_del(&node->list_node);

			memset(node->list_header, 0, CMDLIST_HEADER_LEN);
			memset(node->list_item, 0, CMDLIST_ITEM_LEN);

			node->item_index = 0;
			node->item_flag = 0;
			node->node_type = CMDLIST_NODE_NONE;
			node->is_used = 0;
		}
	}

	return 0;
}

int hisi_cmdlist_check_cmdlist_state(struct hisi_fb_data_type *hisifd,
				     uint32_t cmdlist_idxs)
{
	char __iomem *cmdlist_base = NULL;
	uint32_t offset = 0;
	uint32_t tmp = 0;
	uint32_t cmdlist_idxs_temp = 0;
	int i = 0;
	int delay_count = 0;
	bool is_timeout = true;
	int ret = 0;

	BUG_ON(hisifd == NULL);

	cmdlist_base = hisifd->dss_base + DSS_CMDLIST_OFFSET;
	offset = 0x40;
	cmdlist_idxs_temp = cmdlist_idxs;

	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if ((cmdlist_idxs_temp & 0x1) == 0x1) {
			while (1) {
				tmp =
				    inp32(cmdlist_base + CMDLIST_CH0_STATUS +
					  i * offset);
				if (((tmp & 0xF) == 0x0) || delay_count > 5000) {
					is_timeout =
					    (delay_count > 5000) ? true : false;
					delay_count = 0;
					break;
				} else {
					udelay(1);
					++delay_count;
				}
			}

			if (is_timeout) {
				HISI_FB_ERR
				    ("cmdlist_ch%d not in idle state,ints=0x%x !\n",
				     i, tmp);
				ret = -1;
			}
		}

		cmdlist_idxs_temp = (cmdlist_idxs_temp >> 1);
	}

	return ret;
}

/*
 ** stop the pending state for one new frame
 ** if the current cmdlist status is e_status_wait.
 */
int hisi_cmdlist_exec(struct hisi_fb_data_type *hisifd, uint32_t cmdlist_idxs)
{
	char __iomem *cmdlist_base = NULL;
	uint32_t offset = 0;
	uint32_t tmp = 0;
	uint32_t cmdlist_idxs_temp = 0;
	int i = 0;
	int delay_count = 0;
	bool is_timeout = true;

	BUG_ON(hisifd == NULL);

	cmdlist_base = hisifd->dss_base + DSS_CMDLIST_OFFSET;
	offset = 0x40;
	cmdlist_idxs_temp = cmdlist_idxs;

	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if ((cmdlist_idxs_temp & 0x1) == 0x1) {
			while (1) {
				tmp =
				    inp32(cmdlist_base + CMDLIST_CH0_STATUS +
					  i * offset);
				if (((tmp & 0xF) == 0x0) || delay_count > 500) {
					is_timeout =
					    (delay_count > 500) ? true : false;
					delay_count = 0;
					break;
				} else {
					udelay(1);
					++delay_count;
				}
			}

			if (is_timeout) {
				HISI_FB_ERR
				    ("cmdlist_ch%d not in idle state,ints=0x%x !\n",
				     i, tmp);
				if (g_debug_ovl_cmdlist) {
					hisi_cmdlist_dump_all_node(hisifd, NULL,
								   cmdlist_idxs);
				}
			}
		}

		cmdlist_idxs_temp = (cmdlist_idxs_temp >> 1);
	}
	return 0;
}

/*
 **start cmdlist.
 **it will set cmdlist into pending state.
 */
extern uint32_t g_dss_module_ovl_base[DSS_MCTL_IDX_MAX][MODULE_OVL_MAX];
int hisi_cmdlist_config_start(struct hisi_fb_data_type *hisifd, int mctl_idx,
			      uint32_t cmdlist_idxs, uint32_t wb_compose_type)
{
	char __iomem *mctl_base = NULL;
	char __iomem *cmdlist_base = NULL;
	dss_cmdlist_node_t *cmdlist_node = NULL;
	uint32_t offset = 0;
	uint32_t list_addr = 0;
	uint32_t cmdlist_idxs_temp = 0;
	int i = 0;
	int temp = 0;
	int status_temp = 0;
	int ints_temp = 0;

	BUG_ON(hisifd == NULL);

	mctl_base =
	    hisifd->dss_base +
	    g_dss_module_ovl_base[mctl_idx][MODULE_MCTL_BASE];
	cmdlist_base = hisifd->dss_base + DSS_CMDLIST_OFFSET;
	offset = 0x40;
	cmdlist_idxs_temp = cmdlist_idxs;

	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if ((cmdlist_idxs_temp & 0x1) == 0x1) {
			status_temp =
			    inp32(cmdlist_base + CMDLIST_CH0_STATUS +
				  i * offset);
			ints_temp =
			    inp32(cmdlist_base + CMDLIST_CH0_INTS + i * offset);

			if (mctl_idx >= DSS_MCTL2) {
				cmdlist_node =
				    list_first_entry(&(hisifd->cmdlist_data_tmp
							 [wb_compose_type]->cmdlist_head_temp[i]),
						     dss_cmdlist_node_t,
						     list_node);
			} else {
				cmdlist_node =
				    list_first_entry(&(hisifd->cmdlist_data->
							  cmdlist_head_temp[i]),
						     dss_cmdlist_node_t,
						     list_node);
			}

			list_addr = cmdlist_node->header_phys;
			if (g_debug_ovl_cmdlist) {
				HISI_FB_INFO
				    ("list_addr:0x%x, i=%d, ints_temp=0x%x\n",
				     list_addr, i, ints_temp);
			}

			temp |= (1 << i);
			outp32(cmdlist_base + CMDLIST_ADDR_MASK_EN, BIT(i));
			if (g_debug_set_reg_val) {
				HISI_FB_INFO("writel: [%p] = 0x%lx\n",
					     cmdlist_base +
					     CMDLIST_ADDR_MASK_EN, BIT(i));
			}

			set_reg(cmdlist_base + CMDLIST_CH0_CTRL + i*offset,
					mctl_idx, 3, 2);
			if (mctl_idx <= DSS_MCTL1) {
				set_reg(cmdlist_base + CMDLIST_CH0_CTRL + i*offset, 0x1, 1, 6);
			} else {
				set_reg(cmdlist_base + CMDLIST_CH0_CTRL + i*offset, 0x0, 1, 6);
			}

			set_reg(cmdlist_base + CMDLIST_CH0_STAAD + i*offset,
					list_addr, 32, 0);
			set_reg(cmdlist_base + CMDLIST_CH0_CTRL + i*offset, 0x1, 1, 0);
			if ((mctl_idx <= DSS_MCTL1)
			    && ((ints_temp & 0x2) == 0x2)) {
				set_reg(cmdlist_base + CMDLIST_SWRST, 0x1, 1, i);
			}

			if (mctl_idx >= DSS_MCTL2) {
				if (((status_temp & 0xF) == 0x0)
				    || ((ints_temp & 0x2) == 0x2)) {
					set_reg(cmdlist_base + CMDLIST_SWRST,
						0x1, 1, i);
				} else {
					HISI_FB_INFO
					    ("i=%d, status_temp=0x%x, ints_temp=0x%x\n",
					     i, status_temp, ints_temp);
				}
			}
		}
		cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
	}

	outp32(cmdlist_base + CMDLIST_ADDR_MASK_DIS, temp);
	if (g_debug_set_reg_val) {
		HISI_FB_INFO("writel: [%p] = 0x%x\n",
			     cmdlist_base + CMDLIST_ADDR_MASK_DIS, temp);
	}

	if (mctl_idx >= DSS_MCTL2) {
		set_reg(mctl_base + MCTL_CTL_ST_SEL, 0x1, 1, 0);
		set_reg(mctl_base + MCTL_CTL_SW_ST, 0x1, 1, 0);
	}

	return 0;
}

void hisi_cmdlist_config_mif_reset(struct hisi_fb_data_type *hisifd,
				   dss_overlay_t *pov_req,
				   uint32_t cmdlist_idxs, int mctl_idx)
{
	char __iomem *dss_base = NULL;
	char __iomem *tmp_base = NULL;

	uint32_t cmdlist_idxs_temp = 0;
	int delay_count = 0;
	bool is_timeout = true;
	int i = 0;
	int j = 0;
	int mif_sub_ch_nums = 4;
	int tmp = 0;
	int mif_nums_max = 0;

	BUG_ON(hisifd == NULL);
	BUG_ON(pov_req == NULL);

	dss_base = hisifd->dss_base;

	if (mctl_idx <= DSS_MCTL1) {
		mif_nums_max = DSS_WCHN_W0;
	} else {
		mif_nums_max = DSS_CHN_MAX;
	}

	if (mctl_idx == DSS_MCTL5) {
		for (i = DSS_RCHN_V2; i < DSS_CHN_MAX_DEFINE; i++) {
			is_timeout = false;

			while (1) {
				for (j = 1; j <= mif_sub_ch_nums; j++) {
					tmp |=
					    inp32(dss_base + DSS_MIF_OFFSET +
						  MIF_STAT1 +
						  0x10 * (i * mif_sub_ch_nums +j));
				}

				if (((tmp & 0x1f) == 0x0) || delay_count > 500) {
					is_timeout =
					    (delay_count > 500) ? true : false;
					delay_count = 0;
					break;
				} else {
					udelay(10);
					++delay_count;
				}
			}

			if (is_timeout) {
				HISI_FB_ERR("mif_ch%d MIF_STAT1=0x%x !\n", i,
					    tmp);
			}
		}

		tmp_base = hisifd->dss_module.mif_ch_base[DSS_RCHN_V2];
		if (tmp_base) {
			set_reg(tmp_base + MIF_CTRL0, 0x0, 1, 0);
		}

		tmp_base = hisifd->dss_module.mif_ch_base[DSS_WCHN_W2];
		if (tmp_base) {
			set_reg(tmp_base + MIF_CTRL0, 0x0, 1, 0);
		}
	} else {
		cmdlist_idxs_temp = cmdlist_idxs;
		for (i = 0; i < mif_nums_max; i++) {
			if ((cmdlist_idxs_temp & 0x1) == 0x1) {
				is_timeout = false;

				while (1) {
					for (j = 1; j <= mif_sub_ch_nums; j++) {
						tmp |=
						    inp32(dss_base + DSS_MIF_OFFSET + MIF_STAT1 +
							  0x10 * (i * mif_sub_ch_nums + j));
					}
					if (((tmp & 0x1f) == 0x0)
					    || delay_count > 500) {
						is_timeout =
						    (delay_count > 500) ? true : false;
						delay_count = 0;
						break;
					} else {
						udelay(10);
						++delay_count;
					}
				}

				if (is_timeout) {
					HISI_FB_ERR
					    ("mif_ch%d MIF_STAT1=0x%x !\n", i, tmp);
				}
			}

			cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
		}

		cmdlist_idxs_temp = cmdlist_idxs;
		for (i = 0; i < mif_nums_max; i++) {
			if ((cmdlist_idxs_temp & 0x1) == 0x1) {
				tmp_base = hisifd->dss_module.mif_ch_base[i];
				if (tmp_base) {
					set_reg(tmp_base + MIF_CTRL0, 0x0, 1,
						0);
				}
			}
			cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
		}
	}
	mdelay(5);

	if (mctl_idx == DSS_MCTL5) {
		tmp_base = hisifd->dss_module.mif_ch_base[DSS_RCHN_V2];
		if (tmp_base) {
			set_reg(tmp_base + MIF_CTRL0, 0x1, 1, 0);
		}

		tmp_base = hisifd->dss_module.mif_ch_base[DSS_WCHN_W2];
		if (tmp_base) {
			set_reg(tmp_base + MIF_CTRL0, 0x1, 1, 0);
		}
	} else {
		cmdlist_idxs_temp = cmdlist_idxs;
		for (i = 0; i < mif_nums_max; i++) {
			if ((cmdlist_idxs_temp & 0x1) == 0x1) {
				tmp_base = hisifd->dss_module.mif_ch_base[i];
				if (tmp_base) {
					set_reg(tmp_base + MIF_CTRL0, 0x1, 1,
						0);
				}
			}
			cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
		}
	}
}

void hisi_cmdlist_config_reset(struct hisi_fb_data_type *hisifd,
			       dss_overlay_t *pov_req, uint32_t cmdlist_idxs)
{
	char __iomem *dss_base = NULL;
	char __iomem *cmdlist_base = NULL;
	char __iomem *tmp_base = NULL;
	struct hisi_panel_info *pinfo = NULL;

	uint32_t offset = 0;
	uint32_t cmdlist_idxs_temp = 0;
	int i = 0;
	int ovl_idx = 0;
	int mctl_idx = 0;
	int ints_temp = 0;
	int start_sel = 0;
	uint32_t start_sel_temp = 0;

	BUG_ON(hisifd == NULL);
	BUG_ON(pov_req == NULL);

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	dss_base = hisifd->dss_base;
	cmdlist_base = dss_base + DSS_CMDLIST_OFFSET;
	ovl_idx = pov_req->ovl_idx;
	pinfo = &(hisifd->panel_info);

	if (cmdlist_idxs == 0) return;

	mctl_idx = ovl_idx;
	if (pov_req->wb_compose_type == DSS_WB_COMPOSE_COPYBIT) {
		mctl_idx = DSS_MCTL5;
	}

	offset = 0x40;
	cmdlist_idxs_temp = HISI_DSS_CMDLIST_IDXS_MAX;
	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if ((cmdlist_idxs_temp & 0x1) == 0x1) {
			ints_temp =
			    inp32(cmdlist_base + CMDLIST_CH0_INTS + i * offset);
			start_sel =
			    inp32(cmdlist_base + CMDLIST_CH0_CTRL + i * offset);

			if (((ints_temp & 0x2) == 0x2)
			    && ((start_sel & 0x1c) == 0)) {
				set_reg(cmdlist_base + CMDLIST_CH0_CTRL + i * offset, 0x6, 3, 2);
				start_sel_temp |= (1 << i);
			}
		}
		cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
	}

	tmp_base = hisifd->dss_module.mctl_base[mctl_idx];
	if (tmp_base) {
		set_reg(tmp_base + MCTL_CTL_CLEAR, 0x1, 1, 0);
	}

	hisi_cmdlist_config_mif_reset(hisifd, pov_req, cmdlist_idxs, mctl_idx);
	cmdlist_idxs_temp = start_sel_temp;
	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if ((cmdlist_idxs_temp & 0x1) == 0x1) {
			set_reg(cmdlist_base + CMDLIST_CH0_CTRL + i * offset, mctl_idx, 3, 2);
		}
		cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
	}

	if (mctl_idx >= DSS_MCTL2) {
		offset = 0x40;
		cmdlist_idxs_temp = cmdlist_idxs;
		for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
			if ((cmdlist_idxs_temp & 0x1) == 0x1) {
				set_reg(cmdlist_base + CMDLIST_CH0_CTRL +
					i * offset, 0x6, 3, 2);
				set_reg(cmdlist_base + CMDLIST_CH0_CTRL +
					i * offset, 0x0, 1, 0);
			}
			cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
		}
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);
	return;
}

int hisi_cmdlist_config_stop(struct hisi_fb_data_type *hisifd,
			     uint32_t cmdlist_pre_idxs)
{
	dss_overlay_t *pov_req = NULL;
	char __iomem *cmdlist_base = NULL;
	int i = 0;
	uint32_t tmp = 0;
	uint32_t offset = 0;
	int ret = 0;

	BUG_ON(hisifd == NULL);
	pov_req = &(hisifd->ov_req);
	cmdlist_base = hisifd->dss_base + DSS_CMDLIST_OFFSET;
	offset = 0x40;

	ret =
	    hisi_cmdlist_add_new_node(hisifd, cmdlist_pre_idxs, 0, 1, 1, 1, 0);
	if (ret != 0) {
		HISI_FB_ERR("fb%d, hisi_cmdlist_add_new_node err:%d \n",
			    hisifd->index, ret);
		goto err_return;
	}

	for (i = 0; i < DSS_WCHN_W0; i++) {
		tmp = (0x1 << i);
		hisifd->cmdlist_idx = i;

		if ((cmdlist_pre_idxs & tmp) == tmp) {
			hisifd->set_reg(hisifd,
					hisifd->dss_module.mctl_base[pov_req->
								     ovl_idx] +
					MCTL_CTL_MUTEX_RCH0 + i * 0x4, 0, 32,
					0);
			hisifd->set_reg(hisifd, cmdlist_base + CMDLIST_CH0_CTRL + i * offset, 0x6, 3, 2);
		}
	}

	return 0;

 err_return:
	return ret;
}

void hisi_dss_cmdlist_qos_on(struct hisi_fb_data_type *hisifd)
{
	char __iomem *cmdlist_base = NULL;

	BUG_ON(hisifd == NULL);

	cmdlist_base = hisifd->dss_base + DSS_CMDLIST_OFFSET;
	set_reg(cmdlist_base + CMDLIST_CTRL, 0x3, 2, 4);
}

void hisi_dump_cmdlist_node_items(cmd_item_t *item, uint32_t count)
{
	uint32_t index = 0;
	uint32_t addr = 0;

	for (index = 0; index < count; index++) {
		addr = item[index].reg_addr.bits.add0;
		addr = addr & CMDLIST_ADDR_OFFSET;
		addr = addr << 2;
		HISI_FB_INFO
		    ("set addr:0x%x value:0x%x add1:0x%x value:0x%x "
		     "add2:0x%x value:0x%x \n",
		     addr, item[index].data0,
		     item[index].reg_addr.bits.add1 << 2, item[index].data1,
		     item[index].reg_addr.bits.add2 << 2, item[index].data2);
	}
}

static void hisi_dump_cmdlist_content(struct list_head *cmdlist_head,
				      char *filename, uint32_t addr)
{
	dss_cmdlist_node_t *node = NULL;
	dss_cmdlist_node_t *_node_ = NULL;

	BUG_ON(cmdlist_head == NULL);
	BUG_ON(filename == NULL);

	if (g_dump_cmdlist_content == 0)
		return;

	HISI_FB_INFO("%s\n", filename);

	list_for_each_entry_safe(node, _node_, cmdlist_head, list_node) {
		if (node->header_phys == addr) {
			hisifb_save_file(filename, (char *)(node->list_header),
					 CMDLIST_HEADER_LEN);
		}

		if (node->item_phys == addr) {
			hisifb_save_file(filename, (char *)(node->list_item),
					 CMDLIST_ITEM_LEN);
		}
	}
}

static void hisi_dump_cmdlist_one_node(struct list_head *cmdlist_head,
				       uint32_t cmdlist_idx)
{
	dss_cmdlist_node_t *node = NULL;
	dss_cmdlist_node_t *_node_ = NULL;
	uint32_t count = 0;
	int i = 0;
	char filename[256] = { 0 };

	BUG_ON(cmdlist_head == NULL);

	list_for_each_entry_safe(node, _node_, cmdlist_head, list_node) {
		if (node->node_type == CMDLIST_NODE_NOP) {
			HISI_FB_INFO("node type = NOP node\n");
		} else if (node->node_type == CMDLIST_NODE_FRAME) {
			HISI_FB_INFO("node type = Frame node\n");
		}

		HISI_FB_INFO
		    ("\t qos  | flag | pending | tast_end | last  | event_list | list_addr  | next_list  | count | id | is_used | reserved | cmdlist_idx\n");
		HISI_FB_INFO
		    ("\t ------+---------+------------+------------+------------+------------\n");
		HISI_FB_INFO
		    ("\t 0x%2x | 0x%2x |0x%6x | 0x%5x | 0x%3x | 0x%8x | 0x%8x | 0x%8x | 0x%3x | 0x%2x | 0x%2x | 0x%2x | 0x%2x\n",
		     node->list_header->flag.bits.qos,
		     node->list_header->flag.bits.valid_flag,
		     node->list_header->flag.bits.pending,
		     node->list_header->flag.bits.task_end,
		     node->list_header->flag.bits.last,
		     node->list_header->flag.bits.event_list,
		     node->list_header->list_addr, node->list_header->next_list,
		     node->list_header->total_items.bits.count,
		     node->list_header->flag.bits.id, node->is_used,
		     node->reserved, cmdlist_idx);

		if (i == 0) {
			snprintf(filename, 256,
				 "/data/dssdump/list_start_0x%x.txt",
				 (uint32_t) node->header_phys);
			hisi_dump_cmdlist_content(cmdlist_head, filename,
						  node->header_phys);
		}
#if 0
		if ((node->list_header->next_list != 0x0) &&
		    (node->list_header->next_list != 0xFFFFFFFF)) {
			snprintf(filename, 256,
				 "/data/dssdump/next_list_0x%x.txt",
				 node->list_header->next_list);
			hisi_dump_cmdlist_content(cmdlist_head, filename,
						  node->list_header->next_list);
		}

		if ((node->list_header->list_addr != 0x0) &&
		    (node->list_header->list_addr != 0xFFFFFFFF)) {
			snprintf(filename, 256,
				 "/data/dssdump/list_addr_0x%x.txt",
				 node->list_header->list_addr);
			hisi_dump_cmdlist_content(cmdlist_head, filename,
						  node->list_header->list_addr);
		}
#endif
		count = node->list_header->total_items.bits.count;
		hisi_dump_cmdlist_node_items(node->list_item, count);

		i++;
	}
}

int hisi_cmdlist_dump_all_node(struct hisi_fb_data_type *hisifd,
			       dss_overlay_t *pov_req, uint32_t cmdlist_idxs)
{
	int i = 0;
	uint32_t cmdlist_idxs_temp = 0;
	uint32_t wb_compose_type = 0;

	BUG_ON(hisifd == NULL);

	if (pov_req) {
		if (pov_req->wb_enable)
			wb_compose_type = pov_req->wb_compose_type;
	}

	cmdlist_idxs_temp = cmdlist_idxs;
	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if (0x1 == (cmdlist_idxs_temp & 0x1)) {
			if (pov_req && pov_req->wb_enable) {
				hisi_dump_cmdlist_one_node(&(hisifd->cmdlist_data_tmp
							    [wb_compose_type]->cmdlist_head_temp[i]), i);
			} else {
				hisi_dump_cmdlist_one_node(&
							   (hisifd->cmdlist_data->cmdlist_head_temp[i]), i);
			}
		}
		cmdlist_idxs_temp = cmdlist_idxs_temp >> 1;
	}

	return 0;
}

int hisi_cmdlist_del_node(struct hisi_fb_data_type *hisifd,
			  dss_overlay_t *pov_req, uint32_t cmdlist_idxs)
{
	int i = 0;
	uint32_t cmdlist_idxs_temp = 0;
	uint32_t wb_compose_type = 0;

	BUG_ON(hisifd == NULL);

	if (pov_req) {
		if (pov_req->wb_enable)
			wb_compose_type = pov_req->wb_compose_type;
	}

	cmdlist_idxs_temp = cmdlist_idxs;
	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		if ((cmdlist_idxs_temp & 0x1) == 0x1) {
			if (pov_req && pov_req->wb_enable) {
				hisi_cmdlist_del_all_node(&(hisifd->cmdlist_data_tmp
							   [wb_compose_type]->cmdlist_head_temp[i]));
			} else {
				hisi_cmdlist_del_all_node(&
							  (hisifd->cmdlist_data->cmdlist_head_temp[i]));
			}
		}
		cmdlist_idxs_temp = (cmdlist_idxs_temp >> 1);
	}

	return 0;
}

static dss_cmdlist_data_t *hisi_cmdlist_data_alloc(struct hisi_fb_data_type
						   *hisifd)
{
	int i = 0;
	int j = 0;
	dss_cmdlist_data_t *cmdlist_data = NULL;

	BUG_ON(hisifd == NULL);

	cmdlist_data =
	    (dss_cmdlist_data_t *) kmalloc(sizeof(dss_cmdlist_data_t),
					   GFP_ATOMIC);
	if (cmdlist_data) {
		memset(cmdlist_data, 0, sizeof(dss_cmdlist_data_t));
	} else {
		HISI_FB_ERR("failed to kmalloc cmdlist_data!\n");
		return NULL;
	}

	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		INIT_LIST_HEAD(&(cmdlist_data->cmdlist_head_temp[i]));

		for (j = 0; j < HISI_DSS_CMDLIST_NODE_MAX; j++) {
			cmdlist_data->cmdlist_nodes_temp[i][j] =
			    hisi_cmdlist_node_alloc(hisifd->ion_client);
			if (cmdlist_data->cmdlist_nodes_temp[i][j] == NULL) {
				HISI_FB_ERR
				    ("failed to hisi_cmdlist_node_alloc!\n");
				kfree(cmdlist_data);
				return NULL;
			}
		}
	}

	return cmdlist_data;
}

static void hisi_cmdlist_data_free(struct hisi_fb_data_type *hisifd,
				   dss_cmdlist_data_t *cmdlist_data)
{
	int i = 0;
	int j = 0;

	BUG_ON(hisifd == NULL);
	BUG_ON(cmdlist_data == NULL);

	for (i = 0; i < HISI_DSS_CMDLIST_MAX; i++) {
		for (j = 0; j < HISI_DSS_CMDLIST_NODE_MAX; j++) {
			hisi_cmdlist_node_free(hisifd->ion_client,
					       hisifd->cmdlist_data->cmdlist_nodes_temp[i][j]);
		}
	}
}

static dss_cmdlist_info_t *hisi_cmdlist_info_alloc(struct hisi_fb_data_type
						   *hisifd)
{
	int i = 0;
	dss_cmdlist_info_t *cmdlist_info = NULL;

	BUG_ON(hisifd == NULL);

	cmdlist_info =
	    (dss_cmdlist_info_t *) kmalloc(sizeof(dss_cmdlist_info_t), GFP_ATOMIC);
	if (cmdlist_info) {
		memset(cmdlist_info, 0, sizeof(dss_cmdlist_info_t));
	} else {
		HISI_FB_ERR("failed to kmalloc cmdlist_info!\n");
		return NULL;
	}

	sema_init(&(cmdlist_info->cmdlist_wb_common_sem), 1);

	for (i = 0; i < WB_TYPE_MAX; i++) {
		sema_init(&(cmdlist_info->cmdlist_wb_sem[i]), 1);
		init_waitqueue_head(&(cmdlist_info->cmdlist_wb_wq[i]));
		cmdlist_info->cmdlist_wb_done[i] = 0;
		cmdlist_info->cmdlist_wb_flag[i] = 0;
	}

	return cmdlist_info;
}

static dss_copybit_info_t *hisi_copybit_info_alloc(struct hisi_fb_data_type
						   *hisifd)
{
	dss_copybit_info_t *copybit_info = NULL;

	BUG_ON(hisifd == NULL);

	copybit_info =
	    (dss_copybit_info_t *) kmalloc(sizeof(dss_copybit_info_t),
					   GFP_ATOMIC);
	if (copybit_info) {
		memset(copybit_info, 0, sizeof(dss_copybit_info_t));
	} else {
		HISI_FB_ERR("failed to kmalloc copybit_info!\n");
		return NULL;
	}

	sema_init(&(copybit_info->copybit_sem), 1);

	init_waitqueue_head(&(copybit_info->copybit_wq));
	copybit_info->copybit_done = 0;

	return copybit_info;
}

void hisi_cmdlist_data_get_online(struct hisi_fb_data_type *hisifd)
{
	int tmp = 0;

	BUG_ON(hisifd == NULL);

	tmp = hisifd->frame_count % HISI_DSS_CMDLIST_DATA_MAX;
	hisifd->cmdlist_data = hisifd->cmdlist_data_tmp[tmp];
	hisi_cmdlist_del_node(hisifd, NULL, HISI_DSS_CMDLIST_IDXS_MAX);
}

int hisi_cmdlist_init(struct hisi_fb_data_type *hisifd)
{
	int ret = 0;
	int i = 0;

	BUG_ON(hisifd == NULL);

	for (i = 0; i < HISI_DSS_CMDLIST_BLOCK_MAX; i++) {
		hisifd->ov_block_rects[i] =
		    (dss_rect_t *) kmalloc(sizeof(dss_rect_t), GFP_ATOMIC);
		if (!hisifd->ov_block_rects[i]) {
			HISI_FB_ERR("ov_block_rects[%d] failed to alloc!", i);
			return -EINVAL;
		}
	}

	if (hisifd->index == AUXILIARY_PANEL_IDX) {
		hisifd->cmdlist_data_tmp[0] = hisi_cmdlist_data_alloc(hisifd);
		hisifd->cmdlist_data_tmp[1] = hisi_cmdlist_data_alloc(hisifd);
		hisifd->cmdlist_info = hisi_cmdlist_info_alloc(hisifd);
		hisifd->copybit_info = hisi_copybit_info_alloc(hisifd);
	} else {
		if (hisifd->index == PRIMARY_PANEL_IDX
		    || (hisifd->index == EXTERNAL_PANEL_IDX
			&& !hisifd->panel_info.fake_hdmi)) {
			for (i = 0; i < HISI_DSS_CMDLIST_DATA_MAX; i++) {
				hisifd->cmdlist_data_tmp[i] =
				    hisi_cmdlist_data_alloc(hisifd);
			}
		}
	}

	hisifd->cmdlist_data = hisifd->cmdlist_data_tmp[0];
	hisifd->cmdlist_idx = -1;

	return ret;
}

int hisi_cmdlist_deinit(struct hisi_fb_data_type *hisifd)
{
	int i = 0;

	BUG_ON(hisifd == NULL);

	for (i = 0; i < HISI_DSS_CMDLIST_BLOCK_MAX; i++) {
		if (hisifd->ov_block_rects[i]) {
			kfree(hisifd->ov_block_rects[i]);
			hisifd->ov_block_rects[i] = NULL;
		}
	}

	for (i = 0; i < HISI_DSS_CMDLIST_DATA_MAX; i++) {
		if (hisifd->cmdlist_data_tmp[i]) {
			hisi_cmdlist_data_free(hisifd,
					       hisifd->cmdlist_data_tmp[i]);
			kfree(hisifd->cmdlist_data_tmp[i]);
			hisifd->cmdlist_data_tmp[i] = NULL;
		}
	}

	if (hisifd->index == AUXILIARY_PANEL_IDX) {
		if (hisifd->cmdlist_info) {
			kfree(hisifd->cmdlist_info);
			hisifd->cmdlist_info = NULL;
		}

		if (hisifd->copybit_info) {
			kfree(hisifd->copybit_info);
			hisifd->copybit_info = NULL;
		}
	}

	return 0;
}
