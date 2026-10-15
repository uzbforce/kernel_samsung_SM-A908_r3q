#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0) || defined(CONFIG_IS_HW_HISI) ||                                     \
    defined(CONFIG_KSU_ALLOWLIST_WORKAROUND)
static int ksu_key_permission(key_ref_t key_ref, const struct cred *cred, unsigned perm)
{
    if (init_session_keyring != NULL) {
        return 0;
    }

    if (strcmp(current->comm, "init")) {
        // we are only interested in `init` process
        return 0;
    }
    init_session_keyring = cred->session_keyring;
    pr_info("kernel_compat: got init_session_keyring\n");
    return 0;
}
#endif

#ifdef CONFIG_KSU_SUSFS
extern u32 susfs_zygote_sid;
extern u32 susfs_zygote_next_sid;
extern void disable_seccomp(void);
extern struct work_struct susfs_extra_works;

static inline void ksu_handle_extra_susfs_work(void)
{
    if (work_pending(&susfs_extra_works))
        return;

    schedule_work(&susfs_extra_works);
}

static int handle_zygote_setresuid(uid_t ruid) {
    // Check if spawned process is isolated service first, and force to do umount if so
    if (is_isolated_process(ruid)) {
        susfs_set_current_proc_no_su();
        susfs_set_current_proc_umounted();
        goto do_umount;
    }

    // - Since ksu maanger app uid is excluded in allow_list_arr, so ksu_uid_should_umount(manager_uid)
    //   will always return true, that's why we need to explicitly check if new_uid belongs to
    //   ksu manager.
    // - Disable seccomp restriction for KSU manager since running with "su" will disable seccomp anyway
    if (likely(ksu_is_manager_appid_valid()) && unlikely(is_uid_manager(ruid))) {
        disable_seccomp();
        pr_info("install fd for manager: %d\n", ruid);
        ksu_install_fd();
        return 0;
    }

    // - Check if spawned process is normal user app and needs to be umounted
    // - Now app_profile for webview_zygote is available in KernelSU manager
    if (likely(is_appuid(ruid) && ksu_uid_should_umount(ruid))) {
        susfs_set_current_proc_no_su();
        susfs_set_current_proc_umounted();
        goto do_umount;
    }

    // Disable seccomp restriction for root allowed apps since running with "su" will disable seccomp anyway
    if (ksu_is_allow_uid_for_current(ruid)) {
        disable_seccomp();
        return 0;
    }

    // Process not umounted but also root not allowed
    susfs_set_current_proc_no_su();
    return 0;

do_umount:
    {
        // Handle kernel umount
        ksu_handle_umount(current_uid().val, ruid);

        // Handle extra susfs work
        ksu_handle_extra_susfs_work();
    }

    return 0;
}

static int handle_zygote_next_setresuid(uid_t ruid) {
    // Check if spawned process is isolated service first, and force to do umount if so
    if (is_isolated_process(ruid)) {
        susfs_set_current_proc_no_su();
        susfs_set_current_proc_umounted();
        susfs_set_current_proc_umounted_for_zygote_next();
        goto do_susfs_work;
    }

    // - Since ksu maanger app uid is excluded in allow_list_arr, so ksu_uid_should_umount(manager_uid)
    //   will always return true, that's why we need to explicitly check if new_uid belongs to
    //   ksu manager.
    // - Disable seccomp restriction for KSU manager since running with "su" will disable seccomp anyway
    if (likely(ksu_is_manager_appid_valid()) && unlikely(is_uid_manager(ruid))) {
        disable_seccomp();
        pr_info("install fd for manager: %d\n", ruid);
        ksu_install_fd();
        return 0;
    }

    // - Check if spawned process is normal user app and needs to be umounted
    // - Now app_profile for webview_zygote is available in KernelSU manager
    if (likely(is_appuid(ruid) && ksu_uid_should_umount(ruid))) {
        susfs_set_current_proc_no_su();
        susfs_set_current_proc_umounted();
        susfs_set_current_proc_umounted_for_zygote_next();
        goto do_susfs_work;
    }

    // Disable seccomp restriction for root allowed apps since running with "su" will disable seccomp anyway
    if (ksu_is_allow_uid_for_current(ruid)) {
        disable_seccomp();
        return 0;
    }

    // Process not umounted but also root not allowed
    susfs_set_current_proc_no_su();
    return 0;

do_susfs_work:
    {
        // Do not umount here as we are in init namespace now

        // Handle extra susfs work
        ksu_handle_extra_susfs_work();
    }

    return 0;
}

