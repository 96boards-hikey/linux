/*
 *
 * (C) COPYRIGHT 2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





#include <linux/ioport.h>
#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_config.h>
#ifdef CONFIG_PM_DEVFREQ
#include <linux/devfreq.h>
#endif /* CONFIG_PM_DEVFREQ */

#include <trace/events/power.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_REPORT_VSYNC
#include <linux/export.h>
#endif
#include <linux/delay.h>
#include <backend/gpu/mali_kbase_pm_internal.h>
#include "mali_kbase_config_platform.h"
#include "mali_kbase_config_hifeatures.h"

typedef enum {
        MALI_ERROR_NONE = 0,
        MALI_ERROR_OUT_OF_GPU_MEMORY,
        MALI_ERROR_OUT_OF_MEMORY,
        MALI_ERROR_FUNCTION_FAILED,
}mali_error;

#define HARD_RESET_AT_POWER_OFF 0

static inline void kbase_os_reg_write(struct kbase_device *kbdev, u16 offset, u32 value)
{
	writel(value, kbdev->reg + offset);
}

static inline u32 kbase_os_reg_read(struct kbase_device *kbdev, u16 offset)
{
	return readl(kbdev->reg + offset);
}

#ifndef CONFIG_OF
static struct kbase_io_resources io_resources = {
	.job_irq_number = 68,
	.mmu_irq_number = 69,
	.gpu_irq_number = 70,
	.io_memory_region = {
	.start = 0xFC010000,
	.end = 0xFC010000 + (4096 * 4) - 1
	}
};
#endif /* CONFIG_OF */

#define RUNTIME_PM_DELAY_1MS      1
#define RUNTIME_PM_DELAY_30MS    30

#ifdef CONFIG_REPORT_VSYNC
static struct kbase_device *kbase_dev = NULL;
#endif

struct hisi_platform_data {
	int vsync_hit;
	void __iomem *pctrlreg;
	void __iomem *pmctrlreg;
	unsigned long features_mask[2];
	u32 gpu_vid;
};

static int kbase_set_hi_features_mask(struct kbase_device *kbdev,
				      struct hisi_platform_data *pd)
{
	const enum kbase_hi_feature *hi_features;
	u32 gpu_vid;
	u32 product_id;

	gpu_vid = pd->gpu_vid;
	product_id = gpu_vid & GPU_ID_VERSION_PRODUCT_ID;
	product_id >>= GPU_ID_VERSION_PRODUCT_ID_SHIFT;

	if (GPU_ID_IS_NEW_FORMAT(product_id)) {
		switch (gpu_vid) {
		case GPU_ID2_MAKE(6, 0, 10, 0, 0, 0, 2):
			hi_features = kbase_hi_feature_tMIx_r0p0;
			break;
		case GPU_ID2_MAKE(6, 2, 2, 1, 0, 0, 0):
			hi_features = kbase_hi_feature_tHEx_r0p0;
			break;
		case GPU_ID2_MAKE(6, 2, 2, 1, 0, 0, 1):
			hi_features = kbase_hi_feature_tHEx_r0p0;
			break;
		default:
			dev_err(kbdev->dev,
				"[hi-feature]Unknown GPU ID %x", gpu_vid);
			return -EINVAL;
		}
	} else {
		switch (gpu_vid) {
		case GPU_ID_MAKE(GPU_ID_PI_TFRX, 0, 2, 0):
			hi_features = kbase_hi_feature_t880_r0p2;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_T83X, 1, 0, 0):
			hi_features = kbase_hi_feature_t830_r2p0;
			break;
		case GPU_ID_MAKE(GPU_ID_PI_TFRX, 2, 0, 0):
			hi_features = kbase_hi_feature_t880_r2p0;
			break;
		default:
			dev_err(kbdev->dev,
				"[hi-feature]Unknown GPU ID %x", gpu_vid);
			return -EINVAL;
		}
	}

	dev_info(kbdev->dev, "[hi-feature]GPU identified as 0x%04x r%dp%d status %d",
		(gpu_vid & GPU_ID_VERSION_PRODUCT_ID) >> GPU_ID_VERSION_PRODUCT_ID_SHIFT,
		(gpu_vid & GPU_ID_VERSION_MAJOR) >> GPU_ID_VERSION_MAJOR_SHIFT,
		(gpu_vid & GPU_ID_VERSION_MINOR) >> GPU_ID_VERSION_MINOR_SHIFT,
		(gpu_vid & GPU_ID_VERSION_STATUS) >> GPU_ID_VERSION_STATUS_SHIFT);

	for (; *hi_features != KBASE_HI_FEATURE_END; hi_features++)
		set_bit(*hi_features, &pd->features_mask[0]);

	return 0;
}

