/*
 * Device driver for regulators in Hi6421V530 IC
 *
 * Copyright (c) <2014-2017> HiSilicon Technologies Co., Ltd.
 *              http://www.hisilicon.com
 * Copyright (c) <2013-2014> Linaro Ltd.
 *              http://www.linaro.org
 *
 * Author: Wang Xiaoyin <hw.wangxiaoyin@hisilicon.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/hi6421-pmic.h>
#include <linux/io.h>

/*
 * struct hi6421c530_regulator_pdata - Hi6421V530 regulator data
 * of platform device.
 * @lock: mutex to serialize regulator enable
 */
struct hi6421v530_regulator_pdata {
	struct mutex lock;
};

/*
 * struct hi6421v530_regulator_info - hi6421v530 regulator information
 * @desc: regulator description
 * @mode_mask: ECO mode bitmask of LDOs; for BUCKs, this masks sleep
 * @eco_microamp: eco mode load upper limit (in uA), valid for LDOs only
 */
struct hi6421v530_regulator_info {
	struct regulator_desc	desc;
	u8		mode_mask;
	u32		eco_microamp;
};

/* HI6421v530 regulators */
enum hi6421v530_regulator_id {
	HI6421V530_LDO3,
	HI6421V530_LDO9,
	HI6421V530_LDO11,
	HI6421V530_LDO15,
	HI6421V530_LDO16,
	HI6421V530_NUM_REGULATORS,
};

#define HI6421V530_REGULATOR_OF_MATCH(_name, id)	\
{							\
	.name = #_name,					\
	.driver_data = (void *) HI6421V530_##id,	\
}

static struct of_regulator_match hi6421v530_regulator_match[] = {
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo3, LDO3),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo9, LDO9),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo11, LDO11),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo15, LDO15),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo16, LDO16),
};

static const unsigned int ldo_3_voltages[] = {
	1800000, 1825000, 1850000, 1875000,
	1900000, 1925000, 1950000, 1975000,
	2000000, 2025000, 2050000, 2075000,
	2100000, 2125000, 2150000, 2200000,
};

static const unsigned int ldo_9_11_voltages[] = {
	1750000, 1800000, 1825000, 2800000,
	2850000, 2950000, 3000000, 3300000,
};

static const unsigned int ldo_15_16_voltages[] = {
	1750000, 1800000, 2400000, 2600000,
	2700000, 2850000, 2950000, 3000000,
};

static const struct regulator_ops hi6421v530_ldo_ops;

#define HI6421V530_LDO_ENABLE_TIME (350)

/*
 * _id - LDO id name string
 * v_table - voltage table
 * vreg - voltage select register
 * vmask - voltage select mask
 * ereg - enable register
 * emask - enable mask
 * odelay - off/on delay time in uS
 * ecomask - eco mode mask
 * ecoamp - eco mode load uppler limit in uA
 */
#define HI6421V530_LDO(_id, v_table, vreg, vmask, ereg, emask,		\
		   odelay, ecomask, ecoamp)				\
	[HI6421V530_##_id] = {						\
		.desc = {						\
			.name		= #_id,				\
			.ops		= &hi6421v530_ldo_ops,		\
			.type		= REGULATOR_VOLTAGE,		\
			.id		= HI6421V530_##_id,		\
			.owner		= THIS_MODULE,			\
			.n_voltages	= ARRAY_SIZE(v_table),		\
			.volt_table	= v_table,			\
			.vsel_reg	= HI6421_REG_TO_BUS_ADDR(vreg),	\
			.vsel_mask	= vmask,			\
			.enable_reg	= HI6421_REG_TO_BUS_ADDR(ereg),	\
			.enable_mask	= emask,			\
			.enable_time	= HI6421V530_LDO_ENABLE_TIME,	\
			.off_on_delay	= odelay,			\
		},							\
		.mode_mask		= ecomask,			\
		.eco_microamp		= ecoamp,			\
	}

/* HI6421V530 regulator information */

static struct hi6421v530_regulator_info
		hi6421v530_regulator_info[HI6421V530_NUM_REGULATORS] = {
	HI6421V530_LDO(LDO3, ldo_3_voltages, 0x061, 0xf, 0x060, 0x2,
		   20000, 0x6, 8000),
	HI6421V530_LDO(LDO9, ldo_9_11_voltages, 0x06b, 0x7, 0x06a, 0x2,
		   40000, 0x6, 8000),
	HI6421V530_LDO(LDO11, ldo_9_11_voltages, 0x06f, 0x7, 0x06e, 0x2,
		   40000, 0x6, 8000),
	HI6421V530_LDO(LDO15, ldo_15_16_voltages, 0x077, 0x7, 0x076, 0x2,
		   40000, 0x6, 8000),
	HI6421V530_LDO(LDO16, ldo_15_16_voltages, 0x079, 0x7, 0x078, 0x2,
		   40000, 0x6, 8000),
};

static int hi6421v530_regulator_enable(struct regulator_dev *rdev)
{
	struct hi6421v530_regulator_pdata *pdata;
	int ret = 0;

	pdata = dev_get_drvdata(rdev->dev.parent);
	mutex_lock(&pdata->lock);

	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
			rdev->desc->enable_mask,
			1 << (ffs(rdev->desc->enable_mask) - 1));

	mutex_unlock(&pdata->lock);
	return ret;
}

