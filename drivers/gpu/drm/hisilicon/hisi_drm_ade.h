/*
 * Copyright (c) 2014-2015 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __HISI_DRM_ADE_H__
#define __HISI_DRM_ADE_H__

int ade_enable_vblank(struct drm_device *dev, unsigned int pipe);
void ade_disable_vblank(struct drm_device *dev, unsigned int pipe);

#endif