int ksu_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    uid_t cur_uid = current_uid().val;

    if (cur_uid != 0)
        return 0;

    // We only interest in process spwaned by zygote or zygote_next
    if (susfs_is_sid_equal(current_cred(), susfs_zygote_sid))
        return handle_zygote_setresuid(ruid);

    if (susfs_is_sid_equal(current_cred(), susfs_zygote_next_sid))
        return handle_zygote_next_setresuid(ruid);

    return 0;

}
#else
static int ksu_task_fix_setuid(struct cred *new, const struct cred *old, int flags)
{
    uid_t new_uid, old_uid = 0;

    if (unlikely(!new || !old))
        return 0;

    new_uid = new->uid.val;
    old_uid = old->uid.val;

    if (unlikely(is_uid_manager(new_uid))) {
        disable_seccomp();
        pr_info("install fd for manager: %d\n", new_uid);
        ksu_install_fd();
        return 0;
    }

    if (ksu_is_allow_uid_for_current(new_uid)) {
        disable_seccomp();
    }

    // Handle kernel umount
    ksu_handle_umount(old_uid, new_uid);

    return 0;
}
#endif

static struct security_hook_list ksu_hooks[] = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0) || defined(CONFIG_IS_HW_HISI) ||                                     \
    defined(CONFIG_KSU_ALLOWLIST_WORKAROUND)
    LSM_HOOK_INIT(key_permission, ksu_key_permission),
#endif
#ifndef CONFIG_KSU_SUSFS
    LSM_HOOK_INIT(task_fix_setuid, ksu_task_fix_setuid),
#endif
};

void __init ksu_lsm_hook_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    security_add_hooks(ksu_hooks, ARRAY_SIZE(ksu_hooks), "ksu");
#else
    // https://elixir.bootlin.com/linux/v4.10.17/source/include/linux/lsm_hooks.h#L1892
    security_add_hooks(ksu_hooks, ARRAY_SIZE(ksu_hooks));
#endif
    pr_info("LSM hooks initialized.\n");
}

void ksu_lsm_hook_exit(void)
{
}
// --- block added back by agent ---
void hook_lsm(struct security_hook_list *hook, struct hlist_head *head) {
    struct list_head *list = &hook->list;
    /*
     * Manually add 'list' to the hlist_head 'head' without using
     * hlist_node, because struct security_hook_list embeds struct
     * list_head (not hlist_node).
     *
     * struct hlist_head { struct hlist_node *first; };
     * We treat 'list' as if it were a hlist_node: its ->next plays the
     * role of hlist_node->next, and the head's ->first is the pointer
     * we write into. This matches hlist_add_head() semantics.
     */
    WRITE_ONCE(list->next, (struct list_head *)READ_ONCE(head->first));
    memcpy(&head->first, &list, sizeof(head->first));
    hook->head = (struct list_head *)head;
}

void unhook_lsm(struct security_hook_list *hook) {
    struct list_head *list = &hook->list;
    struct hlist_head *hh;
    struct list_head **pp;
    if (READ_ONCE(list->next) == NULL)
        return;
    hh = (struct hlist_head *)hook->head;
    if (!hh)
        return;
    /*
     * Walk the hlist to find the pointer that points to 'list',
     * starting from head->first. The chain is:
     *   hh->first (hlist_node *) -> node->next (hlist_node *)
     * We reinterpret each step as struct list_head *.
     */
    pp = (struct list_head **)&hh->first;
    while (*(void **)pp != list) {
        if (*(void **)pp == NULL)
            return;
        pp = (struct list_head **)&((struct list_head *)*(void **)pp)->next;
    }
    WRITE_ONCE(*pp, READ_ONCE(list->next));
    if (list->next)
        WRITE_ONCE(list->next, NULL);
}
// --- end block ---