static inline void kbase_platform_on(struct kbase_device *kbdev)
{
	if (kbdev->regulator) {
		struct hisi_platform_data *pd;

		pd = (struct hisi_platform_data *)kbdev->platform_context;

		if (unlikely(regulator_enable(kbdev->regulator))) {
			dev_err(kbdev->dev, "Failed to enable regulator\n");
			BUG_ON(1);
		}

		if (pd->gpu_vid == 0) {
			pd->gpu_vid = kbase_os_reg_read(kbdev,
							GPU_CONTROL_REG(GPU_ID)
							);
			if (unlikely(kbase_set_hi_features_mask(kbdev, pd))) {
				dev_err(kbdev->dev,
					"Failed to set hi features\n");
			}
		}

		if (kbase_has_hi_feature(pd, KBASE_FEATURE_HI0004)) {
			kbase_os_reg_write(kbdev, GPU_CONTROL_REG(PWR_KEY),
					   KBASE_PWR_KEY_VALUE);
			kbase_os_reg_write(kbdev,
					   GPU_CONTROL_REG(PWR_OVERRIDE1),
					   KBASE_PWR_OVERRIDE_VALUE);
		}

		if (kbase_has_hi_feature(pd, KBASE_FEATURE_HI0003)) {
			int value = 0;
			value = readl(pd->pctrlreg + PERI_CTRL19) &
				GPU_X2P_GATOR_BYPASS;
			writel(value, pd->pctrlreg + PERI_CTRL19);
		}
	}
}

static inline void kbase_platform_off(struct kbase_device *kbdev)
{
	if (kbdev->regulator) {
		if (unlikely(regulator_disable(kbdev->regulator))) {
			dev_err(kbdev->dev, "MALI-MIDGARD: Failed to disable regulator\n");
		}
	}
}

#ifdef CONFIG_REPORT_VSYNC
void mali_kbase_pm_report_vsync(int buffer_updated)
{
	unsigned long flags;

	if (kbase_dev){
		struct hisi_platform_data *pd;

		pd = (struct hisi_platform_data *)kbase_dev->platform_context;

		spin_lock_irqsave(&kbase_dev->pm.backend.metrics.lock, flags);
		pd->vsync_hit = buffer_updated;
		spin_unlock_irqrestore(&kbase_dev->pm.backend.metrics.lock,
				       flags);
	}
}
EXPORT_SYMBOL(mali_kbase_pm_report_vsync);
#endif

#ifdef CONFIG_MALI_MIDGARD_DVFS
int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation, u32 util_gl_share, u32 util_cl_share[2])
{
	return 1;
}

int kbase_platform_dvfs_enable(struct kbase_device *kbdev, bool enable, int freq)
{
	unsigned long flags;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	if (enable != kbdev->pm.backend.metrics.timer_active) {
		if (enable) {
			spin_lock_irqsave(&kbdev->pm.backend.metrics.lock, flags);
			kbdev->pm.backend.metrics.timer_active = true;
			spin_unlock_irqrestore(&kbdev->pm.backend.metrics.lock, flags);
			hrtimer_start(&kbdev->pm.backend.metrics.timer,
					HR_TIMER_DELAY_MSEC(kbdev->pm.dvfs_period),
					HRTIMER_MODE_REL);
		} else {
			spin_lock_irqsave(&kbdev->pm.backend.metrics.lock, flags);
			kbdev->pm.backend.metrics.timer_active = false;
			spin_unlock_irqrestore(&kbdev->pm.backend.metrics.lock, flags);
			hrtimer_cancel(&kbdev->pm.backend.metrics.timer);
		}
	}

	return 1;
}
#endif

