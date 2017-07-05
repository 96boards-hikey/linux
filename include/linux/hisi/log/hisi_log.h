#ifndef _LINUX_HISILOG_H
#define _LINUX_HISILOG_H

#include <linux/printk.h>
#include <linux/types.h>

enum {
	HISILOG_ERR         = 1U << 0,
	HISILOG_WARNING     = 1U << 1,
	HISILOG_INFO        = 1U << 2,
	HISILOG_DEBUG       = 1U << 3,
	HISILOG_DEBUG1      = 1U << 4,
	HISILOG_DEBUG2      = 1U << 5,
	HISILOG_DEBUG3      = 1U << 6,
	HISILOG_DEBUG4      = 1U << 7,
};

#define HISILOG_TAG_DEFOUTL_LEVEL (HISILOG_ERR \
		| HISILOG_WARNING \
		| HISILOG_INFO)

struct hisi_log_tag {
	const char *name;
	u32 level;
};

#define HISILOG_REGIST()	\
	HISILOG_REGIST_TAG_LEVEL(HISILOG_TAG, HISILOG_TAG_DEFOUTL_LEVEL)

#define HISILOG_REGIST_LEVEL(level)	\
	HISILOG_REGIST_TAG_LEVEL(HISILOG_TAG, level)

#define HISILOG_REGIST_TAG_LEVEL(name, level)	\
	_HISILOG_REGIST_TAG_LEVEL(name, level)

#define _HISILOG_REGIST_TAG_LEVEL(name, level)	\
	static struct hisi_log_tag TAG_STRUCT_NAME(name)	\
__used								\
__attribute__ ((unused, __section__("__hisilog_tag"))) \
= { #name, level}

#define hisilog_err(x...) \
	_hisilog_err(HISILOG_TAG, ##x)

#define _hisilog_err(TAG, x...) \
	__hisilog_err(TAG, ##x)

#define __hisilog_err(TAG, fmt, ...) \
	do { \
		if (TAG_STRUCT_NAME(TAG).level & HISILOG_ERR) \
		pr_err(hw_fmt_tag(TAG, E) fmt, ##__VA_ARGS__);    \
	} while (0)

#define hisilog_warn(x...) \
	_hisilog_warn(HISILOG_TAG, ##x)

#define _hisilog_warn(TAG, x...) \
	__hisilog_warn(TAG, ##x)

#define __hisilog_warn(TAG, fmt, ...) \
	do { \
		if (TAG_STRUCT_NAME(TAG).level & HISILOG_WARNING) \
		pr_err(hw_fmt_tag(TAG, W) fmt, ##__VA_ARGS__);    \
	} while (0)

#define hisilog_info(x...) \
	_hisilog_info(HISILOG_TAG, ##x)

#define _hisilog_info(TAG, x...) \
	__hisilog_info(TAG, ##x)

#define __hisilog_info(TAG, fmt, ...) \
	do { \
		if (TAG_STRUCT_NAME(TAG).level & HISILOG_INFO) \
		pr_info(hw_fmt_tag(TAG, I) fmt, ##__VA_ARGS__);    \
	} while (0)

#define hisilog_debug(x...) \
	_hisilog_debug(HISILOG_TAG, ##x)

#define _hisilog_debug(TAG, x...) \
	__hisilog_debug(TAG, ##x)

#define __hisilog_debug(TAG, fmt, ...) \
	do { \
		if (TAG_STRUCT_NAME(TAG).level & HISILOG_DEBUG) \
		pr_err(hw_fmt_tag(TAG, D) fmt, ##__VA_ARGS__);    \
	} while (0)

#define hisilog_debug1(x...) \
	_hisilog_debug1(HISILOG_TAG, ##x)

#define _hisilog_debug1(TAG, x...) \
	__hisilog_debug1(TAG, ##x)

#define __hisilog_debug1(TAG, fmt, ...) \
	do { \
		if (TAG_STRUCT_NAME(TAG).level & HISILOG_DEBUG1) \
		pr_err(hw_fmt_tag(TAG, D1) fmt, ##__VA_ARGS__);    \
	} while (0)

#define hisilog_debug2(x...) \
	_hisilog_debug2(HISILOG_TAG, ##x)

#define _hisilog_debug2(TAG, x...) \
	__hisilog_debug2(TAG, ##x)

#define __hisilog_debug2(TAG, fmt, ...) \
	do { \
		if (TAG_STRUCT_NAME(TAG).level & HISILOG_DEBUG2) \
		pr_err(hw_fmt_tag(TAG, D2) fmt, ##__VA_ARGS__);    \
	} while (0)

#define hisilog_debug3(x...) \
	_hisilog_debug3(HISILOG_TAG, ##x)

#define _hisilog_debug3(TAG, x...) \
	__hisilog_debug3(TAG, ##x)

#define __hisilog_debug3(TAG, fmt, ...) \
	do { \
		if (TAG_STRUCT_NAME(TAG).level & HISILOG_DEBUG3) \
		pr_err(hw_fmt_tag(TAG, D3) fmt, ##__VA_ARGS__);    \
	} while (0)

#define hisilog_debug4(x...) \
	_hisilog_debug4(HISILOG_TAG, ##x)

#define _hisilog_debug4(TAG, x...) \
	__hisilog_debug4(TAG, ##x)

#define __hisilog_debug4(TAG, fmt, ...) \
	do { \
		if (TAG_STRUCT_NAME(TAG).level & HISILOG_DEBUG4) \
		pr_err(hw_fmt_tag(TAG, D4) fmt, ##__VA_ARGS__);    \
	} while (0)

#define TAG_STRUCT_NAME(name) \
	_hwtag_##name

#define hw_fmt_tag(TAG, LEVEL) "[" #LEVEL "/" #TAG "] "

#endif
