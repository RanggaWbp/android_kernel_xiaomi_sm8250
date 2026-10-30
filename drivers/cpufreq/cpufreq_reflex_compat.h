/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Reflex CPUFreq governor: Linux 4.19-only API shims.
 * cpufreq_reflex.c is kept byte-close to upstream (6.12); every version
 * difference lives in this header.
 *
 * 4.19 deltas vs 6.12:
 *  - No cpufreq_driver_test_flags() / CPUFREQ_NEED_UPDATE_LIMITS.
 *  - No cpufreq_driver_adjust_perf() / ->has_adjust_perf(); only the
 *    freq (slow/fast-switch) path exists.  The perf fast path is dropped.
 *  - arm64 arch_scale_cpu_capacity() is topology_get_cpu_scale(sd, cpu).
 */
#ifndef _CPUFREQ_REFLEX_COMPAT_H
#define _CPUFREQ_REFLEX_COMPAT_H

#include <linux/cpufreq.h>
#include <linux/stddef.h>	/* NULL */

#define CPUFREQ_NEED_UPDATE_LIMITS	(0)
#define cpufreq_driver_test_flags(f)	(0)

#ifdef CONFIG_ACPI_CPPC_LIB
/*
 * 6.12-only helper absent in 4.19; reconstruct the perf->kHz conversion
 * (perf * nominal_freq) / nominal_perf.
 */
static inline unsigned int cppc_perf_to_khz(struct cppc_perf_caps *caps,
					    unsigned int perf)
{
	return (perf * caps->nominal_freq) / caps->nominal_perf;
}
#endif

#endif /* _CPUFREQ_REFLEX_COMPAT_H */
