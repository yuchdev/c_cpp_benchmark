/*
 * Shared, flexible benchmark CLI options for the benchmarks/matrix suite.
 *
 * This header is valid C11 *and* C++17 so that the C driver
 * (benchmarks_c.c) and the C++ driver (benchmarks_cpp.cpp) parse exactly
 * the same command-line interface.  All logic lives in `static inline`
 * functions, so there is no separate translation unit to compile/link.
 *
 * Supported options (long form, with `=` or space separated values):
 *
 *   --sizes   <list>      Comma-separated square sizes, or a geometric
 *                         progression "start:xMUL:steps" (e.g. "64:x2:4"
 *                         -> 64,128,256,512).  Default: 32,128,512
 *   --ops     <list>      Comma-separated operation names to run, or "all".
 *                         Default: all
 *   --warmup  <n>         Warmup iterations (untimed).            Default: 3
 *   --iters   <n>         Measured iterations per repeat.         Default: 20
 *   --repeats <n>         Independent repeats; the *best* (min)   Default: 1
 *                         average is reported (reduces OS jitter).
 *   --heavy-divisor <n>   Divide iters by this for O(N^3) ops     Default: 4
 *                         (mul / transpose_mul / mul_add) when N is large.
 *   --seed    <n>         Base RNG seed.                          Default: 1
 *   --csv     <path>      CSV output path ("" / "-" disables).
 *   --format  <fmt>       table | csv | json  (stdout format).   Default: table
 *   --scenarios <list>    Comma-separated benchmark scenarios, or "all".
 *                         core,chain,fixed,cliff,batch,block,tri,conv
 *                         Default: all.  (--sizes/--ops shape "core"; --ops
 *                         also filters "fixed".  The other sweeps are fixed
 *                         in bench_scenarios.h so C and C++ share them.)
 *   --no-fixed            Skip the fixed-size sweep (same as dropping "fixed"
 *                         from --scenarios, but order-independent).
 *   --image-dir <path>    Directory (must exist) for the "conv" scenario's PGM
 *                         images (input + blurred output). Default: none.
 *   --list-ops            Print the available operation names and exit.
 *   --list-scenarios      Print the available scenario names and exit.
 *   --help                Print usage and exit.
 *
 * Backward compatibility: a single bare positional argument is still treated
 * as the CSV output path, matching the historical `bench <csv>` interface.
 */
#ifndef BENCH_OPTIONS_H
#define BENCH_OPTIONS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BENCH_MAX_SIZES   64
#define BENCH_MAX_OPS      32
#define BENCH_NAME_LEN     40
#define BENCH_PATH_LEN   1024

typedef enum { BENCH_FMT_TABLE = 0, BENCH_FMT_CSV = 1, BENCH_FMT_JSON = 2 } BenchFormat;

typedef struct {
    size_t       sizes[BENCH_MAX_SIZES];
    int          num_sizes;
    char         ops[BENCH_MAX_OPS][BENCH_NAME_LEN];
    int          num_ops;        /* 0 => run all operations */
    int          warmup;
    int          iters;
    int          repeats;
    int          heavy_divisor;
    unsigned int seed;
    char         csv_path[BENCH_PATH_LEN];
    int          csv_enabled;
    BenchFormat  format;
    int          fixed_enabled;  /* fixed-size sweep; cleared by --no-fixed */
    unsigned int scenarios;      /* BenchScenario bitmask (default: all) */
    char         image_dir[BENCH_PATH_LEN]; /* "" => do not write images */
    int          list_ops;
    int          list_scenarios;
    int          help;
    int          error;          /* nonzero => parse error */
} BenchOptions;

/* Canonical list of operation names understood by the drivers. */
static const char *const BENCH_ALL_OPS[] = {
    "transpose", "add", "sub", "scale", "matvec",
    "mul", "transpose_mul", "add3", "mul_add"
};
static const int BENCH_NUM_ALL_OPS =
    (int)(sizeof(BENCH_ALL_OPS) / sizeof(BENCH_ALL_OPS[0]));

