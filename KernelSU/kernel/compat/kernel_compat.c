// SPDX-License-Identifier: GPL-2.0-only
/*
 * Compatibility implementations for pre-5.8 kernels.
 *
 * The 4.14 kernel has neither the *_user_nofault() accessors (added in 5.8)
 * nor the copy_from_user_nofault()/copy_to_user_nofault() names, so provide
 * them on top of the inatomic primitives. Ported from the v3.2.0-legacy tree.
 */

#include <linux/version.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/uaccess.h>
#include "klog.h" // IWYU pragma: keep
#include "kernel_compat.h"

/*
 * path_mount() only exists from 5.9 on; it was written in terms of do_mount()
 * before that.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0) && !defined(KSU_HAS_PATH_MOUNT)
extern long do_mount(const char *dev_name, const char __user *dir_name, const char *type_page, unsigned long flags,
                     void *data_page);

int path_mount(const char *dev_name, struct path *path, const char *type_page, unsigned long flags, void *data_page)
{
    mm_segment_t old_fs;
    long ret = 0;
    char buf[384];

    char *realpath = d_path(path, buf, sizeof(buf));
    if (IS_ERR(realpath)) {
        pr_err("ksu_mount: d_path failed, err: %lu\n", PTR_ERR(realpath));
        return PTR_ERR(realpath);
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    ret = do_mount(dev_name, (const char __user *)realpath, type_page, flags, data_page);
    set_fs(old_fs);
    return ret;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
long copy_from_user_nofault(void *dst, const void __user *src, size_t size)
{
    // https://elixir.bootlin.com/linux/v5.8/source/mm/maccess.c#L205
    long ret = -EFAULT;
    mm_segment_t old_fs = get_fs();

    set_fs(USER_DS);
    if (ksu_access_ok(src, size)) {
        pagefault_disable();
        ret = __copy_from_user_inatomic(dst, src, size);
        pagefault_enable();
    }
    set_fs(old_fs);

    if (ret)
        return -EFAULT;
    return 0;
}

long copy_to_user_nofault(void __user *dst, const void *src, size_t size)
{
    long ret = -EFAULT;
    mm_segment_t old_fs = get_fs();

    set_fs(USER_DS);
    if (access_ok(VERIFY_WRITE, dst, size)) {
        pagefault_disable();
        ret = __copy_to_user_inatomic(dst, src, size);
        pagefault_enable();
    }
    set_fs(old_fs);

    if (ret)
        return -EFAULT;
    return 0;
}

long strncpy_from_user_nofault(char *dst, const void __user *unsafe_addr, long count)
{
    // https://elixir.bootlin.com/linux/v5.8/source/mm/maccess.c#L205
    mm_segment_t old_fs = get_fs();
    long ret;

    if (unlikely(count <= 0))
        return 0;

    set_fs(USER_DS);
    pagefault_disable();
    ret = strncpy_from_user(dst, unsafe_addr, count);
    pagefault_enable();
    set_fs(old_fs);

    if (ret >= count) {
        ret = count;
        dst[ret - 1] = '\0';
    } else if (ret > 0) {
        ret++;
    }

    return ret;
}
#endif // #if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
