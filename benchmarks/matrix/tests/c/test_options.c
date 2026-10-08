/*
 * Tests for the shared CLI option parser (benchmarks/bench_options.h).
 * Exit 0 = all pass, nonzero = failure.
 */
#include "bench_options.h"
#include "bench_scenarios.h"
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

static void test_scenarios_default_all(void) {
    BenchOptions o;
    const char *argv[] = {"bench"};
    parse_argv(&o, 1, argv);
    check("scenarios_default_is_all", o.scenarios == BENCH_SCN_ALL);
    int every = 1;
    for (int i = 0; i < BENCH_NUM_SCENARIOS; ++i)
        every = every && bench_scenario_enabled(&o, (BenchScenario)(1 << i));
    check("scenarios_default_every_scenario_enabled", every);
    check("scenarios_default_no_image_dir", o.image_dir[0] == '\0');
}

static void test_scenarios_list(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--scenarios", "chain,fixed,conv"};
    int rc = parse_argv(&o, 3, argv);
    check("scenarios_list_ok", rc == 0);
    check("scenarios_list_selected", bench_scenario_enabled(&o, BENCH_SCN_CHAIN) &&
                                     bench_scenario_enabled(&o, BENCH_SCN_FIXED) &&
                                     bench_scenario_enabled(&o, BENCH_SCN_CONV));
    check("scenarios_list_others_off", !bench_scenario_enabled(&o, BENCH_SCN_CORE) &&
                                       !bench_scenario_enabled(&o, BENCH_SCN_CLIFF) &&
                                       !bench_scenario_enabled(&o, BENCH_SCN_BATCH) &&
                                       !bench_scenario_enabled(&o, BENCH_SCN_BLOCK) &&
                                       !bench_scenario_enabled(&o, BENCH_SCN_TRI));
}

static void test_scenarios_all_and_inline(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--scenarios=core", "--scenarios=all"};
    parse_argv(&o, 3, argv);
    check("scenarios_all_restores_everything", o.scenarios == BENCH_SCN_ALL);
    const char *argv2[] = {"bench", "--scenarios=core"};
    parse_argv(&o, 2, argv2);
    check("scenarios_inline_value", o.scenarios == BENCH_SCN_CORE);
}

static void test_scenarios_invalid(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--scenarios", "chain,bogus"};
    int rc = parse_argv(&o, 3, argv);
    check("scenarios_invalid_name_rc", rc != 0 && o.error != 0);
    const char *argv2[] = {"bench", "--scenarios", ","};
    rc = parse_argv(&o, 3, argv2);
    check("scenarios_empty_list_rc", rc != 0 && o.error != 0);
}

static void test_no_fixed_is_order_independent(void) {
    BenchOptions o;
    const char *a1[] = {"bench", "--no-fixed", "--scenarios", "all"};
    parse_argv(&o, 4, a1);
    check("no_fixed_before_scenarios_all", !bench_scenario_enabled(&o, BENCH_SCN_FIXED) &&
                                           bench_scenario_enabled(&o, BENCH_SCN_CHAIN));
    const char *a2[] = {"bench", "--scenarios", "all", "--no-fixed"};
    parse_argv(&o, 4, a2);
    check("no_fixed_after_scenarios_all", !bench_scenario_enabled(&o, BENCH_SCN_FIXED));
}

static void test_image_dir_and_list_scenarios(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--image-dir", "out/images"};
    int rc = parse_argv(&o, 3, argv);
    check("image_dir_parsed", rc == 0 && strcmp(o.image_dir, "out/images") == 0);
    const char *argv2[] = {"bench", "--list-scenarios"};
    parse_argv(&o, 2, argv2);
    check("list_scenarios_flag", o.list_scenarios == 1);
}

static void test_scenario_names_table(void) {
    check("scenario_table_matches_enum_count", BENCH_NUM_SCENARIOS == 8);
    check("scenario_table_first_is_core", strcmp(BENCH_SCENARIO_NAMES[0], "core") == 0);
    check("scenario_all_mask", BENCH_SCN_ALL == 0xFFu);
}

static void test_iters_scaled(void) {
    BenchOptions o;
    const char *argv[] = {"bench", "--iters", "20"};
    parse_argv(&o, 3, argv);
    /* huge work: untouched */
    check("iters_scaled_large_work_unscaled", bench_iters_scaled(&o, 1.0e9) == 20);
    /* tiny work: multiplied but capped */
    check("iters_scaled_tiny_work_capped",
          bench_iters_scaled(&o, 1.0) == (int)(20.0 * BENCH_SCALE_MAX));
    /* monotone: less work never means fewer iterations */
    int mono = 1;
    int prev = bench_iters_scaled(&o, 1.0e8);
    for (double w = 1.0e7; w >= 1.0; w /= 4.0) {
        int it = bench_iters_scaled(&o, w);
        if (it < prev) mono = 0;
        prev = it;
    }
    check("iters_scaled_monotone_in_work", mono);
    /* a depends only on (iters, work): two option structs agree */
    BenchOptions o2;
    parse_argv(&o2, 3, argv);
    check("iters_scaled_is_pure", bench_iters_scaled(&o, 5000.0) == bench_iters_scaled(&o2, 5000.0));
    BenchOptions seven;
    const char *argv7[] = {"bench", "--iters", "7"};
    parse_argv(&seven, 3, argv7);
    check("iters_scaled_respects_iters_flag", bench_iters_scaled(&seven, 1.0e9) == 7);
}

