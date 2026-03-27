/*
 * Data Analysis Toolkit — toolkit.c
 * ====================================
 * Implementation of all dataset operations, callbacks,
 * file I/O, and input helpers.
 */

#include "toolkit.h"

/* ================================================================
 * Dataset lifecycle
 * ================================================================ */

Dataset *ds_create(void) {
    Dataset *ds = malloc(sizeof(Dataset));
    if (!ds) { perror("malloc Dataset"); return NULL; }

    ds->data = malloc(INITIAL_CAPACITY * sizeof(double));
    if (!ds->data) {
        perror("malloc ds->data");
        free(ds);
        return NULL;
    }
    ds->count    = 0;
    ds->capacity = INITIAL_CAPACITY;
    return ds;
}

void ds_destroy(Dataset *ds) {
    if (!ds) return;
    free(ds->data);
    free(ds);
}

/* Double the internal buffer using realloc */
int ds_expand(Dataset *ds) {
    int newcap = ds->capacity * 2;
    double *tmp = realloc(ds->data, newcap * sizeof(double));
    if (!tmp) {
        perror("realloc ds->data");
        return -1;
    }
    ds->data     = tmp;
    ds->capacity = newcap;
    return 0;
}

int ds_append(Dataset *ds, double value) {
    if (!ds) return -1;
    if (ds->count >= ds->capacity) {
        if (ds_expand(ds) != 0) return -1;
    }
    ds->data[ds->count++] = value;
    return 0;
}

/* Reset count to 0 (keeps allocated memory) */
void ds_reset(Dataset *ds) {
    if (ds) ds->count = 0;
}

void ds_print(const Dataset *ds) {
    if (!ds || ds->count == 0) {
        printf("  (dataset is empty)\n");
        return;
    }
    printf("  Dataset [%d element(s)]:\n  ", ds->count);
    for (int i = 0; i < ds->count; i++) {
        printf("%-10.4g", ds->data[i]);
        if ((i + 1) % 8 == 0 && i < ds->count - 1) printf("\n  ");
    }
    printf("\n");
}

/* ================================================================
 * Comparison callbacks for qsort
 * ================================================================ */