/* Benchmark scenarios selectable with --scenarios (bitmask values). */
typedef enum {
    BENCH_SCN_CORE  = 1 << 0,  /* the original op x size grid (+ shared --sizes/--ops) */
    BENCH_SCN_CHAIN = 1 << 1,  /* A1 + A2 + ... + Ak expression-chain depth sweep      */
    BENCH_SCN_FIXED = 1 << 2,  /* compile-time-N sweep, N = 2..16                      */
    BENCH_SCN_CLIFF = 1 << 3,  /* cache-cliff size sweep, per-element time             */
    BENCH_SCN_BATCH = 1 << 4,  /* one 4x4 transform applied to M points                */
    BENCH_SCN_BLOCK = 1 << 5,  /* submatrix copy / multiply, block-size sweep          */
    BENCH_SCN_TRI   = 1 << 6,  /* triangular solve + symmetric rank-k update           */
    BENCH_SCN_CONV  = 1 << 7   /* 3x3 convolution (blur) on a generated image          */
} BenchScenario;

static const char *const BENCH_SCENARIO_NAMES[] = {
    "core", "chain", "fixed", "cliff", "batch", "block", "tri", "conv"
};
#define BENCH_NUM_SCENARIOS 8
#define BENCH_SCN_ALL ((1u << BENCH_NUM_SCENARIOS) - 1u)

static inline void bench_options_defaults(BenchOptions *o) {
    memset(o, 0, sizeof(*o));
    o->sizes[0] = 32; o->sizes[1] = 128; o->sizes[2] = 512;
    o->num_sizes      = 3;
    o->num_ops        = 0;     /* all */
    o->warmup         = 3;
    o->iters          = 20;
    o->repeats        = 1;
    o->heavy_divisor  = 4;
    o->seed           = 1u;
    o->csv_enabled    = 0;
    o->csv_path[0]    = '\0';
    o->format         = BENCH_FMT_TABLE;
    o->fixed_enabled  = 1;
    o->scenarios      = BENCH_SCN_ALL;
    o->image_dir[0]   = '\0';
    o->list_ops       = 0;
    o->list_scenarios = 0;
    o->help           = 0;
    o->error          = 0;
}

static inline int bench_op_is_heavy(const char *name) {
    return strcmp(name, "mul") == 0 ||
           strcmp(name, "transpose_mul") == 0 ||
           strcmp(name, "mul_add") == 0;
}

static inline int bench_op_enabled(const BenchOptions *o, const char *name) {
    if (o->num_ops == 0) return 1;
    for (int i = 0; i < o->num_ops; ++i)
        if (strcmp(o->ops[i], name) == 0) return 1;
    return 0;
}

/* Effective iteration count for a given op at a given size. */
static inline int bench_iters_for(const BenchOptions *o, const char *name, size_t n) {
    if (bench_op_is_heavy(name) && n >= 256 && o->heavy_divisor > 1) {
        int it = o->iters / o->heavy_divisor;
        return it < 1 ? 1 : it;
    }
    return o->iters;
}

/* True when `scn` (a BenchScenario value) was selected.  The fixed-size sweep also
 * honours the legacy --no-fixed switch regardless of option order. */
static inline int bench_scenario_enabled(const BenchOptions *o, BenchScenario scn) {
    if (scn == BENCH_SCN_FIXED && !o->fixed_enabled) return 0;
    return (o->scenarios & (unsigned int)scn) != 0u;
}

/*
 * Iteration scaling for runs that reach tiny problem sizes.
 *
 * `work` is a rough operation count for one call (elements touched, or N^3 for
 * GEMM-like kernels).  Cheap calls get their iteration count multiplied so one
 * timed sample spans well over the clock resolution (macOS CLOCK_MONOTONIC is
 * only ~1 us, so 20 calls of a 300 ns kernel are mostly quantisation noise);
 * expensive calls keep the plain count.  Pure functions of their arguments, so
 * the C and C++ drivers always agree.
 */
