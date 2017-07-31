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

#include "kirin_drm_drv.h"

#include "drm_crtc.h"
#include "drm_crtc_helper.h"

struct kirin_framebuffer {
	struct drm_framebuffer base;
};
#define to_kirin_framebuffer(x) container_of(x, struct kirin_framebuffer, base)


static int kirin_framebuffer_create_handle(struct drm_framebuffer *fb,
		struct drm_file *file_priv,
		unsigned int *handle)
{
	//struct kirin_framebuffer *kirin_fb = to_kirin_framebuffer(fb);
	return 0;
}

static void kirin_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct kirin_framebuffer *kirin_fb = to_kirin_framebuffer(fb);

	DRM_DEBUG("destroy: FB ID: %d (%p)", fb->base.id, fb);

	drm_framebuffer_cleanup(fb);

	kfree(kirin_fb);
}

static int kirin_framebuffer_dirty(struct drm_framebuffer *fb,
		struct drm_file *file_priv, unsigned flags, unsigned color,
		struct drm_clip_rect *clips, unsigned num_clips)
{
	return 0;
}

static const struct drm_framebuffer_funcs kirin_framebuffer_funcs = {
	.create_handle = kirin_framebuffer_create_handle,
	.destroy = kirin_framebuffer_destroy,
	.dirty = kirin_framebuffer_dirty,
};

struct drm_framebuffer *kirin_framebuffer_init(struct drm_device *dev,
		struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct kirin_framebuffer *kirin_fb = NULL;
	struct drm_framebuffer *fb;
	int ret;

	kirin_fb = kzalloc(sizeof(*kirin_fb), GFP_KERNEL);
	if (!kirin_fb) {
		ret = -ENOMEM;
		goto fail;
	}

	fb = &kirin_fb->base;

	drm_helper_mode_fill_fb_struct(fb, mode_cmd);

	ret = drm_framebuffer_init(dev, fb, &kirin_framebuffer_funcs);
	if (ret) {
		dev_err(dev->dev, "framebuffer init failed: %d\n", ret);
		goto fail;
	}

	DRM_DEBUG("create: FB ID: %d (%p)", fb->base.id, fb);

	return fb;

fail:
	kfree(kirin_fb);

	return ERR_PTR(ret);
}