int cmp_asc(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

int cmp_desc(const void *a, const void *b) {
    return cmp_asc(b, a);
}

/* ================================================================
 * Filter callbacks
 * ================================================================ */

int filter_above(double value, double threshold) { return value >  threshold; }
int filter_below(double value, double threshold) { return value <  threshold; }
int filter_equal(double value, double threshold) { return value == threshold; }

/* ================================================================
 * Transform callbacks
 * ================================================================ */

double transform_scale(double value, double factor)  { return value * factor; }
double transform_offset(double value, double offset) { return value + offset; }
double transform_square(double value, double unused) { (void)unused; return value * value; }
double transform_abs(double value, double unused)    { (void)unused; return fabs(value); }
double transform_sqrt(double value, double unused) {
    (void)unused;
    if (value < 0) {
        fprintf(stderr, "  Warning: sqrt of negative value %.4g — using 0.\n", value);
        return 0.0;
    }
    return sqrt(value);
}

/* ================================================================
 * Built-in operations
 * ================================================================ */

void op_compute_sum(Dataset *ds) {
    if (!ds || ds->count == 0) { printf("  Dataset is empty.\n"); return; }
    double sum = 0.0;
    for (int i = 0; i < ds->count; i++) sum += ds->data[i];
    printf("  Sum = %.6g\n", sum);
}

void op_compute_average(Dataset *ds) {
    if (!ds || ds->count == 0) { printf("  Dataset is empty.\n"); return; }
    double sum = 0.0;
    for (int i = 0; i < ds->count; i++) sum += ds->data[i];
    printf("  Average = %.6g  (%d elements)\n", sum / ds->count, ds->count);
}

void op_find_min_max(Dataset *ds) {
    if (!ds || ds->count == 0) { printf("  Dataset is empty.\n"); return; }
    double mn = ds->data[0], mx = ds->data[0];
    int mn_idx = 0, mx_idx = 0;
    for (int i = 1; i < ds->count; i++) {
        if (ds->data[i] < mn) { mn = ds->data[i]; mn_idx = i; }
        if (ds->data[i] > mx) { mx = ds->data[i]; mx_idx = i; }
    }
    printf("  Minimum = %.6g  (index %d)\n", mn, mn_idx);
    printf("  Maximum = %.6g  (index %d)\n", mx, mx_idx);
}

void op_sort_asc(Dataset *ds) {
    if (!ds || ds->count == 0) { printf("  Dataset is empty.\n"); return; }
    qsort(ds->data, ds->count, sizeof(double), cmp_asc);
    printf("  Sorted ascending.\n");
    ds_print(ds);
}

void op_sort_desc(Dataset *ds) {
    if (!ds || ds->count == 0) { printf("  Dataset is empty.\n"); return; }
    qsort(ds->data, ds->count, sizeof(double), cmp_desc);
    printf("  Sorted descending.\n");
    ds_print(ds);
}

void op_search(Dataset *ds) {
    if (!ds || ds->count == 0) { printf("  Dataset is empty.\n"); return; }
    double target = input_double("  Enter value to search for: ");
    int found = 0;
    for (int i = 0; i < ds->count; i++) {
        if (ds->data[i] == target) {
            printf("  Found %.6g at index %d\n", target, i);
            found++;
        }
    }
    if (!found) printf("  Value %.6g not found in dataset.\n", target);
    else        printf("  Total occurrences: %d\n", found);
}

/*
 * op_filter — uses a FilterFunc callback chosen at runtime.
 * Creates a new dataset with only matching values, then prints.
 */
void op_filter(Dataset *ds) {
    if (!ds || ds->count == 0) { printf("  Dataset is empty.\n"); return; }

    printf("  Filter condition:\n");
    printf("    1. Values ABOVE threshold\n");
    printf("    2. Values BELOW threshold\n");
    printf("    3. Values EQUAL to threshold\n");
    int opt = input_int_range("  Choose: ", 1, 3);

    FilterFunc cb = NULL;
    const char *label = NULL;
    switch (opt) {
        case 1: cb = filter_above; label = "above"; break;
        case 2: cb = filter_below; label = "below"; break;
        case 3: cb = filter_equal; label = "equal"; break;
    }

    if (!cb) { printf("  NULL callback — aborting filter.\n"); return; }

    double threshold = input_double("  Enter threshold: ");

    Dataset *result = ds_create();
    if (!result) return;

    for (int i = 0; i < ds->count; i++) {
        if (cb(ds->data[i], threshold)) {
            ds_append(result, ds->data[i]);
        }
    }

    printf("  Filtered (%s %.6g): %d / %d values match:\n",
           label, threshold, result->count, ds->count);
    ds_print(result);
    ds_destroy(result);
}

/*
 * op_transform — applies a TransformFunc callback to every element
 * (modifies dataset in-place).
 */
void op_transform(Dataset *ds) {
    if (!ds || ds->count == 0) { printf("  Dataset is empty.\n"); return; }

    printf("  Transformation:\n");
    printf("    1. Scale   (multiply by factor)\n");
    printf("    2. Offset  (add constant)\n");
    printf("    3. Square  (x²)\n");
    printf("    4. Sqrt    (√x)\n");
    printf("    5. Absolute value\n");
    int opt = input_int_range("  Choose: ", 1, 5);

    TransformFunc cb  = NULL;
    double        param = 0.0;
    const char   *desc = "";

    switch (opt) {
        case 1:
            param = input_double("  Scale factor: ");
            cb = transform_scale;
            desc = "scaled";
            break;
        case 2:
            param = input_double("  Offset value: ");
            cb = transform_offset;
            desc = "offset applied";
            break;
        case 3: cb = transform_square; desc = "squared";        break;
        case 4: cb = transform_sqrt;   desc = "sqrt applied";   break;
        case 5: cb = transform_abs;    desc = "abs applied";    break;
    }

    if (!cb) { printf("  NULL function pointer — aborting.\n"); return; }

    for (int i = 0; i < ds->count; i++) {
        ds->data[i] = cb(ds->data[i], param);
    }

    printf("  Transformation complete (%s).\n", desc);
    ds_print(ds);
}

/* Compute and display a full statistical summary */
void op_full_statistics(Dataset *ds) {
    if (!ds || ds->count == 0) { printf("  Dataset is empty.\n"); return; }

    double sum = 0.0, mn = ds->data[0], mx = ds->data[0];
    for (int i = 0; i < ds->count; i++) {
        sum += ds->data[i];
        if (ds->data[i] < mn) mn = ds->data[i];
        if (ds->data[i] > mx) mx = ds->data[i];
    }
    double mean = sum / ds->count;

    /* Variance and standard deviation */
    double var = 0.0;
    for (int i = 0; i < ds->count; i++) {
        double diff = ds->data[i] - mean;
        var += diff * diff;
    }
    var /= ds->count;

    /* Median — sort a copy */
    double *tmp = malloc(ds->count * sizeof(double));
    if (tmp) {
        memcpy(tmp, ds->data, ds->count * sizeof(double));
        qsort(tmp, ds->count, sizeof(double), cmp_asc);
        double median;
        if (ds->count % 2 == 0)
            median = (tmp[ds->count / 2 - 1] + tmp[ds->count / 2]) / 2.0;
        else
            median = tmp[ds->count / 2];

        printf("\n  === Full Statistics ===\n");
        printf("  Count   : %d\n",   ds->count);
        printf("  Sum     : %.6g\n", sum);
        printf("  Mean    : %.6g\n", mean);
        printf("  Median  : %.6g\n", median);
        printf("  Min     : %.6g\n", mn);
        printf("  Max     : %.6g\n", mx);
        printf("  Range   : %.6g\n", mx - mn);
        printf("  Variance: %.6g\n", var);
        printf("  Std Dev : %.6g\n", sqrt(var));

        free(tmp);
    } else {
        /* fallback if malloc failed */
        printf("  Count: %d  Sum: %.6g  Mean: %.6g  Min: %.6g  Max: %.6g\n",
               ds->count, sum, mean, mn, mx);
    }
}

/* ================================================================
 * File I/O
 * ================================================================ */

int ds_load_file(Dataset *ds, const char *filename) {
    if (!ds || !filename) return -1;
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "  Cannot open '%s': %s\n", filename, strerror(errno));
        return -1;
    }

    double val;
    int loaded = 0;
    while (fscanf(fp, "%lf", &val) == 1) {
        if (ds_append(ds, val) != 0) break;
        loaded++;
    }

    fclose(fp);
    printf("  Loaded %d value(s) from '%s'.\n", loaded, filename);
    return loaded;
}

