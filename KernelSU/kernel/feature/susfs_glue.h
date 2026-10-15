#ifndef __KSU_H_SUSFS_GLUE
#define __KSU_H_SUSFS_GLUE

#include <linux/types.h>

/* Exported variables required by fs/susfs.c */
extern u32 susfs_zygote_sid;
extern bool susfs_is_umount_for_zygote_system_process_enabled;

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
extern bool susfs_is_log_enabled;
#else
#define susfs_is_log_enabled 0
#endif

/* Domain helpers – externs called by fs/susfs.c and fs/namespace.c */
bool susfs_is_current_ksu_domain(void);
bool susfs_is_current_zygote_domain(void);

/* Root helpers */
bool susfs_is_allow_su(void);
void ksu_escape_to_root(void);

/* Umount bridge – extern in fs/susfs.c */
void ksu_try_umount(const char *mnt, bool check_mnt, int flags, uid_t uid);
void susfs_try_umount_all(uid_t uid);

/* Init / exit */
void ksu_susfs_glue_init(void);
void ksu_susfs_glue_exit(void);

#endif /* __KSU_H_SUSFS_GLUE */
