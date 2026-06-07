/*
 * Shared benchmark reporter for the matrix_perf_compare suite.
 *
 * Valid in both C11 and C++17.  Streams results to stdout in the format
 * selected on the command line (table / csv / json) and, independently,
 * mirrors every row into a CSV file when one is configured.
 *
 * The CSV schema is intentionally unchanged from earlier versions:
 *     name,rows,cols,iterations,avg_ns
 * so that scripts/summarize_results.py keeps working.
 */
#ifndef BENCH_REPORT_H
#define BENCH_REPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bench_options.h"

typedef struct {
    BenchFormat format;
    FILE       *csv;        /* may be NULL */
    int         json_count; /* number of rows emitted in json mode */
    int         header_done;
} BenchReport;

static inline void bench_report_begin(BenchReport *rp, const BenchOptions *o,
                                      const char *title) {
    rp->format      = o->format;
    rp->json_count  = 0;
    rp->header_done = 0;
    rp->csv         = NULL;

    if (o->csv_enabled && o->csv_path[0]) {
        rp->csv = fopen(o->csv_path, "w");
        if (rp->csv)
            fprintf(rp->csv, "name,rows,cols,iterations,avg_ns\n");
        else
            fprintf(stderr, "warning: could not open %s for writing\n", o->csv_path);
    }

    switch (rp->format) {
        case BENCH_FMT_TABLE:
            printf("=== %s ===\n\n", title);
            printf("%-52s  %11s  %8s  %16s\n", "Benchmark", "Size", "Iters", "Avg Time (ns)");
            printf("%-52s  %11s  %8s  %16s\n",
                   "----------------------------------------------------",
                   "-----------", "--------", "----------------");
            break;
        case BENCH_FMT_CSV:
            printf("name,rows,cols,iterations,avg_ns\n");
            break;
        case BENCH_FMT_JSON:
            printf("{\n  \"benchmark\": \"%s\",\n  \"results\": [\n", title);
            break;
    }
}

static inline void bench_report_row(BenchReport *rp, const char *name,
                                    size_t rows, size_t cols,
                                    int iters, double avg_ns) {
    switch (rp->format) {
        case BENCH_FMT_TABLE:
            printf("%-52s  %5zux%-5zu  iters=%3d  %16.1f\n",
                   name, rows, cols, iters, avg_ns);
            break;
        case BENCH_FMT_CSV:
            printf("%s,%zu,%zu,%d,%.1f\n", name, rows, cols, iters, avg_ns);
            break;
        case BENCH_FMT_JSON:
            if (rp->json_count > 0) printf(",\n");
            printf("    {\"name\": \"%s\", \"rows\": %zu, \"cols\": %zu, "
                   "\"iterations\": %d, \"avg_ns\": %.1f}",
                   name, rows, cols, iters, avg_ns);
            ++rp->json_count;
            break;
    }
    if (rp->csv)
        fprintf(rp->csv, "%s,%zu,%zu,%d,%.1f\n", name, rows, cols, iters, avg_ns);
}

static inline void bench_report_section(BenchReport *rp, const char *label) {
    if (rp->format == BENCH_FMT_TABLE)
        printf("\n--- %s ---\n", label);
}

static inline void bench_report_end(BenchReport *rp, const BenchOptions *o) {
    if (rp->format == BENCH_FMT_JSON)
        printf("\n  ]\n}\n");

    if (rp->csv) {
        fclose(rp->csv);
        rp->csv = NULL;
        if (rp->format == BENCH_FMT_TABLE)
            printf("\nResults written to %s\n", o->csv_path);
    }
}

#endif /* BENCH_REPORT_H */
