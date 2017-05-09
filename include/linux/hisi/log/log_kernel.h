#ifndef __LINUX_HISILOG_KERNEL_H
#define __LINUX_HISILOG_KERNEL_H

#include <linux/hisi/log/janklogconstants.h>
#define HISI_LOG_PRIO_VERBOSE  (2)
#define HISI_LOG_PRIO_DEBUG (3)
#define HISI_LOG_PRIO_INFO (4)
#define HISI_LOG_PRIO_WARN (5)
#define HISI_LOG_PRIO_ERROR (6)

typedef  enum hisilog_id{
	HISI_LOG_ID_MIN = 0,
	HISI_LOG_ID_EXCEPTION = HISI_LOG_ID_MIN,
	HISI_LOG_ID_JANK = 1,
	HISI_LOG_ID_BDAT = 2,
	HISI_LOG_ID_MAX
} hisi_bufid_t;

#if defined (CONFIG_HISILOG_KERNEL)
int hisilog_to_write(int prio, int bufid, const char* tag, const char* fmt, ...);
int hisilog_to_jank(int tag, int prio, const char* fmt, ...);
//for the forward compatibility ,HISI_LOG_PRIO_DEBUG level is just for HW service.
//and the interface name stay the same "pr_HW"
//use LOG_HW_W LOG_HW_V  LOG_HW_I   LOG_HW_E  for other purpose
#ifndef pr_jank
#define pr_jank(tag, fmt, ...) hisilog_to_jank(tag, HISI_LOG_PRIO_DEBUG, fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_JANK_D
#define LOG_JANK_D(tag, fmt, ...) hisilog_to_jank(tag, HISI_LOG_PRIO_DEBUG, fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_JANK_W
#define LOG_JANK_W(tag, fmt, ...) hisilog_to_jank(tag, HISI_LOG_PRIO_WARN, fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_JANK_V
#define LOG_JANK_V(tag, fmt, ...) hisilog_to_jank(tag, HISI_LOG_PRIO_VERBOSE, fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_JANK_I
#define LOG_JANK_I(tag, fmt, ...) hisilog_to_jank(tag, HISI_LOG_PRIO_INFO, fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_JANK_E
#define LOG_JANK_E(tag, fmt, ...) hisilog_to_jank(tag, HISI_LOG_PRIO_ERROR, fmt, ##__VA_ARGS__)
#endif

#ifndef HWBDAT_pr
#define HWBDAT_pr(tag, fmt, ...) hisilog_to_write(HISI_LOG_PRIO_DEBUG, HISI_LOG_ID_BDAT, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef HWBDAT_LOGV
#define HWBDAT_LOGV(tag, fmt, ...) hisilog_to_write(HISI_LOG_PRIO_VERBOSE, HISI_LOG_ID_BDAT, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef HWBDAT_LOGD
#define HWBDAT_LOGD(tag, fmt,...) hisilog_to_write(HISI_LOG_PRIO_DEBUG, HISI_LOG_ID_BDAT, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef HWBDAT_LOGI
#define HWBDAT_LOGI(tag, fmt, ...) hisilog_to_write(HISI_LOG_PRIO_INFO,  HISI_LOG_ID_BDAT, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef HWBDAT_LOGW
#define HWBDAT_LOGW(tag, fmt, ...) hisilog_to_write(HISI_LOG_PRIO_WARN,  HISI_LOG_ID_BDAT, tag, fmt, ##__VA_ARGS__)
#endif

#ifndef HWBDAT_LOGE
#define HWBDAT_LOGE(tag, fmt, ...) hisilog_to_write(HISI_LOG_PRIO_ERROR,  HISI_LOG_ID_BDAT, tag, fmt, ##__VA_ARGS__)
#endif

#else
#define pr_jank(tag, fmt, ...)
#define LOG_JANK_D(tag, fmt, ...)
#define LOG_JANK_W(tag, fmt, ...)
#define LOG_JANK_V(tag, fmt, ...)
#define LOG_JANK_I(tag, fmt, ...)
#define LOG_JANK_E(tag, fmt, ...)

#define HWBDAT_pr(tag, fmt, ...)
#define HWBDAT_LOGV(tag, fmt, ...)
#define HWBDAT_LOGD(tag, fmt, ...)
#define HWBDAT_LOGI(tag, fmt, ...)
#define HWBDAT_LOGW(tag, fmt, ...)
#define HWBDAT_LOGE(tag, fmt, ...)
#endif
#endif
