/*
 *  linux/drivers/devfreq/hisi_ddr_devfreq.c
 *
 *  Copyright (c) 2017 Hisilicon Technologies CO., Ltd.
 *
 *  Author: Tao Wang <kevin.wangtao@hisilicon.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/version.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/devfreq.h>
#include <linux/clk.h>
#include <linux/pm_qos.h>
#include "governor.h"

struct ddr_devfreq_pdata {
	int pm_qos_constraint;
	unsigned int bytes_per_cycle;
	unsigned int polling_ms;
	char *governor;
	void *governor_data;
};

struct ddr_devfreq_device {
	struct devfreq *devfreq;
	struct clk *get_current;
	struct clk *req_dn_thres;
	struct clk *req_up_thres;
	unsigned long dn_thres_freq;
	unsigned long up_thres_freq;
	unsigned long max_freq;
	struct notifier_block nb;
	const struct ddr_devfreq_pdata *pdata;
};

static int ddr_devfreq_target(struct device *dev,
			unsigned long *freq, u32 flags)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct ddr_devfreq_device *ddev = platform_get_drvdata(pdev);
	struct dev_pm_opp *opp = NULL;
	struct dev_pm_opp *max_opp = NULL;
	unsigned long max_freq = ddev->devfreq->max_freq;

	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, freq, flags);
	max_opp = devfreq_recommended_opp(dev, &max_freq,
			DEVFREQ_FLAG_LEAST_UPPER_BOUND);
	rcu_read_unlock();
	if (IS_ERR(opp) || IS_ERR(max_opp)) {
		dev_err(dev, "Failed to get Operating Point\n");
		return IS_ERR(opp) ? PTR_ERR(opp) : PTR_ERR(max_opp);
	}

	if (ddev->devfreq->max_freq &&
	    max_freq != ddev->up_thres_freq &&
	    ddev->req_up_thres) {
		dev_dbg(dev, "set up threshold:%lu\n", max_freq);
		(void)clk_set_rate(ddev->req_up_thres, max_freq);
		ddev->up_thres_freq = max_freq;
	}

	if (ddev->dn_thres_freq != *freq) {
		/* undate ddr freqency down threshold */
		dev_dbg(dev, "set down threshold:%lu\n", *freq);
		(void)clk_set_rate(ddev->req_dn_thres, *freq);
		ddev->dn_thres_freq = *freq;
	}

	return 0;
}

static int ddr_devfreq_get_dev_status(struct device *dev,
			struct devfreq_dev_status *stat)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct ddr_devfreq_device *ddev = platform_get_drvdata(pdev);

#ifdef CONFIG_PM
	if (ddev->pdata->pm_qos_constraint) {
		stat->busy_time =
			pm_qos_request(ddev->pdata->pm_qos_constraint);
		stat->total_time =
			(ddev->max_freq * ddev->pdata->bytes_per_cycle) >> 20;
		stat->current_frequency = ddev->max_freq;
		dev_dbg(&pdev->dev, "ddr bandwdith request: %lu / %lu\n",
				stat->busy_time, stat->total_time);
	}
#endif

	return 0;
}

static int ddr_devfreq_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct ddr_devfreq_device *ddev = platform_get_drvdata(pdev);

	if (ddev->get_current)
		*freq = clk_get_rate(ddev->get_current);
	else
		*freq = ddev->dn_thres_freq;

	return 0;
}

static struct devfreq_dev_profile ddr_devfreq_profile = {
	.polling_ms		= 0,
	.target			= ddr_devfreq_target,
	.get_dev_status		= ddr_devfreq_get_dev_status,
	.get_cur_freq		= ddr_devfreq_get_cur_freq,
};

static struct devfreq_simple_ondemand_data ddr_ondemand = {
	.upthreshold		= 60,
	.downdifferential	= 1,
};

static int devfreq_pm_qos_notifier(struct notifier_block *nb,
			unsigned long val, void *v)
{
	struct ddr_devfreq_device *ddev = container_of(nb,
				struct ddr_devfreq_device, nb);

	mutex_lock(&ddev->devfreq->lock);
	update_devfreq(ddev->devfreq);
	mutex_unlock(&ddev->devfreq->lock);

	return NOTIFY_OK;
}

static int hisi_devfreq_set_freq_table(struct device *dev,
			struct devfreq_dev_profile *profile)
{
	struct dev_pm_opp *opp;
	unsigned long freq;
	int i, count, ret = 0;

	/* Initialize the freq_table from OPP table */
	count = dev_pm_opp_get_opp_count(dev);
	if (count <= 0)
		return -ENOMEM;

	profile->max_state = count;
	profile->freq_table = devm_kcalloc(dev,
			profile->max_state,
			sizeof(*profile->freq_table),
			GFP_KERNEL);
	if (!profile->freq_table) {
		profile->max_state = 0;
		return -ENOMEM;
	}

	rcu_read_lock();
	for (i = 0, freq = 0; i < profile->max_state; i++, freq++) {
		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (IS_ERR(opp)) {
			profile->max_state = 0;
			ret = -ENOMEM;
			break;
		}
		profile->freq_table[i] = freq;
	}
	rcu_read_unlock();