#define BENCH_SCALE_TARGET_WORK 262144.0
#define BENCH_SCALE_MAX         65536.0
static inline double bench_scale_for_work(double work) {
    double scale = BENCH_SCALE_TARGET_WORK / (work > 1.0 ? work : 1.0);
    if (scale < 1.0) scale = 1.0;
    if (scale > BENCH_SCALE_MAX) scale = BENCH_SCALE_MAX;
    return scale;
}

/* --iters multiplied by bench_scale_for_work(work). */
static inline int bench_iters_scaled(const BenchOptions *o, double work) {
    double it = (double)o->iters * bench_scale_for_work(work);
    if (it > 2.0e9) it = 2.0e9;
    return (int)it;
}

static inline int bench__valid_op_name(const char *name) {
    for (int i = 0; i < BENCH_NUM_ALL_OPS; ++i)
        if (strcmp(BENCH_ALL_OPS[i], name) == 0) return 1;
    return 0;
}

/* Parse "a,b,c" or "start:xMUL:steps" into o->sizes. Returns 0 on success. */
static inline int bench__parse_sizes(BenchOptions *o, const char *spec) {
    o->num_sizes = 0;
    /* geometric progression form: start:xMUL:steps */
    if (strchr(spec, ':') != NULL) {
        long start = 0, mul = 0, steps = 0;
        /* tolerate an 'x'/'X' prefix on the multiplier */
        if (sscanf(spec, "%ld:x%ld:%ld", &start, &mul, &steps) != 3 &&
            sscanf(spec, "%ld:X%ld:%ld", &start, &mul, &steps) != 3 &&
            sscanf(spec, "%ld:%ld:%ld",  &start, &mul, &steps) != 3) {
            return 1;
        }
        if (start <= 0 || mul <= 1 || steps <= 0) return 1;
        long v = start;
        for (long i = 0; i < steps && o->num_sizes < BENCH_MAX_SIZES; ++i) {
            o->sizes[o->num_sizes++] = (size_t)v;
            v *= mul;
        }
        return 0;
    }
    /* comma list */
    const char *p = spec;
    while (*p) {
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p) return 1;
        if (v <= 0) return 1;
        if (o->num_sizes >= BENCH_MAX_SIZES) return 1;
        o->sizes[o->num_sizes++] = (size_t)v;
        p = end;
        while (*p == ',' || *p == ' ') ++p;
    }
    return o->num_sizes == 0 ? 1 : 0;
}

/* Parse "add,mul,..." or "all" into o->ops. Returns 0 on success. */
static inline int bench__parse_ops(BenchOptions *o, const char *spec) {
    o->num_ops = 0;
    if (strcmp(spec, "all") == 0 || strcmp(spec, "ALL") == 0) {
        o->num_ops = 0;
        return 0;
    }
    char buf[256];
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') ++tok;
        if (*tok == '\0') continue;
        if (!bench__valid_op_name(tok)) return 1;
        if (o->num_ops >= BENCH_MAX_OPS) return 1;
        strncpy(o->ops[o->num_ops], tok, BENCH_NAME_LEN - 1);
        o->ops[o->num_ops][BENCH_NAME_LEN - 1] = '\0';
        ++o->num_ops;
    }
    return o->num_ops == 0 ? 1 : 0;
}

/* Parse "chain,fixed,..." or "all" into o->scenarios. Returns 0 on success. */
static inline int bench__parse_scenarios(BenchOptions *o, const char *spec) {
    if (strcmp(spec, "all") == 0 || strcmp(spec, "ALL") == 0) {
        o->scenarios = BENCH_SCN_ALL;
        return 0;
    }
    char buf[256];
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    unsigned int mask = 0u;
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') ++tok;
        if (*tok == '\0') continue;
        int found = 0;
        for (int i = 0; i < BENCH_NUM_SCENARIOS; ++i)
            if (strcmp(BENCH_SCENARIO_NAMES[i], tok) == 0) { mask |= 1u << i; found = 1; break; }
        if (!found) return 1;
    }
    if (mask == 0u) return 1;
    o->scenarios = mask;
    return 0;
}

