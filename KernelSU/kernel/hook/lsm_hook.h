#ifndef __KSU_H_LSM_HOOK
#define __KSU_H_LSM_HOOK

#include <linux/list.h>
#include <linux/hlist.h>

void __init ksu_lsm_hook_init(void);
void ksu_lsm_hook_exit(void);
void hook_lsm(struct security_hook_list *hook, struct hlist_head *head);
void unhook_lsm(struct security_hook_list *hook);

#endif