	return ret;
}

static struct ddr_devfreq_pdata hi3660_pdata = {
	.pm_qos_constraint = PM_QOS_MEMORY_BANDWIDTH,
	.bytes_per_cycle = 16,
	.polling_ms = 0,
	.governor = "simple_ondemand",
	.governor_data = &ddr_ondemand,
};

static struct ddr_devfreq_pdata kirin970_pdata = {
	.pm_qos_constraint = PM_QOS_MEMORY_BANDWIDTH,
	.bytes_per_cycle = 16,
	.polling_ms = 0,
	.governor = "simple_ondemand",
	.governor_data = &ddr_ondemand,
};

static const struct of_device_id ddr_devfreq_of_match[] = {
	{
		.compatible = "hisilicon,hi3660-ddrfreq",
		.data = &hi3660_pdata,
	},
	{
		.compatible = "hisilicon,kirin970-ddrfreq",
		.data = &kirin970_pdata,
	},
	{},
};
MODULE_DEVICE_TABLE(of, ddr_devfreq_of_match);

static int ddr_devfreq_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct ddr_devfreq_device *ddev = NULL;
	struct device_node *np = pdev->dev.of_node;
	unsigned int max_state;
	int ret = 0;

	ddev = devm_kzalloc(dev, sizeof(struct ddr_devfreq_device),
				GFP_KERNEL);
	if (!ddev)
		return -ENOMEM;

	match = of_match_device(ddr_devfreq_of_match, dev);
	ddev->pdata = match->data;
	platform_set_drvdata(pdev, ddev);

	ddev->req_dn_thres = of_clk_get(np, 0);
	if (IS_ERR(ddev->req_dn_thres)) {
		dev_err(dev, "Failed to get req-dn-thres-clk\n");
		ret = -ENODEV;
		goto no_clk;
	}

	ddev->get_current = of_clk_get(np, 1);
	if (IS_ERR(ddev->get_current)) {
		dev_err(dev, "Failed to get get-current-clk\n");
		ddev->get_current = NULL;
	}

	ddev->req_up_thres = of_clk_get(np, 2);
	if (IS_ERR(ddev->req_up_thres)) {
		dev_err(dev, "Failed to get req-up-thres-clk\n");
		ddev->req_up_thres = NULL;
	}

	if (dev_pm_opp_of_add_table(dev) ||
	    hisi_devfreq_set_freq_table(dev, &ddr_devfreq_profile)) {
		dev_err(dev, "Failed to init freq table\n");
		ret = -ENODEV;
		goto no_devfreq;
	}

	ddr_devfreq_profile.polling_ms = ddev->pdata->polling_ms;
	max_state = ddr_devfreq_profile.max_state;
	ddev->max_freq = ddr_devfreq_profile.freq_table[max_state-1];
	ddev->devfreq = devm_devfreq_add_device(dev,
			&ddr_devfreq_profile,
			ddev->pdata->governor,
			ddev->pdata->governor_data);
	if (IS_ERR_OR_NULL(ddev->devfreq)) {
		dev_err(dev, "Failed to init ddr devfreq\n");
		ret = -ENODEV;
		goto no_devfreq;
	}

#ifdef CONFIG_PM
	if (ddev->pdata->pm_qos_constraint) {
		ddev->nb.notifier_call = devfreq_pm_qos_notifier;
		ret = pm_qos_add_notifier(ddev->pdata->pm_qos_constraint,
					&ddev->nb);
		if (ret)
			goto no_notifier;
	}
#endif

	dev_info(dev, "init success\n");
	return ret;

no_notifier:
	devfreq_remove_device(ddev->devfreq);
no_devfreq:
	if (ddev->req_up_thres)
		clk_put(ddev->req_up_thres);

	if (ddev->get_current)
		clk_put(ddev->get_current);

	clk_put(ddev->req_dn_thres);
no_clk:
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int ddr_devfreq_remove(struct platform_device *pdev)
{
	struct ddr_devfreq_device *ddev;

	ddev = platform_get_drvdata(pdev);

#ifdef CONFIG_PM
	if (ddev->pdata->pm_qos_constraint)
		pm_qos_remove_notifier(ddev->pdata->pm_qos_constraint,
					&ddev->nb);
#endif

	if (ddev->req_up_thres)
		clk_put(ddev->req_up_thres);

	if (ddev->get_current)
		clk_put(ddev->get_current);

	clk_put(ddev->req_dn_thres);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver ddr_devfreq_driver = {
	.probe = ddr_devfreq_probe,
	.remove = ddr_devfreq_remove,
	.driver = {
		.name = "hisi_ddr_devfreq",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ddr_devfreq_of_match),
	},
};

module_platform_driver(ddr_devfreq_driver);

MODULE_AUTHOR("Tao Wang <kevin.wangtao@hisilicon.com>");
MODULE_DESCRIPTION("hisi ddr devfreq driver");
MODULE_LICENSE("GPL v2");