static inline void bench_print_usage(const char *prog, FILE *out) {
    fprintf(out,
        "Usage: %s [options] [csv_path]\n"
        "\n"
        "Flexible matrix micro-benchmark driver.\n"
        "\n"
        "Options:\n"
        "  --sizes   <list>      Square sizes \"a,b,c\" or progression \"start:xMUL:steps\"\n"
        "                        (default: 32,128,512)\n"
        "  --ops     <list>      Operations to run or \"all\" (default: all)\n"
        "  --warmup  <n>         Warmup iterations (default: 3)\n"
        "  --iters   <n>         Measured iterations (default: 20)\n"
        "  --repeats <n>         Repeats; best average is kept (default: 1)\n"
        "  --heavy-divisor <n>   Iter divisor for O(N^3) ops at N>=256 (default: 4)\n"
        "  --seed    <n>         Base RNG seed (default: 1)\n"
        "  --csv     <path>      CSV output path (\"\"/\"-\" disables)\n"
        "  --format  <fmt>       table | csv | json (default: table)\n"
        "  --scenarios <list>    Scenarios: core,chain,fixed,cliff,batch,block,tri,conv\n"
        "                        or \"all\" (default: all)\n"
        "  --no-fixed            Skip the fixed-size (N=2..16) sweep\n"
        "  --image-dir <path>    Existing directory for conv-scenario PGM images\n"
        "  --list-ops            List available operations and exit\n"
        "  --list-scenarios      List available scenarios and exit\n"
        "  --help                Show this help and exit\n",
        prog);
}

static inline void bench_list_ops(FILE *out) {
    fprintf(out, "Available operations:\n");
    for (int i = 0; i < BENCH_NUM_ALL_OPS; ++i)
        fprintf(out, "  %s\n", BENCH_ALL_OPS[i]);
}

static inline void bench_list_scenarios(FILE *out) {
    fprintf(out, "Available scenarios:\n");
    for (int i = 0; i < BENCH_NUM_SCENARIOS; ++i)
        fprintf(out, "  %s\n", BENCH_SCENARIO_NAMES[i]);
}