static void test_scn_name_grammar(void) {
    char b[96];
    bench_scn_name(b, sizeof(b), "c", "chain", "add", 256, 8, 256);
    check("name_with_param", strcmp(b, "c_chain_add_n256_8x256") == 0);
    bench_scn_name(b, sizeof(b), "cpp", "fixed", "mul", -1, 4, 4);
    check("name_without_param", strcmp(b, "cpp_fixed_mul_4x4") == 0);
    bench_scn_name(b, sizeof(b), "cpp", "block", "mul", BENCH_BLOCK_N, 64, 64);
    check("name_block", strcmp(b, "cpp_block_mul_n512_64x64") == 0);
    bench_scn_name(b, sizeof(b), "c", "batch", "xform4", -1, 1024, 4);
    check("name_batch_nonsquare", strcmp(b, "c_batch_xform4_1024x4") == 0);
}

static void test_sweep_constants(void) {
    check("chain_range", BENCH_CHAIN_MIN_K == 2 && BENCH_CHAIN_MAX_K == 16);
    check("fixed_range", BENCH_FIXED_MIN_N == 2 && BENCH_FIXED_MAX_N == 16);
    int sorted = 1;
    for (int i = 1; i < BENCH_NUM_CLIFF_SIZES; ++i) if (BENCH_CLIFF_SIZES[i] <= BENCH_CLIFF_SIZES[i - 1]) sorted = 0;
    for (int i = 1; i < BENCH_NUM_BATCH_POINTS; ++i) if (BENCH_BATCH_POINTS[i] <= BENCH_BATCH_POINTS[i - 1]) sorted = 0;
    for (int i = 1; i < BENCH_NUM_BLOCK_SIZES; ++i) if (BENCH_BLOCK_SIZES[i] <= BENCH_BLOCK_SIZES[i - 1]) sorted = 0;
    for (int i = 1; i < BENCH_NUM_TRI_SIZES; ++i) if (BENCH_TRI_SIZES[i] <= BENCH_TRI_SIZES[i - 1]) sorted = 0;
    for (int i = 1; i < BENCH_NUM_CONV_SIZES; ++i) if (BENCH_CONV_SIZES[i] <= BENCH_CONV_SIZES[i - 1]) sorted = 0;
    for (int i = 1; i < BENCH_NUM_CHAIN_SIZES; ++i) if (BENCH_CHAIN_SIZES[i] <= BENCH_CHAIN_SIZES[i - 1]) sorted = 0;
    check("sweeps_strictly_increasing", sorted);
    /* every block (offset + size) must fit inside the BENCH_BLOCK_N matrices */
    size_t largest = BENCH_BLOCK_SIZES[BENCH_NUM_BLOCK_SIZES - 1];
    check("blocks_fit_in_matrix",
          BENCH_BLOCK_A_ROW + largest <= BENCH_BLOCK_N && BENCH_BLOCK_A_COL + largest <= BENCH_BLOCK_N &&
          BENCH_BLOCK_B_ROW + largest <= BENCH_BLOCK_N && BENCH_BLOCK_B_COL + largest <= BENCH_BLOCK_N &&
          BENCH_BLOCK_O_ROW + largest <= BENCH_BLOCK_N && BENCH_BLOCK_O_COL + largest <= BENCH_BLOCK_N);
    int image_in_sweep = 0;
    for (int i = 0; i < BENCH_NUM_CONV_SIZES; ++i) if (BENCH_CONV_SIZES[i] == BENCH_CONV_IMAGE_N) image_in_sweep = 1;
    check("conv_image_size_is_in_the_sweep", image_in_sweep);
    int cliff_ops_valid = 1;
    for (int i = 0; i < BENCH_NUM_CLIFF_OPS; ++i)
        cliff_ops_valid = cliff_ops_valid && bench__valid_op_name(BENCH_CLIFF_OPS[i]);
    check("cliff_ops_are_valid_core_ops", cliff_ops_valid);
}

static void test_work_estimates(void) {
    check("work_heavy_ops_are_cubic", bench_work_op("mul", 10.0) == 1000.0 &&
                                      bench_work_op("mul_add", 10.0) == 1000.0 &&
                                      bench_work_op("transpose_mul", 10.0) == 1000.0);
    check("work_light_ops_are_quadratic", bench_work_op("add", 10.0) == 100.0 &&
                                          bench_work_op("matvec", 10.0) == 100.0);
    check("work_block_variants", bench_work_block("mul", 8.0) == 512.0 && bench_work_block("copy", 8.0) == 64.0);
    check("work_chain_scales_with_k_and_n", bench_work_chain(4, 10.0) == 400.0);
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
    test_scenarios_default_all();
    test_scenarios_list();
    test_scenarios_all_and_inline();
    test_scenarios_invalid();
    test_no_fixed_is_order_independent();
    test_image_dir_and_list_scenarios();
    test_scenario_names_table();
    test_iters_scaled();
    test_scn_name_grammar();
    test_sweep_constants();
    test_work_estimates();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0 ? 1 : 0;
}
