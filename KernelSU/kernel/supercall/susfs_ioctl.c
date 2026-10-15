/*
 * KernelSU Next – SUSFS IOCTL Handlers
 *
 * Translates CMD_SUSFS_* ioctl commands (routed here from the prctl bridge in
 * kernel/sys.c) into direct calls to the SUSFS kernel functions.
 *
 * The ksu_susfs userspace binary calls:
 *   prctl(0xDEADBEEF, CMD_SUSFS_*, arg3, 0, &result)
 * The bridge in kernel/sys.c forwards those as:
 *   ksu_supercall_handle_ioctl(CMD_SUSFS_*, arg3)
 * which falls through to the secondary loop in dispatch.c that calls into
 * ksu_susfs_ioctl_handlers[] defined here.
 */

#include <linux/cred.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>

#include "klog.h"
#include "supercall/internal.h"
#include "supercall/supercall.h"
#include "selinux/selinux.h"

#include <linux/susfs.h>
#include <linux/susfs_def.h>
#include "feature/susfs_glue.h"

/* Only uid==0 may issue configuration commands */
static bool susfs_only_root(void)
{
	return current_uid().val == 0;
}

/* ── Per-feature handlers ────────────────────────────────────────────────── */

#ifdef CONFIG_KSU_SUSFS_SUS_PATH
static int do_susfs_add_sus_path(void __user *argp)
{
	int err = susfs_add_sus_path((struct st_susfs_sus_path __user *)argp);
	pr_info("susfs: ADD_SUS_PATH -> %d\n", err);
	return err;
}
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
static int do_susfs_add_sus_mount(void __user *argp)
{
	int err = susfs_add_sus_mount((struct st_susfs_sus_mount __user *)argp);
	pr_info("susfs: ADD_SUS_MOUNT -> %d\n", err);
	return err;
}
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
static int do_susfs_add_sus_kstat(void __user *argp)
{
	int err = susfs_add_sus_kstat((struct st_susfs_sus_kstat __user *)argp);
	pr_info("susfs: ADD_SUS_KSTAT -> %d\n", err);
	return err;
}

static int do_susfs_update_sus_kstat(void __user *argp)
{
	int err = susfs_update_sus_kstat((struct st_susfs_sus_kstat __user *)argp);
	pr_info("susfs: UPDATE_SUS_KSTAT -> %d\n", err);
	return err;
}

static int do_susfs_add_sus_kstat_statically(void __user *argp)
{
	return do_susfs_add_sus_kstat(argp);
}
#endif

#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
static int do_susfs_add_try_umount(void __user *argp)
{
	int err = susfs_add_try_umount((struct st_susfs_try_umount __user *)argp);
	pr_info("susfs: ADD_TRY_UMOUNT -> %d\n", err);
	return err;
}

static int do_susfs_run_umount_for_current_mnt_ns(void __user *argp)
{
	susfs_try_umount(current_uid().val);
	return 0;
}
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
static int do_susfs_set_uname(void __user *argp)
{
	int err = susfs_set_uname((struct st_susfs_uname __user *)argp);
	pr_info("susfs: SET_UNAME -> %d\n", err);
	return err;
}
#endif

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
static int do_susfs_enable_log(void __user *argp)
{
	/* arg3 is passed as an integer value, NOT a pointer */
	int enabled = (int)(unsigned long)argp;
	if (enabled != 0 && enabled != 1)
		return -EINVAL;
	susfs_set_log(enabled);
	return 0;
}
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
static int do_susfs_set_cmdline_or_bootconfig(void __user *argp)
{
	int err = susfs_set_cmdline_or_bootconfig((char __user *)argp);
	pr_info("susfs: SET_CMDLINE_OR_BOOTCONFIG -> %d\n", err);
	return err;
}
#endif

#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
static int do_susfs_add_open_redirect(void __user *argp)
{
	int err = susfs_add_open_redirect(
			(struct st_susfs_open_redirect __user *)argp);
	pr_info("susfs: ADD_OPEN_REDIRECT -> %d\n", err);
	return err;
}
#endif

/* ── Query handlers (no perm_check – available to all processes) ─────────── */

