/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __KSU_HOOK_H
#define __KSU_HOOK_H

#include <linux/fs.h>
#include <linux/types.h>

extern bool ksu_input_hook __read_mostly;

int ksu_handle_execveat(int *fd, struct filename **filename_ptr,
                        void *argv, void *envp, int *flags);

int ksu_handle_faccessat(int *dfd, const char __user **filename_user,
                         int *mode, int *flags);

int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags);

int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,
                        size_t *count_ptr, loff_t **pos);

int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code,
                                  int *value);

int ksu_handle_sys_reboot(int magic1, int magic2, unsigned int cmd,
                          void __user **arg);

int ksu_handle_slow_avc_audit(u32 *tsid);

#endif /* __KSU_HOOK_H */
