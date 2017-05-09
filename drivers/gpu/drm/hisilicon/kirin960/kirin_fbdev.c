/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <drm/drmP.h>
#include <drm_crtc_helper.h>

#include <linux/ion.h>
#include <linux/hisi/hisi_ion.h>

#include "kirin_drm_drv.h"
#include "kirin_dpe_reg.h"
#include "kirin_drm_dpe_utils.h"

#include "drm_crtc.h"
#include "drm_fb_helper.h"

//#define CONFIG_HISI_FB_HEAP_CARVEOUT_USED

#define FBDEV_BUFFER_NUM 3
struct fb_dmabuf_export
{
    __u32 fd;
    __u32 flags;
};
#define FBIOGET_DMABUF  _IOR('F', 0x21, struct fb_dmabuf_export)

#define HISIFB_IOCTL_MAGIC 'M'
#define HISI_DRM_ONLINE_PLAY _IOW(HISIFB_IOCTL_MAGIC, 0x21, struct drm_dss_layer)

/*
 * fbdev funcs, to implement legacy fbdev interface on top of drm driver
 */

#define HISI_FB_ION_CLIENT_NAME	"hisi_fb_ion"

unsigned long kirin_alloc_fb_buffer(struct kirin_fbdev *fbdev, int size)
{
	struct ion_client *client = NULL;
	struct ion_handle *handle = NULL;
	size_t buf_len = 0;
	unsigned long buf_addr = 0;
	int shared_fd = -1;

	if (NULL == fbdev) {
		DRM_ERROR("fbdev is NULL!\n");
		return -EINVAL;
	}

	client = fbdev->ion_client;
	handle = fbdev->ion_handle;

	buf_len = size;

	client = hisi_ion_client_create(HISI_FB_ION_CLIENT_NAME);
	if (!client) {
		DRM_ERROR("failed to create ion client!\n");
		return -ENOMEM;
	}
	memset(&fbdev->iommu_format, 0, sizeof(struct iommu_map_format));

#ifdef CONFIG_HISI_FB_HEAP_CARVEOUT_USED
	handle = ion_alloc(client, buf_len, PAGE_SIZE, ION_HEAP(ION_GRALLOC_HEAP_ID), 0);
#else
	handle = ion_alloc(client, buf_len, PAGE_SIZE, ION_HEAP(ION_SYSTEM_HEAP_ID), 0);
#endif
	if (!handle) {
		DRM_ERROR("failed to ion_alloc!\n");
		goto err_return;
	}

	fbdev->screen_base = ion_map_kernel(client, handle);
	if (!fbdev->screen_base) {
		DRM_ERROR("failed to ion_map_kernel!\n");
		goto err_ion_map;
	}

#ifdef CONFIG_HISI_FB_HEAP_CARVEOUT_USED
	if (ion_phys(client, handle, &buf_addr, &buf_len) < 0) {
		DRM_ERROR("failed to get ion phys!\n");
		goto err_ion_get_addr;
	}
#else
	if (ion_map_iommu(client, handle, &(fbdev->iommu_format))) {
		DRM_ERROR("failed to ion_map_iommu!\n");
		goto err_ion_get_addr;
	}

	buf_addr = fbdev->iommu_format.iova_start;
#endif

	fbdev->shared_fd = shared_fd;
	fbdev->smem_start = buf_addr;
	fbdev->screen_size = buf_len;
	memset(fbdev->screen_base, 0x0, fbdev->screen_size);

	fbdev->ion_client = client;
	fbdev->ion_handle = handle;

	return buf_addr;

err_ion_get_addr:
	ion_unmap_kernel(client, handle);
err_ion_map:
	ion_free(client, handle);
err_return:
	return 0;
}

static int kirin_fbdev_mmap(struct fb_info *info, struct vm_area_struct * vma)
{
	struct sg_table *table = NULL;
	struct scatterlist *sg = NULL;
	struct page *page = NULL;
	unsigned long remainder = 0;
	unsigned long len = 0;
	unsigned long addr = 0;
	unsigned long offset = 0;
	unsigned long size = 0;
	int i = 0;
	int ret = 0;

	struct drm_fb_helper *helper = (struct drm_fb_helper *)info->par;
	struct kirin_fbdev *fbdev = to_kirin_fbdev(helper);

	if (NULL == info) {
		DRM_ERROR("info is NULL!\n");
		return -EINVAL;
	}

	if (NULL == fbdev) {
		DRM_ERROR("fbdev is NULL!\n");
		return -EINVAL;
	}

	table = ion_sg_table(fbdev->ion_client, fbdev->ion_handle);
	if ((table == NULL) || (vma == NULL)) {
		DRM_ERROR("table or vma is NULL!\n");
		return -EFAULT;
	}

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	addr = vma->vm_start;
	offset = vma->vm_pgoff * PAGE_SIZE;
	size = vma->vm_end - vma->vm_start;

	if (size > info->fix.smem_len) {
		DRM_ERROR("size=%lu is out of range(%u)!\n", size, info->fix.smem_len);
		return -EFAULT;
	}

	for_each_sg(table->sgl, sg, table->nents, i) {
		page = sg_page(sg);
		remainder = vma->vm_end - addr;
		len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
			vma->vm_page_prot);
		if (ret != 0) {
			DRM_ERROR("failed to remap_pfn_range! ret=%d\n", ret);
		}

		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}

	return 0;
}

