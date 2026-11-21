// SPDX-License-Identifier: GPL-2.0
/*
 * BORE 7.0.0 portable policy, by Masahito Suzuki <firelzrd@gmail.com>.
 * CFS adaptation: no EEVDF deadlines, protected slices or sleep credits.
 */
#include "sched.h"
#include <linux/sched/bore.h>
#include <linux/sched/signal.h>
#include <linux/mutex.h>

unsigned int __read_mostly sched_bore = 1;
unsigned int __read_mostly sched_burst_inherit_type = 2;
unsigned int __read_mostly sched_burst_smoothness = 1;
unsigned int __read_mostly sched_burst_penalty_offset = 24;
unsigned int __read_mostly sched_burst_penalty_scale = 1536;
unsigned int __read_mostly sched_burst_cache_lifetime = 75000000;

#define MAX_BURST_PENALTY ((40U << 8) - 1)
#define BURST_CACHE_SAMPLE_LIMIT 63
#define BURST_CACHE_SCAN_LIMIT (BURST_CACHE_SAMPLE_LIMIT * 2)

static DEFINE_MUTEX(bore_mutex);

static u32 log2p1_u64_u32fp(u64 v, u8 fp)
{
	int clz, exponent;
	u32 mantissa;

	if (!v)
		return 0;
	clz = __builtin_clzll(v);
	exponent = 64 - clz;
	mantissa = (u32)((v << clz) << 1 >> (64 - fp));
	return exponent << fp | mantissa;
}

static u32 calc_burst_penalty(u64 burst_time)
{
	u32 greed = log2p1_u64_u32fp(burst_time, 8);
	u32 tolerance = READ_ONCE(sched_burst_penalty_offset) << 8;
	u32 penalty = max(0, (s32)greed - (s32)tolerance);

	return min(MAX_BURST_PENALTY,
		   penalty * READ_ONCE(sched_burst_penalty_scale) >> 10);
}

static u32 binary_smooth(u32 new, u32 old)
{
	u32 shift = READ_ONCE(sched_burst_smoothness);

	if (new <= old)
		return new;
	return old + ((new - old + (1U << shift) - 1) >> shift);
}

int effective_prio_bore(struct task_struct *p)
{
	int prio = p->static_prio - MAX_RT_PRIO;

	if (READ_ONCE(sched_bore) && !(p->flags & PF_KTHREAD))
		prio += bore_score(p);
	return clamp(prio, 0, 39);
}

static void update_penalty(struct task_struct *p)
{
	struct bore_ctx *ctx = &p->bore;
	int old_prio = effective_prio_bore(p);
	int new_prio;

	WRITE_ONCE(ctx->penalty, (p->flags & PF_KTHREAD) ? 0 :
		   max(ctx->curr_penalty, ctx->prev_penalty));
	new_prio = effective_prio_bore(p);
	if (old_prio != new_prio && !idle_policy(p->policy))
		reweight_task(p, new_prio);
}

void update_curr_bore(struct task_struct *p, u64 delta_exec)
{
	struct bore_ctx *ctx = &p->bore;

	if (ctx->stop_update || !READ_ONCE(sched_bore))
		return;
	/* Saturate rather than wrap after an exceptionally long burst. */
	ctx->burst_time += min(delta_exec, U64_MAX - ctx->burst_time);
	ctx->curr_penalty = calc_burst_penalty(ctx->burst_time);
	if (ctx->curr_penalty > ctx->prev_penalty)
		update_penalty(p);
}

void restart_burst_bore(struct task_struct *p)
{
	struct bore_ctx *ctx = &p->bore;

	if (!READ_ONCE(sched_bore))
		return;
	ctx->prev_penalty = binary_smooth(ctx->curr_penalty, ctx->prev_penalty);
	ctx->curr_penalty = 0;
	ctx->burst_time = 0;
	update_penalty(p);
}

void reset_task_bore(struct task_struct *p)
{
	/* Live callers hold rq lock, not tasklist_lock: leave shared caches alone. */
	p->bore.burst_time = 0;
	p->bore.prev_penalty = 0;
	p->bore.curr_penalty = 0;
	WRITE_ONCE(p->bore.penalty, 0);
	p->bore.stop_update = false;
}

