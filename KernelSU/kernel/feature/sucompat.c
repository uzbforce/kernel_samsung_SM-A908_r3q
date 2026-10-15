#include "linux/file.h"
#include "linux/fcntl.h"
#include "linux/namei.h"
#include <linux/compiler_types.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <asm/current.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/sched/task_stack.h>
#include <linux/ptrace.h>

#include "arch.h"
#include "compat/kernel_compat.h"
#include "policy/allowlist.h"
#include "policy/feature.h"
#include "klog.h" // IWYU pragma: keep
#include "runtime/ksud.h"
#include "feature/sucompat.h"
#include "policy/app_profile.h"
#include "hook/syscall_hook.h"
#include "hook/syscall_event_bridge.h"
#include "selinux/selinux.h"
#include "supercall/supercall.h"
#include "sulog/event.h"
#include "ksu.h"
#include "util.h"

#define SU_PATH "/system/bin/su"
#define SH_PATH "/system/bin/sh"

bool ksu_su_compat_enabled __read_mostly = true;

static int su_compat_feature_get(u64 *value)
{
	*value = ksu_su_compat_enabled ? 1 : 0;
	return 0;
}

static int su_compat_feature_set(u64 value)
{
	bool enable = value != 0;
	ksu_su_compat_enabled = enable;
	pr_info("su_compat: set to %d\n", enable);
	return 0;
}

static const struct ksu_feature_handler su_compat_handler = {
	.feature_id = KSU_FEATURE_SU_COMPAT,
	.name = "su_compat",
	.get_handler = su_compat_feature_get,
	.set_handler = su_compat_feature_set,
};

static void __user *userspace_stack_buffer(const void *d, size_t len)
{
	// To avoid having to mmap a page in userspace, just write below the stack
	// pointer.
	char __user *p = (void __user *)current_user_stack_pointer() - len;

	return copy_to_user(p, d, len) ? NULL : p;
}static char __user *ksud_user_path(void)
{
    static const char ksud_path[] = KSUD_PATH;

    return userspace_stack_buffer(ksud_path, sizeof(ksud_path));
}

static char __user *sh_user_path(void)
{
    static const char sh_path[] = SH_PATH;

    return userspace_stack_buffer(sh_path, sizeof(sh_path));
}

static char __user *empty_user_path(void)
{
	return userspace_stack_buffer("", sizeof(""));
}

static const char su_path[] = SU_PATH;

static bool is_ksud_exists()
{
	struct path path;

	if (kern_path(KSUD_PATH, 0, &path) < 0) {
		return false;
	}
	path_put(&path);
	return true;
}

long ksu_handle_faccessat_sucompat(int orig_nr, struct pt_regs *regs)
{
	const char __user **filename_user, *orig_filename;
	long ret;
	const struct cred *old_cred;

	if (!ksu_is_allow_uid_for_current(current_uid().val)) {
		goto do_orig_facessat;
	}

	filename_user = (const char __user **)&PT_REGS_PARM2(regs);

	char path[sizeof(su_path) + 1];
	memset(path, 0, sizeof(path));
	// Linux < 5.8 has no strncpy_from_user_nofault(); this runs in plain
	// syscall context, so the faulting variant is safe here.
	strncpy_from_user(path, *filename_user, sizeof(path));

	if (unlikely(!memcmp(path, su_path, sizeof(su_path)))) {
		old_cred = override_creds(ksu_cred);
		if (is_ksud_exists()) {
			ksu_compat_sulog('a');
			pr_info("faccessat su->ksud!\n");
			orig_filename = *filename_user;
			*filename_user = ksud_user_path();
			ret = ksu_syscall_table[orig_nr](regs);
			revert_creds(old_cred);
			*filename_user = orig_filename;
			return ret;
		} else {
			revert_creds(old_cred);
		}
	}

do_orig_facessat:
	return ksu_syscall_table[orig_nr](regs);
}

long ksu_handle_stat_sucompat(int orig_nr, struct pt_regs *regs)
{
	const char __user **filename_user, *orig_filename;
	long ret;
	const struct cred *old_cred;

	if (!ksu_is_allow_uid_for_current(current_uid().val)) {
		goto do_orig_stat;
	}

	filename_user = (const char __user **)&PT_REGS_PARM2(regs);

	char path[sizeof(su_path) + 1];
	memset(path, 0, sizeof(path));
	// Linux < 5.8 has no strncpy_from_user_nofault(); this runs in plain
	// syscall context, so the faulting variant is safe here.
	strncpy_from_user(path, *filename_user, sizeof(path));

	if (unlikely(!memcmp(path, su_path, sizeof(su_path)))) {
		old_cred = override_creds(ksu_cred);
		if (is_ksud_exists()) {
			ksu_compat_sulog('s');
			pr_info("newfstatat su->ksud!\n");
			orig_filename = *filename_user;
			*filename_user = ksud_user_path();
			ret = ksu_syscall_table[orig_nr](regs);
			revert_creds(old_cred);
			*filename_user = orig_filename;
			return ret;
		} else {
			revert_creds(old_cred);
		}
	}

do_orig_stat:
	return ksu_syscall_table[orig_nr](regs);
}

