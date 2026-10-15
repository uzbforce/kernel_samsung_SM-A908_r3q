#ifndef __KSU_H_KERNEL_COMPAT
#define __KSU_H_KERNEL_COMPAT

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/task_work.h>
#include <linux/uaccess.h>

/*
 * Compatibility shims for kernels predating the modern helper APIs that
 * KernelSU-Next v3.4.0 assumes. Ported from the v3.2.0-legacy tree, which is
 * known to build and run on Linux 4.14.
 */

// Linux >= 5.7: task_work_add(struct, struct, enum task_work_notify_mode)
// Linux <  5.7: task_work_add(struct, struct, bool)
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
#ifndef TWA_RESUME
#define TWA_RESUME true
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
#define ksu_access_ok(addr, size) access_ok(addr, size)
#else
#define ksu_access_ok(addr, size) access_ok(VERIFY_READ, addr, size)
#endif

// The fallthrough statement attribute macro was only introduced in 5.x
// (include/linux/compiler_attributes.h).
#ifndef fallthrough
#define fallthrough __attribute__((__fallthrough__))
#endif

// ktime_get_boottime_ts64() only exists from 4.18 on.
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
#include <linux/ktime.h>
#include <linux/timekeeping.h>
static inline void ktime_get_boottime_ts64(struct timespec64 *ts)
{
    *ts = ktime_to_timespec64(ktime_get_boottime());
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
long copy_from_user_nofault(void *dst, const void __user *src, size_t size);
long copy_to_user_nofault(void __user *dst, const void *src, size_t size);
long strncpy_from_user_nofault(char *dst, const void __user *unsafe_addr, long count);
#endif

// path_mount() only exists from 5.9 on.
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0) && !defined(KSU_HAS_PATH_MOUNT)
int path_mount(const char *dev_name, struct path *path, const char *type_page, unsigned long flags, void *data_page);
#endif

#endif // #ifndef __KSU_H_KERNEL_COMPAT