static bool task_is_bore_eligible(struct task_struct *p)
{
	return p->sched_class == &fair_sched_class && !READ_ONCE(p->exit_state);
}

/* All topology and cache accesses below hold tasklist_lock for writing.
 * No RCU conversion of the legacy children/sibling lists is needed.
 */
static unsigned int count_children_upto2(struct task_struct *p)
{
	struct list_head *head = &p->children;
	struct list_head *first = head->next;

	if (first == head)
		return 0;
	return first->next == head ? 1 : 2;
}

static bool burst_cache_expired(struct bore_bc *bc, u64 now)
{
	return !bc->timestamp ||
		now - bc->timestamp > READ_ONCE(sched_burst_cache_lifetime);
}

static void update_burst_cache(struct bore_bc *bc, struct task_struct *p,
			       u32 count, u32 total, u64 now)
{
	/* At most 63 samples: exact integer average needs no reciprocal LUT. */
	bc->penalty = max(count ? total / count : 0,
			 (u32)READ_ONCE(p->bore.penalty));
	bc->timestamp = now;
}

static u16 inherit_from_parent(struct task_struct *parent, u64 now)
{
	struct bore_bc *bc = &parent->bore.subtree;
	struct task_struct *child;
	u32 count = 0, total = 0, scanned = 0;

	if (burst_cache_expired(bc, now)) {
		list_for_each_entry(child, &parent->children, sibling) {
			if (count >= BURST_CACHE_SAMPLE_LIMIT ||
			    scanned++ >= BURST_CACHE_SCAN_LIMIT)
				break;
			if (!task_is_bore_eligible(child))
				continue;
			count++;
			total += READ_ONCE(child->bore.penalty);
		}
		update_burst_cache(bc, parent, count, total, now);
	}
	return bc->penalty;
}

static u16 inherit_from_ancestor_hub(struct task_struct *parent,
				    unsigned long clone_flags, u64 now)
{
	struct task_struct *ancestor = parent, *child, *descendant;
	struct bore_bc *bc;
	u32 count = 0, total = 0, scanned = 0, hops = 0;
	u32 sole_child_count = !!(clone_flags & CLONE_PARENT);

	/* Bound both upward and downward single-child chains, not just samples. */
	while (ancestor->real_parent != ancestor &&
	       count_children_upto2(ancestor) <= sole_child_count &&
	       hops++ < BURST_CACHE_SCAN_LIMIT) {
		ancestor = ancestor->real_parent;
		sole_child_count = 1;
	}
	bc = &ancestor->bore.subtree;
	if (burst_cache_expired(bc, now)) {
		list_for_each_entry(child, &ancestor->children, sibling) {
			if (count >= BURST_CACHE_SAMPLE_LIMIT ||
			    scanned++ >= BURST_CACHE_SCAN_LIMIT)
				break;
			descendant = child;
			while (count_children_upto2(descendant) == 1 &&
			       scanned < BURST_CACHE_SCAN_LIMIT) {
				descendant = list_first_entry(&descendant->children,
						struct task_struct, sibling);
				scanned++;
			}
			if (!task_is_bore_eligible(descendant))
				continue;
			count++;
			total += READ_ONCE(descendant->bore.penalty);
		}
		update_burst_cache(bc, ancestor, count, total, now);
	}
	return bc->penalty;
}

static u16 inherit_from_thread_group(struct task_struct *parent, u64 now)
{
	struct task_struct *leader = parent->group_leader, *thread;
	struct bore_bc *bc = &leader->bore.group;
	u32 count = 0, total = 0, scanned = 0;

	if (burst_cache_expired(bc, now)) {
		for_each_thread(leader, thread) {
			if (count >= BURST_CACHE_SAMPLE_LIMIT ||
			    scanned++ >= BURST_CACHE_SCAN_LIMIT)
				break;
			if (!task_is_bore_eligible(thread))
				continue;
			count++;
			total += READ_ONCE(thread->bore.penalty);
		}
		update_burst_cache(bc, leader, count, total, now);
	}
	return bc->penalty;
}