static int kirin_dmabuf_export(struct fb_info *info, void __user *argp)
{
	int ret;
	struct drm_fb_helper *helper;
	struct kirin_fbdev *fbdev;
	struct fb_dmabuf_export dmabuf_export;

	helper = (struct drm_fb_helper *)info->par;
	fbdev = to_kirin_fbdev(helper);

	ret = copy_from_user(&dmabuf_export, argp, sizeof(struct fb_dmabuf_export));
	if (ret) {
		DRM_ERROR("copy for user failed!ret=%d.\n", ret);
		ret = -EINVAL;
	} else {
		dmabuf_export.flags = 0;
		dmabuf_export.fd = ion_share_dma_buf_fd(fbdev->ion_client, fbdev->ion_handle);
		if (dmabuf_export.fd < 0) {
			DRM_ERROR("failed to ion_share!\n");
		}

		ret = copy_to_user(argp, &dmabuf_export, sizeof(struct fb_dmabuf_export));
		if (ret) {
			DRM_ERROR("copy to user failed!ret=%d.", ret);
			ret = -EFAULT;
		}
	}

	return ret;
}

static int kirin_dss_online_compose(struct fb_info *info, void __user *argp)
{
	int ret;
	struct drm_fb_helper *helper;
	struct kirin_drm_private *priv;
	struct drm_plane *plane;

	struct drm_dss_layer layer;

	helper = (struct drm_fb_helper *)info->par;
	priv = helper->dev->dev_private;
	plane =priv->crtc[0]->primary;

	ret = copy_from_user(&layer, argp, sizeof(struct drm_dss_layer));
	if (ret) {
		DRM_ERROR("copy for user failed!ret=%d.\n", ret);
		return -EINVAL;
	}

	hisi_dss_online_play(plane, &layer);

	return ret;
}

static int kirin_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOSYS;
	void __user *argp = (void __user *)arg;

	if (NULL == info) {
		DRM_ERROR("info is NULL!\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FBIOGET_DMABUF:
		ret = kirin_dmabuf_export(info, argp);
		break;
	case HISI_DRM_ONLINE_PLAY:
		ret = kirin_dss_online_compose(info, argp);
		break;
	default:
		break;
	}

	if (ret == -ENOSYS)
		DRM_ERROR("unsupported ioctl (%x)\n", cmd);

	return ret;
}


static struct fb_ops kirin_fb_ops = {
	.owner = THIS_MODULE,

	/* Note: to properly handle manual update displays, we wrap the
	 * basic fbdev ops which write to the framebuffer
	 */
	.fb_read = drm_fb_helper_sys_read,
	.fb_write = drm_fb_helper_sys_write,
	.fb_fillrect = drm_fb_helper_sys_fillrect,
	.fb_copyarea = drm_fb_helper_sys_copyarea,
	.fb_imageblit = drm_fb_helper_sys_imageblit,
	.fb_mmap = kirin_fbdev_mmap,

	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,

	.fb_ioctl = kirin_fb_ioctl,
	.fb_compat_ioctl = kirin_fb_ioctl,
};

static int kirin_fbdev_create(struct drm_fb_helper *helper,
		struct drm_fb_helper_surface_size *sizes)
{
	struct kirin_fbdev *fbdev = to_kirin_fbdev(helper);
	struct drm_device *dev = helper->dev;
	struct drm_framebuffer *fb = NULL;
	struct fb_info *fbi = NULL;
	struct drm_mode_fb_cmd2 mode_cmd = {0};
	int ret, size;
	unsigned int bytes_per_pixel;

