/*
 * Device driver for regulators in Hi6421V530 IC
 *
 * Copyright (c) <2011-2014> HiSilicon Technologies Co., Ltd.
 *              http://www.hisilicon.com
 * Copyright (c) <2013-2014> Linaro Ltd.
 *              http://www.linaro.org
 *
 * Author: Zhong Kaihua <zhongkaihua@huawei.com>
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
 * struct hi6421c530_regulator_pdata - Hi6421V530 regulator data of platform device
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
	/*HI6421V530_LDO0_2,*/
	HI6421V530_LDO1,
	/*HI6421V530_LDO2,*/
	HI6421V530_LDO3,
	HI6421V530_LDO4,
	/*HI6421V530_LDO5,
	HI6421V530_LDO7,
	HI6421V530_LDO8,*/
	HI6421V530_LDO9,
	HI6421V530_LDO10,
	/*HI6421V530_LDO11,
	HI6421V530_LDO12,*/
	HI6421V530_LDO13,
	/*HI6421V530_LDO14,*/
	HI6421V530_LDO15,
	HI6421V530_LDO16,
	HI6421V530_LDO17,
	HI6421V530_LDO19,
	HI6421V530_LDO20,
	/*HI6421V530_LDO21,
	HI6421V530_LDO22,
	HI6421V530_LDO23,*/
	HI6421V530_LDO24,
	HI6421V530_LDO25,
	/*HI6421V530_LDO26,
	HI6421V530_LDO27,
	HI6421V530_LDO28,
	HI6421V530_LDO29,
	HI6421V530_LDO30,*/
	HI6421V530_LDO31,
	HI6421V530_LDO32,
	/*HI6421V530_BUCK0,
	HI6421V530_BUCK1,
	HI6421V530_BUCK2,
	HI6421V530_BUCK3,
	HI6421V530_BUCK4,*/
	HI6421V530_NUM_REGULATORS,
};

#define HI6421V530_REGULATOR_OF_MATCH(_name, id)				\
{									\
	.name = #_name,							\
	.driver_data = (void *) HI6421V530_##id,				\
}

static struct of_regulator_match hi6421v530_regulator_match[] = {
	/*HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo0_2, LDO0_2),*/
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo1, LDO1),
	/*HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo2, LDO2),*/
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo3, LDO3),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo4, LDO4),
	/*HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo5, LDO5),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo7, LDO7),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo8, LDO8),*/
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo9, LDO9),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo10, LDO10),
	/*HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo11, LDO11),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo12, LDO12),*/
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo13, LDO13),
	/*HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo14, LDO14),*/
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo15, LDO15),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo16, LDO16),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo17, LDO17),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo19, LDO19),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo20, LDO20),
	/*HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo21, LDO21),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo22, LDO22),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo23, LDO23),*/
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo24, LDO24),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo25, LDO25),
	/*HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo26, LDO26),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo27, LDO27),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo28, LDO28),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo29, LDO29),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo30, LDO30),*/
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo31, LDO31),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_ldo32, LDO32),
	/*HI6421V530_REGULATOR_OF_MATCH(hi6421v530_buck0, BUCK0),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_buck1, BUCK1),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_buck2, BUCK2),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_buck3, BUCK3),
	HI6421V530_REGULATOR_OF_MATCH(hi6421v530_buck4, BUCK4),*/
};


static const unsigned int ldo_0_voltages[] = {
	600000, 650000, 700000, 750000,
	775000, 800000, 825000, 850000,
};

static const unsigned int ldo_1_voltages[] = {
	1000000, 1050000, 1100000, 1150000,
	1200000, 1250000, 1260000, 1270000,
	1280000, 1290000, 1300000, 1310000,
	1320000, 1330000, 1340000, 1500000,
};

static const unsigned int ldo_2_4_voltages[] = {
	1700000, 1750000, 1775000, 1800000,
	1825000, 1850000, 1875000, 1900000,
};

static const unsigned int ldo_3_voltages[] = {
	1800000, 1825000, 1850000, 1875000,
	1900000, 1925000, 1950000, 1975000,
	2000000, 2025000, 2050000, 2075000,
	2100000, 2125000, 2150000, 2200000,
};


static const unsigned int ldo_5_7_8_voltages[] = {
	1700000, 1750000, 1775000, 1800000,
	1825000, 1850000, 1900000, 1950000,
};

static const unsigned int ldo_9_11_12_13_14_voltages[] = {
	1750000, 1800000, 1825000, 2800000,
	2850000, 2950000, 3000000, 3300000,
};

static const unsigned int ldo_10_voltages[] = {
	3100000, 3150000, 3200000, 3300000,
	3300000, 3300000, 3300000, 3300000,
};

static const unsigned int ldo_15_16_voltages[] = {
	1750000, 1800000, 2400000, 2600000,
	2700000, 2850000, 2950000, 3000000,
};

static const unsigned int ldo_17_19_voltages[] = {
	1800000, 2600000, 2700000, 2750000,
	2800000, 2850000, 2900000, 3000000,
};

