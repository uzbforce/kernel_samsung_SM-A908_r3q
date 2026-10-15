#ifndef __KSU_H_SYSCALL_EVENT_BRIDGE
#define __KSU_H_SYSCALL_EVENT_BRIDGE

#include <asm/ptrace.h>
#include <linux/jump_label.h>

/* Set until the first zygote is seen; see ksu_stop_ksud_execve_hook(). */
extern struct static_key_true ksud_execve_key;

long ksu_hook_newfstatat(int orig_nr, const struct pt_regs *regs);
long ksu_hook_faccessat(int orig_nr, const struct pt_regs *regs);
long ksu_hook_execve(int orig_nr, const struct pt_regs *regs);
long ksu_hook_execveat(int orig_nr, const struct pt_regs *regs);
long ksu_hook_setresuid(int orig_nr, const struct pt_regs *regs);

void ksu_stop_ksud_execve_hook(void);

/*
 * Manual hook mode variant of the init exec tracking below: fs/exec.c hands
 * over the resolved kernel side filename instead of a user pointer.
 */
void ksu_handle_init_mark_tracker_manual(const char *path);

#endif // __KSU_H_SYSCALL_EVENT_BRIDGE
