#pragma once
#include "bench_options.h"
#include "bench_report.h"

// Run every scenario selected in o.scenarios except "core" (which the driver's main()
// handles), emitting rows through `rp`.
void bench_run_scenarios_cpp(BenchReport& rp, const BenchOptions& o);