static const unsigned int ldo_20_32_voltages[] = {
	800000, 850000, 900000, 950000,
	1000000, 1050000, 1100000, 1200000,
	1250000, 1260000, 1270000, 1280000,
	1290000, 1300000, 1310000, 1320000,
};

static const unsigned int ldo_21_voltages[] = {
	1700000, 1750000, 1775000, 1800000,
	1825000, 1850000, 1875000, 1900000,
};

static const unsigned int ldo_22_voltages[] = {
	1000000, 1050000, 1100000, 1150000,
	1200000, 1250000, 1260000, 1270000,
	1280000, 1290000, 1300000, 1310000,
	1320000, 1330000, 1340000, 1500000,
};

static const unsigned int ldo_23_24_voltages[] = {
	2600000, 2700000, 2800000, 2900000,
	3000000, 3100000, 3200000, 3300000,
};

static const unsigned int ldo_25_voltages[] = {
	1500000, 1800000, 2400000, 2500000,
	2600000, 2700000, 2850000, 3000000,
};

static const unsigned int ldo_26_voltages[] = {
	1500000, 1550000, 1600000, 1650000,
	1700000, 1750000, 1800000, 1850000,
};

static const unsigned int ldo_27_voltages[] = {
	2200000, 2250000, 2300000, 2350000,
	2375000, 2400000, 2425000, 2450000,
	2475000, 2500000, 2550000, 2600000,
	2650000, 2700000, 2800000, 2850000,
};

static const unsigned int ldo_28_voltages[] = {
	1700000, 1750000, 1775000, 1800000,
	1825000, 1850000, 1875000, 1900000,
};

static const unsigned int ldo_29_voltages[] = {
	1000000, 1050000, 1100000, 1200000,
	1250000, 1300000, 1500000, 1550000,
};

static const unsigned int ldo_30_voltages[] = {
	700000, 725000, 750000, 775000,
	775000, 800000, 810000, 825000,
	830000, 850000, 875000, 900000,
	925000, 950000, 976000, 1000000,
};

static const unsigned int ldo_31_voltages[] = {
	2500000, 2600000, 2700000, 2800000,
	2900000, 3000000, 3100000, 3200000,
};

static const unsigned int buck_0_4_voltages[] = {
	700000, 725000, 750000, 775000,
	790000, 800000, 810000, 825000,
	830000, 850000, 875000, 900000,
	925000, 950000, 975000, 1000000,
};

static const unsigned int buck_1_voltages[] = {
	925000, 950000, 975000, 1000000,
	1025000, 1050000, 1075000, 1100000,
	1125000, 1150000, 1175000, 1200000,
	1225000, 1250000, 1275000, 1300000,
};

static const unsigned int buck_2_voltages[] = {
	1350000, 1375000, 1400000, 1425000,
	1450000, 1475000, 1500000, 1525000,
	1550000, 1575000, 1600000, 1625000,
	1650000, 1675000, 1700000, 1725000,
};