static int do_susfs_show_version(void __user *argp)
{
	const char *ver = SUSFS_VERSION;
	if (copy_to_user(argp, ver, strlen(ver) + 1))
		return -EFAULT;
	return 0;
}

static int do_susfs_show_variant(void __user *argp)
{
	const char *var = SUSFS_VARIANT;
	if (copy_to_user(argp, var, strlen(var) + 1))
		return -EFAULT;
	return 0;
}

static int do_susfs_show_enabled_features(void __user *argp)
{
	u64 f = 0;
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	f |= (1ULL << 0);
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	f |= (1ULL << 1);
#endif
#ifdef CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT
	f |= (1ULL << 2);
#endif
#ifdef CONFIG_KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT
	f |= (1ULL << 3);
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	f |= (1ULL << 4);
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_OVERLAYFS
	f |= (1ULL << 5);
#endif
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
	f |= (1ULL << 6);
#endif
#ifdef CONFIG_KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT
	f |= (1ULL << 7);
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	f |= (1ULL << 8);
#endif
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
	f |= (1ULL << 9);
#endif
#ifdef CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
	f |= (1ULL << 10);
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	f |= (1ULL << 11);
#endif
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	f |= (1ULL << 12);
#endif
#ifdef CONFIG_KSU_SUSFS_HAS_MAGIC_MOUNT
	f |= (1ULL << 14);
#endif
	if (copy_to_user(argp, &f, sizeof(f)))
		return -EFAULT;
	return 0;
}

/* ── Handler table ───────────────────────────────────────────────────────── */

const struct ksu_ioctl_cmd_map ksu_susfs_ioctl_handlers[] = {
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	{ CMD_SUSFS_ADD_SUS_PATH,            "SUSFS_ADD_SUS_PATH",            do_susfs_add_sus_path,            susfs_only_root },
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	{ CMD_SUSFS_ADD_SUS_MOUNT,           "SUSFS_ADD_SUS_MOUNT",           do_susfs_add_sus_mount,           susfs_only_root },
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	{ CMD_SUSFS_ADD_SUS_KSTAT,           "SUSFS_ADD_SUS_KSTAT",           do_susfs_add_sus_kstat,           susfs_only_root },
	{ CMD_SUSFS_UPDATE_SUS_KSTAT,        "SUSFS_UPDATE_SUS_KSTAT",        do_susfs_update_sus_kstat,        susfs_only_root },
	{ CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY,"SUSFS_ADD_SUS_KSTAT_STATICALLY",do_susfs_add_sus_kstat_statically,susfs_only_root },
#endif
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
	{ CMD_SUSFS_ADD_TRY_UMOUNT,          "SUSFS_ADD_TRY_UMOUNT",          do_susfs_add_try_umount,          susfs_only_root },
	{ CMD_SUSFS_RUN_UMOUNT_FOR_CURRENT_MNT_NS, "SUSFS_RUN_UMOUNT", do_susfs_run_umount_for_current_mnt_ns, susfs_only_root },
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	{ CMD_SUSFS_SET_UNAME,               "SUSFS_SET_UNAME",               do_susfs_set_uname,               susfs_only_root },
#endif
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
	{ CMD_SUSFS_ENABLE_LOG,              "SUSFS_ENABLE_LOG",              do_susfs_enable_log,              susfs_only_root },
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	{ CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG,"SUSFS_SET_CMDLINE_OR_BOOTCONFIG",do_susfs_set_cmdline_or_bootconfig,susfs_only_root },
#endif
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	{ CMD_SUSFS_ADD_OPEN_REDIRECT,       "SUSFS_ADD_OPEN_REDIRECT",       do_susfs_add_open_redirect,       susfs_only_root },
#endif
	/* Query commands – NULL perm_check means always allowed */
	{ CMD_SUSFS_SHOW_VERSION,            "SUSFS_SHOW_VERSION",            do_susfs_show_version,            NULL },
	{ CMD_SUSFS_SHOW_ENABLED_FEATURES,   "SUSFS_SHOW_ENABLED_FEATURES",   do_susfs_show_enabled_features,   NULL },
	{ CMD_SUSFS_SHOW_VARIANT,            "SUSFS_SHOW_VARIANT",            do_susfs_show_variant,            NULL },
	{ 0, NULL, NULL, NULL } /* sentinel */
};

