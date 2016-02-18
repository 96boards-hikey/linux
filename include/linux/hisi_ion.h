/*
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef _LINUX_HISI_ION_H
#define _LINUX_HISI_ION_H

#include <ion/ion.h>

/**
 * These are the only ids that should be used for Ion heap ids.
 * The ids listed are the order in which allocation will be attempted
 * if specified. Don't swap the order of heap ids unless you know what
 * you are doing!
 * Id's are spaced by purpose to allow new Id's to be inserted in-between (for
 * possible fallbacks)
 */

enum ion_heap_ids {
        ION_SYSTEM_HEAP_ID = 0,
        ION_SYSTEM_CONTIG_HEAP_ID = 1,
        ION_GRALLOC_HEAP_ID = 2,
        ION_OVERLAY_HEAP_ID = 7,
        ION_FB_HEAP_ID = 10,
        ION_VPU_HEAP_ID = 11,
        ION_JPU_HEAP_ID = 12,
};


/**
 * Macro should be used with ion_heap_ids defined above.
 */
#define ION_HEAP(bit) (1 << (bit))
#define HISI_ION_NAME_LEN 16

struct ion_heap_info_data{
        char name[HISI_ION_NAME_LEN];
        phys_addr_t heap_phy;
        unsigned int  heap_size;
};

int hisi_ion_get_heap_info(unsigned int id,struct ion_heap_info_data* data);
struct ion_device * get_ion_device(void);

#endif