/* Returns 0 on success; sets o->error on parse failure. */
static inline int bench_parse_args(BenchOptions *o, int argc, char **argv) {
    bench_options_defaults(o);

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        /* allow "--key=value" by splitting on '=' */
        char keybuf[64];
        const char *value_inline = NULL;
        const char *eq = strchr(arg, '=');
        if (eq && strncmp(arg, "--", 2) == 0) {
            size_t klen = (size_t)(eq - arg);
            if (klen >= sizeof(keybuf)) klen = sizeof(keybuf) - 1;
            memcpy(keybuf, arg, klen);
            keybuf[klen] = '\0';
            value_inline = eq + 1;
            arg = keybuf;
        }

        /* helper to fetch the value: inline (--k=v) or next argv (--k v) */
        #define BENCH_NEXT_VALUE(dst) do {                         \
            if (value_inline) { (dst) = value_inline; }            \
            else if (i + 1 < argc) { (dst) = argv[++i]; }          \
            else { o->error = 1; fprintf(stderr,                   \
                "error: missing value for %s\n", arg); return 1; } \
        } while (0)

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            o->help = 1; return 0;
        } else if (strcmp(arg, "--list-ops") == 0) {
            o->list_ops = 1; return 0;
        } else if (strcmp(arg, "--list-scenarios") == 0) {
            o->list_scenarios = 1; return 0;
        } else if (strcmp(arg, "--no-fixed") == 0) {
            o->fixed_enabled = 0;
        } else if (strcmp(arg, "--scenarios") == 0) {
            const char *v; BENCH_NEXT_VALUE(v);
            if (bench__parse_scenarios(o, v)) {
                o->error = 1; fprintf(stderr, "error: invalid --scenarios '%s'\n", v); return 1;
            }
        } else if (strcmp(arg, "--image-dir") == 0) {
            const char *v; BENCH_NEXT_VALUE(v);
            strncpy(o->image_dir, v, BENCH_PATH_LEN - 1);
            o->image_dir[BENCH_PATH_LEN - 1] = '\0';
        } else if (strcmp(arg, "--sizes") == 0) {
            const char *v; BENCH_NEXT_VALUE(v);
            if (bench__parse_sizes(o, v)) {
                o->error = 1; fprintf(stderr, "error: invalid --sizes '%s'\n", v); return 1;
            }
        } else if (strcmp(arg, "--ops") == 0) {
            const char *v; BENCH_NEXT_VALUE(v);
            if (bench__parse_ops(o, v)) {
                o->error = 1; fprintf(stderr, "error: invalid --ops '%s'\n", v); return 1;
            }
        } else if (strcmp(arg, "--warmup") == 0) {
            const char *v; BENCH_NEXT_VALUE(v); o->warmup = atoi(v);
            if (o->warmup < 0) o->warmup = 0;
        } else if (strcmp(arg, "--iters") == 0) {
            const char *v; BENCH_NEXT_VALUE(v); o->iters = atoi(v);
            if (o->iters < 1) { o->error = 1; fprintf(stderr, "error: --iters must be >= 1\n"); return 1; }
        } else if (strcmp(arg, "--repeats") == 0) {
            const char *v; BENCH_NEXT_VALUE(v); o->repeats = atoi(v);
            if (o->repeats < 1) o->repeats = 1;
        } else if (strcmp(arg, "--heavy-divisor") == 0) {
            const char *v; BENCH_NEXT_VALUE(v); o->heavy_divisor = atoi(v);
            if (o->heavy_divisor < 1) o->heavy_divisor = 1;
        } else if (strcmp(arg, "--seed") == 0) {
            const char *v; BENCH_NEXT_VALUE(v); o->seed = (unsigned int)strtoul(v, NULL, 10);
        } else if (strcmp(arg, "--csv") == 0) {
            const char *v; BENCH_NEXT_VALUE(v);
            if (strcmp(v, "") == 0 || strcmp(v, "-") == 0) {
                o->csv_enabled = 0; o->csv_path[0] = '\0';
            } else {
                strncpy(o->csv_path, v, BENCH_PATH_LEN - 1);
                o->csv_path[BENCH_PATH_LEN - 1] = '\0';
                o->csv_enabled = 1;
            }
        } else if (strcmp(arg, "--format") == 0) {
            const char *v; BENCH_NEXT_VALUE(v);
            if (strcmp(v, "table") == 0)      o->format = BENCH_FMT_TABLE;
            else if (strcmp(v, "csv") == 0)   o->format = BENCH_FMT_CSV;
            else if (strcmp(v, "json") == 0)  o->format = BENCH_FMT_JSON;
            else { o->error = 1; fprintf(stderr, "error: invalid --format '%s'\n", v); return 1; }
        } else if (strncmp(arg, "--", 2) == 0) {
            o->error = 1; fprintf(stderr, "error: unknown option '%s'\n", arg); return 1;
        } else {
            /* positional: treat as CSV path (backward compatible) */
            strncpy(o->csv_path, arg, BENCH_PATH_LEN - 1);
            o->csv_path[BENCH_PATH_LEN - 1] = '\0';
            o->csv_enabled = 1;
        }

        #undef BENCH_NEXT_VALUE
    }
    return 0;
}

#endif /* BENCH_OPTIONS_H */
