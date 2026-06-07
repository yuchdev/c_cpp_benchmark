/*
 * Tests for the shared CLI option parser (benchmarks/bench_options.h).
 * Exit 0 = all pass, nonzero = failure.
 */
#include "bench_options.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int cond) {
    if (cond) { printf("[PASS] %s\n", name); ++g_pass; }
    else       { printf("[FAIL] %s\n", name); ++g_fail; }
}

static int parse_argv(BenchOptions *o, int argc, const char **argv) {
    /* bench_parse_args takes char**; cast away constness for the test */
    return bench_parse_args(o, argc, (char **)argv);
}

static void test_defaults(void) {
    BenchOptions o;
    const char *argv[] = {"bench"};
    int rc = parse_argv(&o, 1, argv);
    check("defaults_parse_ok", rc == 0 && o.error == 0);
    check("defaults_sizes", o.num_sizes == 3 && o.sizes[0] == 32 &&
                            o.sizes[1] == 128 && o.sizes[2] == 512);
    check("defaults_ops_all", o.num_ops == 0);
    check("defaults_iters", o.iters == 20 && o.warmup == 3 && o.repeats == 1);
    check("defaults_seed", o.seed == 1u);
    check("defaults_format", o.format == BENCH_FMT_TABLE);
}

static void test_positional_csv(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "results/foo.csv"};
    parse_argv(&o, 2, argv);
    check("positional_csv_enabled", o.csv_enabled == 1);
    check("positional_csv_path", strcmp(o.csv_path, "results/foo.csv") == 0);
}

static void test_sizes_list(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--sizes", "16,64,256"};
    int rc = parse_argv(&o, 3, argv);
    check("sizes_list_ok", rc == 0);
    check("sizes_list_vals", o.num_sizes == 3 && o.sizes[0] == 16 &&
                             o.sizes[1] == 64 && o.sizes[2] == 256);
}

static void test_sizes_geometric(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--sizes", "64:x2:4"};
    int rc = parse_argv(&o, 3, argv);
    check("sizes_geo_ok", rc == 0);
    check("sizes_geo_vals", o.num_sizes == 4 && o.sizes[0] == 64 &&
                            o.sizes[1] == 128 && o.sizes[2] == 256 &&
                            o.sizes[3] == 512);
}

static void test_sizes_invalid(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--sizes", "abc"};
    int rc = parse_argv(&o, 3, argv);
    check("sizes_invalid_rc", rc != 0 && o.error != 0);
}

static void test_ops_filter(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--ops", "add,mul"};
    int rc = parse_argv(&o, 3, argv);
    check("ops_filter_ok", rc == 0 && o.num_ops == 2);
    check("ops_filter_add", bench_op_enabled(&o, "add"));
    check("ops_filter_mul", bench_op_enabled(&o, "mul"));
    check("ops_filter_not_sub", !bench_op_enabled(&o, "sub"));
}

static void test_ops_all(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--ops", "all"};
    parse_argv(&o, 3, argv);
    check("ops_all_num0", o.num_ops == 0);
    check("ops_all_any_enabled", bench_op_enabled(&o, "transpose") &&
                                 bench_op_enabled(&o, "mul_add"));
}

static void test_ops_invalid(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--ops", "add,bogus"};
    int rc = parse_argv(&o, 3, argv);
    check("ops_invalid_rc", rc != 0 && o.error != 0);
}

static void test_inline_value(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--iters=50", "--warmup=2", "--seed=7"};
    int rc = parse_argv(&o, 4, argv);
    check("inline_ok", rc == 0);
    check("inline_iters", o.iters == 50);
    check("inline_warmup", o.warmup == 2);
    check("inline_seed", o.seed == 7u);
}

static void test_format_and_flags(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--format", "json", "--no-fixed",
                          "--repeats", "5", "--heavy-divisor", "8"};
    int rc = parse_argv(&o, 8, argv);
    check("flags_ok", rc == 0);
    check("flags_format_json", o.format == BENCH_FMT_JSON);
    check("flags_no_fixed", o.fixed_enabled == 0);
    check("flags_repeats", o.repeats == 5);
    check("flags_heavy_divisor", o.heavy_divisor == 8);
}

static void test_csv_disable(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--csv", "-"};
    parse_argv(&o, 3, argv);
    check("csv_disable", o.csv_enabled == 0);
}

static void test_unknown_option(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--bogus"};
    int rc = parse_argv(&o, 2, argv);
    check("unknown_option_rc", rc != 0 && o.error != 0);
}

static void test_help(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--help"};
    int rc = parse_argv(&o, 2, argv);
    check("help_flag", rc == 0 && o.help == 1);
}

static void test_heavy_iters(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--iters", "40", "--heavy-divisor", "4"};
    parse_argv(&o, 5, argv);
    /* heavy op at large N gets divided */
    check("heavy_iters_large", bench_iters_for(&o, "mul", 512) == 10);
    /* light op unaffected */
    check("light_iters_large", bench_iters_for(&o, "add", 512) == 40);
    /* heavy op at small N unaffected */
    check("heavy_iters_small", bench_iters_for(&o, "mul", 32) == 40);
}

int main(void) {
    test_defaults();
    test_positional_csv();
    test_sizes_list();
    test_sizes_geometric();
    test_sizes_invalid();
    test_ops_filter();
    test_ops_all();
    test_ops_invalid();
    test_inline_value();
    test_format_and_flags();
    test_csv_disable();
    test_unknown_option();
    test_help();
    test_heavy_iters();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0 ? 1 : 0;
}
