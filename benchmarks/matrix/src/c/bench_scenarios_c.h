#ifndef BENCH_SCENARIOS_C_H
#define BENCH_SCENARIOS_C_H

#include "bench_options.h"
#include "bench_report.h"

/* Run every scenario selected in o->scenarios except "core" (which the driver's
 * main() handles), emitting rows through `rp`. */
void bench_run_scenarios_c(BenchReport *rp, const BenchOptions *o);

#endif /* BENCH_SCENARIOS_C_H */