static long ksu_handle_execve_sucompat_common(const char __user **filename_user,
					      const char __user *const __user *argv_user, unsigned long envp,
					      bool execveat, int orig_nr, struct pt_regs *regs)
{
	const char __user *fn;
	struct ksu_sulog_pending_event *pending_sucompat = NULL;
	char path[sizeof(su_path) + 1];
	long ret, orig_regs[5];
	unsigned long addr;
	int su_fd = -1;
	int tmp_fd;
	struct file *ksud_file;
	const struct cred *old_cred;

	if (execveat && ((int)PT_REGS_PARM1(regs) != AT_FDCWD || (int)PT_REGS_PARM5(regs) != 0))
		goto do_orig_execve;

	if (unlikely(!filename_user))
		goto do_orig_execve;

	if (!ksu_is_allow_uid_for_current(current_uid().val))
		goto do_orig_execve;

	addr = untagged_addr((unsigned long)*filename_user);
	fn = (const char __user *)addr;
	memset(path, 0, sizeof(path));
	ret = strncpy_from_user(path, fn, sizeof(path));

	if (ret < 0) {
		pr_warn("Access filename when execve failed: %ld", ret);
		goto do_orig_execve;
	}

	if (likely(memcmp(path, su_path, sizeof(su_path))))
		goto do_orig_execve;

	ksu_compat_sulog('x');
	pr_info("sys_execve su found\n");

	tmp_fd = get_unused_fd_flags(O_CLOEXEC);
	if (tmp_fd < 0) {
		pr_err("alloc tmp fd err: %d\n", tmp_fd);
		goto do_orig_execve;
	}

	old_cred = override_creds(ksu_cred);
	ksud_file = filp_open(KSUD_PATH, O_PATH, 0);
	revert_creds(old_cred);
	if (IS_ERR(ksud_file)) {
		pr_err("open ksud err: %ld\n", PTR_ERR(ksud_file));
		put_unused_fd(tmp_fd);
		goto do_orig_execve;
	}

	fd_install(tmp_fd, ksud_file);

	pending_sucompat = ksu_sulog_capture_sucompat(*filename_user, argv_user, GFP_KERNEL);
	// execve(file, argv, environ)
	// execveat(fd, file, argv, environ, flags)
	orig_regs[0] = regs->__PT_PARM1_REG;
	orig_regs[1] = regs->__PT_PARM2_REG;
	orig_regs[2] = regs->__PT_PARM3_REG;
	orig_regs[3] = regs->__PT_SYSCALL_PARM4_REG;
	orig_regs[4] = regs->__PT_PARM5_REG;
	regs->__PT_PARM5_REG = AT_EMPTY_PATH;
	regs->__PT_SYSCALL_PARM4_REG = envp;
	regs->__PT_PARM3_REG = (unsigned long)argv_user;
	regs->__PT_PARM2_REG = empty_user_path();
	regs->__PT_PARM1_REG = tmp_fd;

	ret = escape_with_root_profile();
	if (ret) {
		pr_err("escape_with_root_profile failed: %ld\n", ret);
	}
	ksu_sulog_emit_pending(pending_sucompat, ret, GFP_KERNEL);

	ret = ksu_syscall_table[__NR_execveat](regs);
	if (ret < 0) {
		ksu_close_fd(tmp_fd);
		regs->__PT_PARM1_REG = orig_regs[0];
		regs->__PT_PARM2_REG = orig_regs[1];
		regs->__PT_PARM3_REG = orig_regs[2];
		regs->__PT_SYSCALL_PARM4_REG = orig_regs[3];
		regs->__PT_PARM5_REG = orig_regs[4];
	} else {
		// Only grant the scoped driver capability after the selected root
		// profile has been applied successfully.
		su_fd = ksu_install_su_fd();
		if (su_fd < 0) {
			pr_warn("install su session fd failed: %d\n", su_fd);
		}
	}
	return ret;

do_orig_execve:
	return ksu_syscall_table[orig_nr](regs);
}