int ds_save_file(const Dataset *ds, const char *filename) {
    if (!ds || !filename) return -1;
    if (ds->count == 0) { printf("  Dataset is empty — nothing saved.\n"); return 0; }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "  Cannot open '%s' for writing: %s\n", filename, strerror(errno));
        return -1;
    }

    for (int i = 0; i < ds->count; i++) {
        fprintf(fp, "%.10g\n", ds->data[i]);
    }

    fclose(fp);
    printf("  Saved %d value(s) to '%s'.\n", ds->count, filename);
    return ds->count;
}

/* ================================================================
 * Input helpers
 * ================================================================ */

double input_double(const char *prompt) {
    double val;
    char buf[64];
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) && sscanf(buf, "%lf", &val) == 1)
            return val;
        printf("  Invalid input. Please enter a number.\n");
    }
}

int input_int_range(const char *prompt, int min, int max) {
    int val;
    char buf[32];
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) && sscanf(buf, "%d", &val) == 1
            && val >= min && val <= max)
            return val;
        printf("  Please enter an integer between %d and %d.\n", min, max);
    }
}

void input_string(const char *prompt, char *buf, int maxlen) {
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(buf, maxlen, stdin)) {
            buf[strcspn(buf, "\n")] = '\0';
            if (strlen(buf) > 0) return;
        }
        printf("  Input cannot be empty.\n");
    }
}