void task_fork_bore(struct task_struct *p, struct task_struct *parent,
		    unsigned long clone_flags, u64 now)
{
	u16 inherited = 0;
	u16 parent_penalty = READ_ONCE(parent->bore.prev_penalty);
	unsigned int type = READ_ONCE(sched_burst_inherit_type);
	int prio;

	/* Called under write_lock_irq(tasklist_lock), before publication.
	 * Runtime toggles use the same lock: no child can miss their sweep.
	 * Never run reweight_entity on a newborn's unattached PELT averages.
	 */
	if (READ_ONCE(sched_bore) && task_is_bore_eligible(p)) {
		if (clone_flags & CLONE_THREAD)
			inherited = inherit_from_thread_group(parent, now);
		else {
			if (clone_flags & CLONE_PARENT)
				parent = parent->real_parent;
			if (type == 2)
				inherited = inherit_from_ancestor_hub(parent, clone_flags, now);
			else if (type == 1)
				inherited = inherit_from_parent(parent, now);
		}
	}
	if (READ_ONCE(sched_bore) && task_is_bore_eligible(p))
		inherited = max(inherited, parent_penalty);
	p->bore.prev_penalty = inherited;
	p->bore.penalty = (p->flags & PF_KTHREAD) ? 0 : inherited;
	prio = effective_prio_bore(p);
	if (!idle_policy(p->policy)) {
		p->se.load.weight = scale_load(sched_prio_to_weight[prio]);
		p->se.load.inv_weight = sched_prio_to_wmult[prio];
	}
	p->se.runnable_weight = p->se.load.weight;
	init_entity_runnable_average(&p->se);
}

void __init sched_init_bore(void)
{
	reset_task_bore(&init_task);
	pr_info("BORE CPU Scheduler %s by Masahito Suzuki (CFS policy port)\n",
		SCHED_BORE_VERSION);
}

int sched_bore_update_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	struct ctl_table tmp = *table;
	unsigned int value;
	struct task_struct *group, *p;
	struct rq *rq;
	struct rq_flags rf;
	int ret;

	mutex_lock(&bore_mutex);
	value = READ_ONCE(sched_bore);
	tmp.data = &value;
	ret = proc_douintvec_minmax(&tmp, write, buffer, lenp, ppos);
	if (ret || !write || value == READ_ONCE(sched_bore))
		goto out;

	/* Lock order matches normalize_rt_tasks(): tasklist -> pi_lock -> rq.
	 * Serialize visibility with copy_process(), including all CLONE_THREADs.
	 */
	write_lock_irq(&tasklist_lock);
	WRITE_ONCE(sched_bore, value);
	for_each_process_thread(group, p) {
		rq = task_rq_lock(p, &rf);
		if (task_is_bore_eligible(p)) {
			update_rq_clock(rq);
			if (rq->curr->sched_class == &fair_sched_class)
				fair_sched_class.update_curr(rq);
			/* Drop stale bursts/caches for deterministic disable/re-enable. */
			reset_task_bore(p);
			memset(&p->bore.subtree, 0, sizeof(p->bore.subtree));
			memset(&p->bore.group, 0, sizeof(p->bore.group));
			if (READ_ONCE(p->state) == TASK_NEW) {
				int prio = effective_prio_bore(p);

				/* Published, but wake_up_new_task has not attached PELT. */
				if (!idle_policy(p->policy)) {
					p->se.load.weight = scale_load(sched_prio_to_weight[prio]);
					p->se.load.inv_weight = sched_prio_to_wmult[prio];
				}
				p->se.runnable_weight = p->se.load.weight;
				init_entity_runnable_average(&p->se);
			} else {
				reweight_task(p, effective_prio_bore(p));
			}
			resched_curr(rq);
		}
		task_rq_unlock(rq, p, &rf);
	}
	write_unlock_irq(&tasklist_lock);
out:
	mutex_unlock(&bore_mutex);
	return ret;
}
