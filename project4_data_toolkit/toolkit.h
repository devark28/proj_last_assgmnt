#ifndef TOOLKIT_H
#define TOOLKIT_H

/*
 * Data Analysis Toolkit — toolkit.h
 * ===================================
 * Defines the Dataset structure, callback typedefs,
 * and all function prototypes used by the toolkit.
 *
 * Design notes:
 *   - Operations are dispatched through a MenuItem table of
 *     function pointers — no long if/else chains in main.
 *   - Callback functions (filter, transform, compare) are
 *     passed as parameters to processing routines so the
 *     caller chooses behaviour at runtime.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define INITIAL_CAPACITY   16
#define MAX_FILENAME       256

/* ---------------------------------------------------------------
 * Dynamic dataset
 * --------------------------------------------------------------- */
typedef struct {
    double *data;
    int     count;
    int     capacity;
} Dataset;

/* ---------------------------------------------------------------
 * Callback function pointer types
 * --------------------------------------------------------------- */
typedef int    (*FilterFunc)   (double value, double threshold);
typedef double (*TransformFunc)(double value, double param);
typedef int    (*CompareFunc)  (const void *a, const void *b);

/* ---------------------------------------------------------------
 * Operation function pointer — takes a dataset, returns nothing.
 * The dispatcher table maps menu indices to these.
 * --------------------------------------------------------------- */
typedef void (*OperationFunc)(Dataset *ds);

/* Menu item: label + operation function pointer */
typedef struct {
    const char  *label;
    OperationFunc func;
} MenuItem;

/* ---------------------------------------------------------------
 * Dataset lifecycle
 * --------------------------------------------------------------- */
Dataset *ds_create(void);
void     ds_destroy(Dataset *ds);
int      ds_append(Dataset *ds, double value);
int      ds_expand(Dataset *ds);    /* double capacity */
void     ds_reset(Dataset *ds);
void     ds_print(const Dataset *ds);

/* ---------------------------------------------------------------
 * Built-in operations  (compatible with OperationFunc signature)
 * --------------------------------------------------------------- */
void op_compute_sum(Dataset *ds);
void op_compute_average(Dataset *ds);
void op_find_min_max(Dataset *ds);
void op_sort_asc(Dataset *ds);
void op_sort_desc(Dataset *ds);
void op_search(Dataset *ds);
void op_filter(Dataset *ds);
void op_transform(Dataset *ds);
void op_full_statistics(Dataset *ds);

/* ---------------------------------------------------------------
 * File I/O
 * --------------------------------------------------------------- */
int ds_load_file(Dataset *ds, const char *filename);
int ds_save_file(const Dataset *ds, const char *filename);

/* ---------------------------------------------------------------
 * Callback implementations
 * --------------------------------------------------------------- */
int    cmp_asc(const void *a, const void *b);
int    cmp_desc(const void *a, const void *b);

int    filter_above(double value, double threshold);
int    filter_below(double value, double threshold);
int    filter_equal(double value, double threshold);

double transform_scale(double value, double factor);
double transform_offset(double value, double offset);
double transform_square(double value, double unused);
double transform_sqrt(double value, double unused);
double transform_abs(double value, double unused);

/* ---------------------------------------------------------------
 * Input helpers
 * --------------------------------------------------------------- */
double input_double(const char *prompt);
int    input_int_range(const char *prompt, int min, int max);
void   input_string(const char *prompt, char *buf, int maxlen);

#endif /* TOOLKIT_H */
