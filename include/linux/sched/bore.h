/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_BORE_H
#define _LINUX_SCHED_BORE_H

#include <linux/sched.h>
#include <linux/sysctl.h>

#define SCHED_BORE_VERSION "7.0.0-cfs"
extern unsigned int sched_bore;
extern unsigned int sched_burst_inherit_type;
extern unsigned int sched_burst_smoothness;
extern unsigned int sched_burst_penalty_offset;
extern unsigned int sched_burst_penalty_scale;
extern unsigned int sched_burst_cache_lifetime;

static inline u8 bore_score(struct task_struct *p)
{
	return READ_ONCE(p->bore.penalty) >> 8;
}

int effective_prio_bore(struct task_struct *p);
void update_curr_bore(struct task_struct *p, u64 delta_exec);
void restart_burst_bore(struct task_struct *p);
void reset_task_bore(struct task_struct *p);
void task_fork_bore(struct task_struct *p, struct task_struct *parent,
		    unsigned long clone_flags, u64 now);
void sched_init_bore(void);
int sched_bore_update_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos);
#endif