static const unsigned int buck_3_voltages[] = {
	1800000, 1850000, 1900000, 1950000,
	2000000, 2050000, 2100000, 2150000,
	2200000, 2250000, 2300000, 2350000,
	2400000, 2450000, 2500000, 2550000,
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
			.id		= HI6421V530_##_id,			\
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

/*#define HI6421V530_LDO(_id, v_table, vreg, vmask, ereg, emask,		\
		   odelay, ecomask, ecoamp)	*/
/* _id - LDO id name string
 * v_table - voltage table
 * vreg - voltage select register
 * vmask - voltage select mask
 * ereg - enable register
 * emask - enable mask
 * odelay - off/on delay time in uS
 * ecomask - eco mode mask
 * ecoamp - eco mode load uppler limit in uA
 */

static struct hi6421v530_regulator_info
		hi6421v530_regulator_info[HI6421V530_NUM_REGULATORS] = {
	/*HI6421V530_LDO(LDO0_2, ldo_0_voltages, 0x05b, 0x7, 0x05a, 0x2,
		   20000, 0x1, 8000),*/
	HI6421V530_LDO(LDO1, ldo_1_voltages, 0x05d, 0xf, 0x05c, 0x2,
		   20000, 0x1, 8000),
	/*HI6421V530_LDO(LDO2, ldo_2_4_voltages, 0x05f, 0x7, 0x05e, 0x2,
		   20000, 0x1, 8000),*/
	HI6421V530_LDO(LDO3, ldo_3_voltages, 0x061, 0xf, 0x060, 0x2,
		   20000, 0x1, 8000),
	HI6421V530_LDO(LDO4, ldo_2_4_voltages, 0x063, 0x7, 0x062, 0x2,
		   20000, 0x1, 8000),
	/*HI6421V530_LDO(LDO5, ldo_5_7_8_voltages, 0x065, 0x7, 0x064, 0x2,
		   20000, 0x1, 8000),
	HI6421V530_LDO(LDO7, ldo_5_7_8_voltages, 0x067, 0x7, 0x066, 0x2,
		   20000, 0x1, 5000),
	HI6421V530_LDO(LDO8, ldo_5_7_8_voltages, 0x069, 0x7, 0x068, 0x2,
		   20000, 0x1, 8000),*/
	HI6421V530_LDO(LDO9, ldo_9_11_12_13_14_voltages, 0x06b, 0x7, 0x06a, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO10, ldo_10_voltages, 0x06d, 0x7, 0x06c, 0x2,
		   40000, 0x1, 8000),
	/*HI6421V530_LDO(LDO11, ldo_9_11_12_13_14_voltages, 0x06f, 0x7, 0x06e, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO12, ldo_9_11_12_13_14_voltages, 0x071, 0x7, 0x070, 0x2,
		   40000, 0x1, 8000),*/
	HI6421V530_LDO(LDO13, ldo_9_11_12_13_14_voltages, 0x073, 0x7, 0x072, 0x2,
		   40000, 0x1, 8000),
	/*HI6421V530_LDO(LDO14, ldo_9_11_12_13_14_voltages, 0x075, 0x7, 0x074, 0x2,
		   40000, 0x1, 8000),*/
	HI6421V530_LDO(LDO15, ldo_15_16_voltages, 0x077, 0x7, 0x076, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO16, ldo_15_16_voltages, 0x079, 0x7, 0x078, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO17, ldo_17_19_voltages, 0x07b, 0x7, 0x07a, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO19, ldo_17_19_voltages, 0x07d, 0x7, 0x07c, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO20, ldo_20_32_voltages, 0x07f, 0xf, 0x07e, 0x2,
		   40000, 0x1, 8000),
	/*HI6421V530_LDO(LDO21, ldo_21_voltages, 0x081, 0x7, 0x080, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO22, ldo_22_voltages, 0x083, 0xf, 0x082, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO23, ldo_23_24_voltages, 0x085, 0x7, 0x084, 0x2,
		   40000, 0x1, 8000),*/
	HI6421V530_LDO(LDO24, ldo_23_24_voltages, 0x087, 0x7, 0x086, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO25, ldo_25_voltages, 0x089, 0x7, 0x088, 0x2,
		   40000, 0x1, 8000),
	/*HI6421V530_LDO(LDO26, ldo_26_voltages, 0x08b, 0x7, 0x08a, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO27, ldo_27_voltages, 0x08d, 0xf, 0x08c, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO28, ldo_28_voltages, 0x08f, 0x7, 0x08e, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO29, ldo_29_voltages, 0x091, 0x7, 0x090, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO30, ldo_30_voltages, 0x093, 0xf, 0x092, 0x2,
		   40000, 0x1, 8000),*/
	HI6421V530_LDO(LDO31, ldo_31_voltages, 0x095, 0x7, 0x094, 0x2,
		   40000, 0x1, 8000),
	HI6421V530_LDO(LDO32, ldo_20_32_voltages, 0x097, 0xf, 0x096, 0x2,
		   40000, 0x1, 8000),
	/*HI6421V530_LDO(BUCK0, buck_0_4_voltages, 0x099, 0xf, 0x098, 0x2, 4000, 0x1, 20000),
	HI6421V530_LDO(BUCK1, buck_1_voltages, 0x09b, 0xf, 0x09a, 0x2, 4000, 0x1, 20000),
	HI6421V530_LDO(BUCK2, buck_2_voltages, 0x09d, 0xf, 0x09c, 0x2, 4000, 0x1, 20000),
	HI6421V530_LDO(BUCK3, buck_3_voltages, 0x09f, 0xf, 0x09e, 0x2, 4000, 0x1, 20000),
	HI6421V530_LDO(BUCK4, buck_0_4_voltages, 0x0a1, 0xf, 0x0a0, 0x2, 4000, 0x1, 20000),*/
};

static int hi6421v530_regulator_enable(struct regulator_dev *rdev)
{
	struct hi6421v530_regulator_pdata *pdata;
	int ret = 0;

	pdata = dev_get_drvdata(rdev->dev.parent);
	mutex_lock(&pdata->lock);

	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
		rdev->desc->enable_mask, 1 << (ffs(rdev->desc->enable_mask) - 1));

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

static int hi6421v530_regulator_set_voltage(struct regulator_dev *rdev, unsigned sel)
{
	struct hi6421v530_regulator_pdata *pdata;
	int ret = 0;

	pdata = dev_get_drvdata(rdev->dev.parent);
	mutex_lock(&pdata->lock);

	ret = regmap_update_bits(rdev->regmap, rdev->desc->vsel_reg,
				  rdev->desc->vsel_mask, sel);

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

static unsigned int hi6421v530_regulator_ldo_get_mode(struct regulator_dev *rdev)
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
	dev_info(dev, "of_regulator_match return %d!\n", ret);

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
	dev_err(dev, "hi6421v530_regulator driver Init\n");
	return 0;
}

static struct platform_driver hi6421v530_regulator_driver = {
	.driver = {
		.name	= "hi6421v530-regulator",
	},
	.probe	= hi6421v530_regulator_probe,
};
module_platform_driver(hi6421v530_regulator_driver);

MODULE_AUTHOR("Zhong Kaihua <zhongkaihua@huawei.com>");
MODULE_DESCRIPTION("Hi6421v530 regulator driver");
MODULE_LICENSE("GPL v2");