static int hi6421v530_regulator_disable(struct regulator_dev *rdev)
{
	struct hi6421v530_regulator_pdata *pdata;
	int ret = 0;

	pdata = dev_get_drvdata(rdev->dev.parent);
	mutex_lock(&pdata->lock);

	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
		rdev->desc->enable_mask, 0);

	mutex_unlock(&pdata->lock);
	return ret;
}

static int hi6421v530_regulator_is_enabled(struct regulator_dev *rdev)
{
	unsigned int reg_val = 0;
	int ret = 0;

	regmap_read(rdev->regmap, rdev->desc->enable_reg, &reg_val);

	ret = (reg_val & (rdev->desc->enable_mask)) ? 1 : 0;
	return ret;
}

static int hi6421v530_regulator_set_voltage(struct regulator_dev *rdev,
						unsigned int sel)
{
	struct hi6421v530_regulator_pdata *pdata;
	int ret = 0;

	pdata = dev_get_drvdata(rdev->dev.parent);
	mutex_lock(&pdata->lock);

	ret = regmap_update_bits(rdev->regmap, rdev->desc->vsel_reg,
				rdev->desc->vsel_mask,
				sel << (ffs(rdev->desc->vsel_mask) - 1));

	mutex_unlock(&pdata->lock);
	return ret;
}

static int hi6421v530_regulator_get_voltage(struct regulator_dev *rdev)
{
	unsigned int reg_val = 0;
	int voltage;

	regmap_read(rdev->regmap, rdev->desc->vsel_reg, &reg_val);

	voltage = reg_val >> (ffs(rdev->desc->vsel_mask) - 1);
	return voltage;
}

static unsigned int hi6421v530_regulator_ldo_get_mode(
					struct regulator_dev *rdev)
{
	struct hi6421v530_regulator_info *info;
	unsigned int reg_val;

	info = rdev_get_drvdata(rdev);
	regmap_read(rdev->regmap, rdev->desc->enable_reg, &reg_val);

	if (reg_val & (info->mode_mask))
		return REGULATOR_MODE_IDLE;

	return REGULATOR_MODE_NORMAL;
}

static int hi6421v530_regulator_ldo_set_mode(struct regulator_dev *rdev,
						unsigned int mode)
{
	struct hi6421v530_regulator_info *info;
	struct hi6421v530_regulator_pdata *pdata;
	unsigned int new_mode;

	info = rdev_get_drvdata(rdev);
	pdata = dev_get_drvdata(rdev->dev.parent);
	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		new_mode = 0;
		break;
	case REGULATOR_MODE_IDLE:
		new_mode = info->mode_mask;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
			   info->mode_mask, new_mode);
	mutex_unlock(&pdata->lock);

	return 0;
}


static const struct regulator_ops hi6421v530_ldo_ops = {
	.is_enabled = hi6421v530_regulator_is_enabled,
	.enable = hi6421v530_regulator_enable,
	.disable = hi6421v530_regulator_disable,
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_ascend,
	.get_voltage_sel = hi6421v530_regulator_get_voltage,
	.set_voltage_sel = hi6421v530_regulator_set_voltage,
	.get_mode = hi6421v530_regulator_ldo_get_mode,
	.set_mode = hi6421v530_regulator_ldo_set_mode,
};

static int hi6421v530_regulator_register(struct platform_device *pdev,
				     struct regmap *rmap,
				     struct regulator_init_data *init_data,
				     int id, struct device_node *np)
{
	struct hi6421v530_regulator_info *info = NULL;
	struct regulator_config config = { };
	struct regulator_dev *rdev;

	/* assign per-regulator data */
	info = &hi6421v530_regulator_info[id];

	config.dev = &pdev->dev;
	config.init_data = init_data;
	config.driver_data = info;
	config.regmap = rmap;
	config.of_node = np;

	/* register regulator with framework */
	rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
			info->desc.name);
		return PTR_ERR(rdev);
	}

	rdev->constraints->valid_modes_mask = info->mode_mask;
	rdev->constraints->valid_ops_mask |=
			REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE;

	return 0;
}

static int hi6421v530_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct hi6421_pmic *pmic;
	struct hi6421v530_regulator_pdata *pdata;
	int i, ret = 0;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	mutex_init(&pdata->lock);

	platform_set_drvdata(pdev, pdata);

	np = of_get_child_by_name(dev->parent->of_node, "regulators");
	if (!np)
		return -ENODEV;

	ret = of_regulator_match(dev, np,
				 hi6421v530_regulator_match,
				 ARRAY_SIZE(hi6421v530_regulator_match));
	of_node_put(np);
	if (ret < 0) {
		dev_err(dev, "Error parsing regulator init data: %d\n", ret);
		return ret;
	}

	pmic = dev_get_drvdata(dev->parent);

	for (i = 0; i < ARRAY_SIZE(hi6421v530_regulator_match); i++) {
		ret = hi6421v530_regulator_register(pdev, pmic->regmap,
			hi6421v530_regulator_match[i].init_data, i,
			hi6421v530_regulator_match[i].of_node);

		if (ret)
			return ret;
	}

	return 0;
}

static struct platform_driver hi6421v530_regulator_driver = {
	.driver = {
		.name	= "hi6421v530-regulator",
	},
	.probe	= hi6421v530_regulator_probe,
};
module_platform_driver(hi6421v530_regulator_driver);

MODULE_AUTHOR("Wang Xiaoyin <hw.wangxiaoyin@hisilicon.com>");
MODULE_DESCRIPTION("Hi6421v530 regulator driver");
MODULE_LICENSE("GPL v2");
