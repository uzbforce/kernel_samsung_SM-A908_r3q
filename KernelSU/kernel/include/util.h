#ifndef __KSU_H_UTIL
#define __KSU_H_UTIL

#include "linux/fdtable.h" // IWYU pragma: keep
#include <linux/version.h>
#include <linux/syscalls.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/cred.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
#define ksu_close_fd close_fd
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
#define ksu_close_fd ksys_close
#else
/* Linux < 4.17 has neither close_fd() nor ksys_close(). */
#define ksu_close_fd(fd) __close_fd(current->files, (fd))
#endif

static inline struct file *ksu_filp_open_nonotify(const char *path, int flags)
{
    struct path p;
    struct file *f;
    int ret;
    ret = kern_path(path, (flags & O_NOFOLLOW) ? 0 : LOOKUP_FOLLOW, &p);
    if (ret) {
        return ERR_PTR(ret);
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 14, 0)
    f = dentry_open_nonotify(&p, flags, current_cred());
#else
    f = dentry_open(&p, flags | __FMODE_NONOTIFY, current_cred());
#endif

    path_put(&p);
    return f;
}

#endif