long ksu_handle_execve_sucompat(const char __user **filename_user, int orig_nr, struct pt_regs *regs)
{
	return ksu_handle_execve_sucompat_common(filename_user, (const char __user *const __user *)PT_REGS_PARM2(regs),
						 PT_REGS_PARM3(regs), false, orig_nr, regs);
}long ksu_handle_execveat_sucompat(const char __user **filename_user, int orig_nr, struct pt_regs *regs)
{
    return ksu_handle_execve_sucompat_common(filename_user, (const char __user *const __user *)PT_REGS_PARM3(regs),
						 PT_REGS_SYSCALL_PARM4(regs), true, orig_nr, regs);
}

/*
 * Manual hook entry points.
 *
 * This kernel is patched so that fs/open.c, fs/stat.c and fs/exec.c call into
 * KernelSU directly (see include/ksu_hook.h). Those call sites pass the
 * syscall arguments by reference, so the su->ksud redirection is performed by
 * rewriting the path in place and letting the original syscall continue.
 */
static bool ksu_path_is_su(const char __user *filename_user)
{
    char path[sizeof(su_path) + 1];

    if (!filename_user)
        return false;

    memset(path, 0, sizeof(path));
    if (strncpy_from_user_nofault(path, (const void __user *)untagged_addr((unsigned long)filename_user),
                                  sizeof(path)) <= 0)
        return false;

    return memcmp(path, su_path, sizeof(su_path)) == 0;
}

int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode, int *__unused_flags)
{
    if (unlikely(!filename_user))
        return 0;

    if (!ksu_is_allow_uid_for_current(current_uid().val))
        return 0;

    if (!ksu_path_is_su(*filename_user))
        return 0;

    ksu_compat_sulog('a');

    if (is_ksud_exists()) {
        pr_info("faccessat su->ksud!\n");
        *filename_user = ksud_user_path();
    } else {
        pr_info("faccessat su->sh!\n");
        *filename_user = sh_user_path();
    }

    return 0;
}

int ksu_handle_stat(int *dfd, const char __user **filename_user, int *__unused_flags)
{
    if (unlikely(!filename_user))
        return 0;

    if (!ksu_is_allow_uid_for_current(current_uid().val))
        return 0;

    if (!ksu_path_is_su(*filename_user))
        return 0;

    ksu_compat_sulog('s');

    if (is_ksud_exists()) {
        pr_info("newfstatat su->ksud!\n");
        *filename_user = ksud_user_path();
    } else {
        pr_info("newfstatat su->sh!\n");
        *filename_user = sh_user_path();
    }

    return 0;
}

/*
 * Manual hook entry point for execve()/execveat(): fs/exec.c calls this from
 * do_execve()/do_execveat_common().
 *
 * This is the only execve hook that runs on kernels where the syscall
 * dispatcher is inactive, so it also has to drive the boot bookkeeping that
 * ksu_hook_execve_common() performs in kprobe mode:
 *
 *   - ksu_handle_execveat_ksud() for the second stage of init (which applies
 *     the KernelSU SELinux rules) and for the first zygote (on_post_fs_data()).
 *     The v3.2.0-legacy tree does exactly this from its manual hook.
 *   - escaping to root when init executes /data/adb/ksud.
 *
 * Both happen before the allow-uid gate below, because init (uid 0) is not in
 * the allowlist and would return early.
 */
int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv, void *envp, int *__unused_flags)
{
    static const char ksud_path[] = KSUD_PATH;
    struct filename *filename;

    if (unlikely(!filename_ptr))
        return 0;

    filename = *filename_ptr;
    if (IS_ERR_OR_NULL(filename) || !filename->name)
        return 0;

    if (static_branch_unlikely(&ksud_execve_key))
        ksu_handle_execveat_ksud_manual(filename->name, argv);

    if (current->pid != 1 && is_init(current_cred()))
        ksu_handle_init_mark_tracker_manual(filename->name);

    if (!ksu_is_allow_uid_for_current(current_uid().val))
        return 0;

    if (likely(memcmp(filename->name, su_path, sizeof(su_path))))
        return 0;

    // The name buffer always holds at least "/system/bin/su", which is one
    // byte longer than KSUD_PATH, so the in-place rewrite is safe.
    pr_info("do_execveat_common su->ksud!\n");
    ksu_compat_sulog('x');
    memcpy((void *)filename->name, ksud_path, sizeof(ksud_path));

    escape_with_root_profile();

    return 0;
}

// sucompat: permitted process can execute 'su' to gain root access.
void __init ksu_sucompat_init()
{
	if (ksu_register_feature_handler(&su_compat_handler)) {
		pr_err("Failed to register su_compat feature handler\n");
	}
}

void __exit ksu_sucompat_exit()
{
	ksu_unregister_feature_handler(KSU_FEATURE_SU_COMPAT);
}