static int kbase_platform_init(struct kbase_device *kbdev)
{
	struct hisi_platform_data *pd;
	int err;

#ifdef CONFIG_REPORT_VSYNC
	kbase_dev = kbdev;
#endif

	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd) {
		err = -ENOMEM;
		goto no_mem;
	}

	pd->pmctrlreg = ioremap(SYS_REG_PMCTRL_BASE_ADDR, SYS_REG_PMCTRL_SIZE);
	if (!pd->pmctrlreg) {
		dev_err(kbdev->dev, "Can't remap sys pmctrl register window on platform hi3660\n");
		err = -EINVAL;
		goto out_pmctrl_ioremap;
	}

	pd->pctrlreg = ioremap(SYS_REG_PCTRL_BASE_ADDR, SYS_REG_PCTRL_SIZE);
	if (!pd->pctrlreg) {
		dev_err(kbdev->dev, "Can't remap sys pctrl register window on platform hi3660\n");
		err = -EINVAL;
		goto out_pctrl_ioremap;
	}

	kbdev->platform_context = pd;

	kbase_platform_on(kbdev);

	if (kbase_has_hi_feature(pd, KBASE_FEATURE_HI0006)) {
		unsigned int value = 0;
		/*GPU and PMCTRL shader core power on/off decrease freq
		 * handshake start*/
		/*read 0x264 and set it's [3:0] and [19:16]bit to 0,enable G3D
		 * HPM hardware status contrl*/
		value = readl(pd->pmctrlreg + G3DHPMBYPASS) & MASK_G3DHPMBYPASS;
		writel(value, pd->pmctrlreg + G3DHPMBYPASS);

		/*read 0x268 and set it's [0]bit to 0,enable G3D auto clkdiv*/
		value = readl(pd->pmctrlreg + G3DAUTOCLKDIVBYPASS) &
			MASK_G3DAUTOCLKDIVBYPASS;
		writel(value, pd->pmctrlreg + G3DAUTOCLKDIVBYPASS);
		/*GPU and PMCTRL shader core power on/off decrease freq
		 * handshake end*/
		/*GPU IDLE VDM decrease freq start*/
		/*read 0x46c and set it's [26]bit to 1,enable L2 reduce freq
		 * when it IDLE*/
		value = readl(pd->pmctrlreg + VS_CTRL_2) | (1<<26);
		writel(value, pd->pmctrlreg + VS_CTRL_2);
		/*GPU IDLE VDM decrease freq end*/
	}

	kbase_platform_off(kbdev);

	return 0;

out_pctrl_ioremap:
	iounmap(pd->pmctrlreg);
out_pmctrl_ioremap:
	kfree(pd);
no_mem:
	return err;
}

static void kbase_platform_term(struct kbase_device *kbdev)
{
	struct hisi_platform_data *pd;

	pd = (struct hisi_platform_data *)kbdev->platform_context;

	iounmap(pd->pmctrlreg);
	iounmap(pd->pctrlreg);

	kfree(pd);
}

struct kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = &kbase_platform_init,
	.platform_term_func = &kbase_platform_term,
};

static int pm_callback_power_on(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_MIDGARD_RT_PM
	int result;
	int ret_val;
	struct device *dev = kbdev->dev;

#if (HARD_RESET_AT_POWER_OFF != 1)
	if (!pm_runtime_status_suspended(dev))
		ret_val = 0;
	else
#endif
		ret_val = 1;

	if (unlikely(dev->power.disable_depth > 0)) {
		kbase_platform_on(kbdev);
	} else {
		result = pm_runtime_resume(dev);
		if (result < 0 && result == -EAGAIN)
			kbase_platform_on(kbdev);
		else if (result < 0)
			printk("[mali]  pm_runtime_resume failed (%d)\n", result);
	}

	return ret_val;
#else
	kbase_platform_on(kbdev);

	return 1;
#endif
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_MIDGARD_RT_PM
	struct device *dev = kbdev->dev;
	int ret = 0, retry = 0;
	struct hisi_platform_data *pd;

	pd = (struct hisi_platform_data *)kbdev->platform_context;

	if (kbase_has_hi_feature(pd, KBASE_FEATURE_HI0008)) {
		/* when GPU in idle state, auto decrease the clock rate.
		 */
		unsigned int tiler_lo = kbdev->tiler_available_bitmap & 0xFFFFFFFF;
		unsigned int tiler_hi = (kbdev->tiler_available_bitmap >> 32) & 0xFFFFFFFF;
		unsigned int l2_lo = kbdev->l2_available_bitmap & 0xFFFFFFFF;
		unsigned int l2_hi = (kbdev->l2_available_bitmap >> 32) & 0xFFFFFFFF;

		kbase_os_reg_write(kbdev, GPU_CONTROL_REG(TILER_PWROFF_LO), tiler_lo);
		kbase_os_reg_write(kbdev, GPU_CONTROL_REG(TILER_PWROFF_HI), tiler_hi);
		kbase_os_reg_write(kbdev, GPU_CONTROL_REG(L2_PWROFF_LO), l2_lo);
		kbase_os_reg_write(kbdev, GPU_CONTROL_REG(L2_PWROFF_HI), l2_hi);
	}

#if HARD_RESET_AT_POWER_OFF
	/* Cause a GPU hard reset to test whether we have actually idled the GPU
	 * and that we properly reconfigure the GPU on power up.
	 * Usually this would be dangerous, but if the GPU is working correctly it should
	 * be completely safe as the GPU should not be active at this point.
	 * However this is disabled normally because it will most likely interfere with
	 * bus logging etc.
	 */
	KBASE_TRACE_ADD(kbdev, CORE_GPU_HARD_RESET, NULL, NULL, 0u, 0);
	kbase_os_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_HARD_RESET);
