/*
 * KernelSU Next – SUSFS Integration Glue
 *
 * Bridges SUSFS's expected externs to KernelSU Next 3.2.0-legacy's internal
 * APIs.  This file lives inside the KernelSU module so it receives all the
 * Kbuild ccflags (KSU_HAS_PATH_UMOUNT, Samsung compat flags, etc.).
 */

#include <linux/version.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/printk.h>
#include <linux/types.h>

#ifndef KSU_HAS_PATH_UMOUNT
#  include <linux/uaccess.h>
#  include <linux/syscalls.h>
#endif

#include "klog.h"
#include "selinux/selinux.h"
#include "policy/allowlist.h"
#include "policy/app_profile.h"

/* Declared in fs/susfs.c */
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
extern bool susfs_is_log_enabled;
#endif
extern void susfs_try_umount(uid_t target_uid);

/* ── Variables required by fs/susfs.c ───────────────────────────────────── */

/*
 * susfs_zygote_sid – the real cached_zygote_sid inside selinux.c is static, so
 * we cannot expose it.  We publish a separate symbol initialised to 0.  All
 * actual zygote-domain checks go through susfs_is_current_zygote_domain()
 * which calls is_zygote(current_cred()) – the fast SID path.
 */
u32  susfs_zygote_sid __read_mostly = 0;
bool susfs_is_umount_for_zygote_system_process_enabled __read_mostly = false;

/* ── Domain helpers ─────────────────────────────────────────────────────── */

bool susfs_is_current_ksu_domain(void)
{
	return is_ksu_domain();
}

bool susfs_is_current_zygote_domain(void)
{
	return is_zygote(current_cred());
}

/* ── Root helpers ───────────────────────────────────────────────────────── */

bool susfs_is_allow_su(void)
{
	return ksu_is_allow_uid_for_current(current_uid().val);
}

void ksu_escape_to_root(void)
{
	escape_with_root_profile();
}

/* ── path_umount back-port declaration ──────────────────────────────────── */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0) || defined(KSU_HAS_PATH_UMOUNT)
extern int path_umount(struct path *path, int flags);
#endif

/* ── Umount bridge ──────────────────────────────────────────────────────── */

/*
 * ksu_try_umount – called by susfs_try_umount_all() and declared as extern in
 * fs/susfs.c.  Uses path_umount (back-ported by Kbuild on 4.14 via
 * KSU_HAS_PATH_UMOUNT) when available; falls back to the set_fs / ksys_umount
 * trick on older kernels that don't have it.
 */
void ksu_try_umount(const char *mnt, bool check_mnt, int flags, uid_t uid)
{
	struct path path;
	int err;

	err = kern_path(mnt, 0, &path);
	if (err)
		return;

	if (check_mnt && path.dentry != path.mnt->mnt_root) {
		path_put(&path);
		return;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0) || defined(KSU_HAS_PATH_UMOUNT)
	/* path_umount consumes the path reference */
	err = path_umount(&path, flags);
	if (err)
		pr_warn("susfs: umount '%s' failed: %d\n", mnt, err);
#else
	/* Release the reference before calling the user-API wrapper */
	path_put(&path);
	{
		char __user *usermnt = (char __user *)mnt;
		mm_segment_t old_fs = get_fs();
		set_fs(KERNEL_DS);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
		err = ksys_umount(usermnt, flags);
#else
		err = sys_umount(usermnt, flags);
#endif
		set_fs(old_fs);
		if (err)
			pr_warn("susfs: umount '%s' failed: %d\n", mnt, err);
	}
#endif
}

/*
 * susfs_try_umount_all – runs SUSFS's own umount list then forcibly unmounts
 * the standard KSU overlay paths.  Called from fs/namespace.c hooks installed
 * by the susfs kernel patch.
 */
void susfs_try_umount_all(uid_t uid)
{
	/* SUSFS's internal per-uid try-umount list */
	susfs_try_umount(uid);

	/* Standard KSU overlay mount points */
	ksu_try_umount("/system",          true,  0,           uid);
	ksu_try_umount("/system_ext",      true,  0,           uid);
	ksu_try_umount("/vendor",          true,  0,           uid);
	ksu_try_umount("/product",         true,  0,           uid);
	ksu_try_umount("/odm",             true,  0,           uid);
	ksu_try_umount("/data/adb/modules",false, MNT_DETACH,  uid);
	ksu_try_umount("/debug_ramdisk",   true,  MNT_DETACH,  uid);
}

/* ── Init / exit ────────────────────────────────────────────────────────── */

void ksu_susfs_glue_init(void)
{
	pr_info("ksu_susfs_glue: initialized (SUSFS 1.5.5)\n");
}

void ksu_susfs_glue_exit(void)
{
	pr_info("ksu_susfs_glue: exited\n");
}