	DRM_DEBUG("create fbdev: %dx%d@%d (%dx%d)\n", sizes->surface_width,
			sizes->surface_height, sizes->surface_bpp,
			sizes->fb_width, sizes->fb_height);

	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
			sizes->surface_depth);

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height * FBDEV_BUFFER_NUM;

	bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);
	mode_cmd.pitches[0] = sizes->surface_width * bytes_per_pixel;
	//mode_cmd.pitches[0] = align_pitch(mode_cmd.width, sizes->surface_bpp);

	/* allocate backing bo */
	size = mode_cmd.pitches[0] * mode_cmd.height;
	DRM_DEBUG("allocating %d bytes for fb %d", size, dev->primary->index);

	fb = kirin_framebuffer_init(dev, &mode_cmd);
	if (IS_ERR(fb)) {
		dev_err(dev->dev, "failed to allocate fb\n");
		/* note: if fb creation failed, we can't rely on fb destroy
		 * to unref the bo:
		 */
		ret = PTR_ERR(fb);
		goto fail;
	}

	mutex_lock(&dev->struct_mutex);

	fbdev->ion_client = NULL;
	fbdev->ion_handle = NULL;
	fbdev->screen_base = NULL;
	fbdev->smem_start = 0;
	fbdev->screen_size = 0;
	memset(&fbdev->iommu_format, 0, sizeof(struct iommu_map_format));

	kirin_alloc_fb_buffer(fbdev, size);

	fbi = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(fbi)) {
		dev_err(dev->dev, "failed to allocate fb info\n");
		ret = PTR_ERR(fbi);
		goto fail_unlock;
	}

	DRM_DEBUG("fbi=%p, dev=%p", fbi, dev);

	fbdev->fb = fb;
	helper->fb = fb;

	fbi->par = helper;
	fbi->flags = FBINFO_DEFAULT;
	fbi->fbops = &kirin_fb_ops;

	strcpy(fbi->fix.id, "dss");

	drm_fb_helper_fill_fix(fbi, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(fbi, helper, sizes->fb_width, sizes->fb_height);

	dev->mode_config.fb_base = fbdev->smem_start;
	fbi->screen_base = fbdev->screen_base;
	fbi->screen_size = fbdev->screen_size;
	fbi->fix.smem_start = fbdev->smem_start;
	fbi->fix.smem_len = fbdev->screen_size;

	DRM_DEBUG("par=%p, %dx%d", fbi->par, fbi->var.xres, fbi->var.yres);
	DRM_DEBUG("allocated %dx%d fb", fbdev->fb->width, fbdev->fb->height);

	mutex_unlock(&dev->struct_mutex);

	return 0;

fail_unlock:
	mutex_unlock(&dev->struct_mutex);
fail:
	if (ret) {
		if (fb) {
			drm_framebuffer_unregister_private(fb);
			drm_framebuffer_remove(fb);
		}
	}
	return ret;
}

static const struct drm_fb_helper_funcs kirin_fb_helper_funcs = {
	.fb_probe = kirin_fbdev_create,
};

/* initialize fbdev helper */
struct drm_fb_helper *kirin_drm_fbdev_init(struct drm_device *dev)
{
	struct kirin_drm_private *priv = dev->dev_private;
	struct kirin_fbdev *fbdev = NULL;
	struct drm_fb_helper *helper;
	int ret;

	fbdev = kzalloc(sizeof(*fbdev), GFP_KERNEL);
	if (!fbdev)
		goto fail;

	priv->fb_helper = helper = &fbdev->fb_helper;

	drm_fb_helper_prepare(dev, helper, &kirin_fb_helper_funcs);

	DRM_INFO("num_crtc=%d, num_connector=%d.\n",
		dev->mode_config.num_crtc, dev->mode_config.num_connector);

	ret = drm_fb_helper_init(dev, helper,
			dev->mode_config.num_crtc, dev->mode_config.num_connector);
	if (ret) {
		dev_err(dev->dev, "could not init fbdev: ret=%d\n", ret);
		goto fail;
	}

	ret = drm_fb_helper_single_add_all_connectors(helper);
	if (ret)
		goto fini;

	/* disable all the possible outputs/crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(dev);

	ret = drm_fb_helper_initial_config(helper, 32);
	if (ret)
		goto fini;

	priv->fbdev = helper;

	return helper;

fini:
	drm_fb_helper_fini(helper);
fail:
	kfree(fbdev);
	return NULL;
}

void kirin_drm_fbdev_fini(struct drm_device *dev)
{
	struct kirin_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *helper = priv->fbdev;
	struct kirin_fbdev *fbdev;

	drm_fb_helper_unregister_fbi(helper);
	drm_fb_helper_release_fbi(helper);

	drm_fb_helper_fini(helper);

	fbdev = to_kirin_fbdev(priv->fbdev);

	/* this will free the backing object */
	if (fbdev->fb) {
		drm_framebuffer_unregister_private(fbdev->fb);
		drm_framebuffer_remove(fbdev->fb);
	}

	kfree(fbdev);

	priv->fbdev = NULL;
}
