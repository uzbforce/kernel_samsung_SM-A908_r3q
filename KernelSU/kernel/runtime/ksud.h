#ifndef __KSU_H_KSUD
#define __KSU_H_KSUD

#include <asm/syscall.h>

#define KSUD_PATH "/data/adb/ksud"

void ksu_ksud_init();
void ksu_ksud_exit();

void ksu_execve_hook_ksud(const struct pt_regs *regs);
void ksu_execveat_hook_ksud(const struct pt_regs *regs);
void ksu_stop_input_hook_runtime(void);

/*
 * Manual hook (nokprobe) entry point for the ksud/init bookkeeping.
 *
 * The kprobe mode drives this from the sys_enter tracepoint through
 * ksu_execve_hook_ksud()/ksu_execveat_hook_ksud(), which need pt_regs. In
 * manual mode fs/exec.c calls into the already resolved `struct filename`, so
 * the path and argv are handed over directly instead.
 */
void ksu_handle_execveat_ksud_manual(const char *path, void *argv);

#endif