#endif

	if (unlikely(dev->power.disable_depth > 0)) {
		kbase_platform_off(kbdev);
	} else {
		do {
			if (kbase_has_hi_feature(pd, KBASE_FEATURE_HI0007))
				ret = pm_schedule_suspend(dev, RUNTIME_PM_DELAY_1MS);
			else
				ret = pm_schedule_suspend(dev, RUNTIME_PM_DELAY_30MS);
			if (ret != -EAGAIN) {
				if (unlikely(ret < 0)) {
					pr_err("[mali]  pm_schedule_suspend failed (%d)\n\n", ret);
					WARN_ON(1);
				}

				/* correct status */
				break;
			}

			/* -EAGAIN, repeated attempts for 1s totally */
			msleep(50);
		} while (++retry < 20);
	}
#else
	kbase_platform_off(kbdev);
#endif
}

#ifdef CONFIG_MALI_MIDGARD_RT_PM
static int pm_callback_runtime_init(struct kbase_device *kbdev)
{
	pm_suspend_ignore_children(kbdev->dev, true);
	pm_runtime_enable(kbdev->dev);
	return 0;
}

static void pm_callback_runtime_term(struct kbase_device *kbdev)
{
	pm_runtime_disable(kbdev->dev);
}

static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
#if defined(CONFIG_MALI_MIDGARD_DVFS)
	kbase_platform_dvfs_enable(kbdev, false, 0);
#endif

	kbase_platform_off(kbdev);
}

static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	kbase_platform_on(kbdev);

#if defined(CONFIG_MALI_MIDGARD_DVFS)
	if (!kbase_platform_dvfs_enable(kbdev, true, 0))
		return -EPERM;
#endif

	return 0;
}
#endif

static inline void pm_callback_suspend(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_MIDGARD_RT_PM
	if (!pm_runtime_status_suspended(kbdev->dev))
		pm_callback_runtime_off(kbdev);
#else
	pm_callback_power_off(kbdev);
#endif
}

static inline void pm_callback_resume(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_MIDGARD_RT_PM
	if (!pm_runtime_status_suspended(kbdev->dev))
		pm_callback_runtime_on(kbdev);
	else
		pm_callback_power_on(kbdev);
#else
	pm_callback_power_on(kbdev);
#endif
}

#ifdef CONFIG_MALI_MIDGARD_RT_PM
static inline int pm_callback_runtime_idle(struct kbase_device *kbdev)
{
	return 1;
}
#endif

struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = pm_callback_suspend,
	.power_resume_callback = pm_callback_resume,
#ifdef CONFIG_MALI_MIDGARD_RT_PM
	.power_runtime_init_callback = pm_callback_runtime_init,
	.power_runtime_term_callback = pm_callback_runtime_term,
	.power_runtime_off_callback = pm_callback_runtime_off,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_idle_callback = pm_callback_runtime_idle
#else
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_off_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_idle_callback = NULL
#endif
};



static struct kbase_platform_config hi_platform_config = {
#ifndef CONFIG_OF
	.io_resources = &io_resources
#endif
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &hi_platform_config;
}

int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}
