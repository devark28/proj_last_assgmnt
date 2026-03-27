/*
 * Data Analysis Toolkit — main.c
 * =================================
 * Entry point and menu dispatcher.
 *
 * Dispatcher design:
 *   A static array of MenuItem structs maps each menu option to
 *   an OperationFunc pointer. The dispatcher calls the function
 *   through the pointer — no if/else chain needed.
 *
 * Memory management:
 *   A single Dataset is kept alive for the full program session.
 *   It is expanded with realloc as values are added and freed on exit.
 */

#include "toolkit.h"

/* ================================================================
 * Forward declarations for handlers that need extra interaction
 * (they don't fit the simple OperationFunc signature)
 * ================================================================ */
static void handler_create_manual(Dataset *ds);
static void handler_load_file(Dataset *ds);
static void handler_save_file(Dataset *ds);
static void handler_delete_reset(Dataset *ds);

/* ================================================================
 * Operation dispatch table
 *
 * Each entry: { "display label", function_pointer }
 * Indices 0-8 map to the operation submenu options.
 * ================================================================ */
static const MenuItem operations[] = {
    { "Compute sum",                  op_compute_sum      },
    { "Compute average",              op_compute_average  },
    { "Find minimum and maximum",     op_find_min_max     },
    { "Sort ascending",               op_sort_asc         },
    { "Sort descending",              op_sort_desc        },
    { "Search for value",             op_search           },
    { "Filter dataset (callback)",    op_filter           },
    { "Transform dataset (callback)", op_transform        },
    { "Full statistics summary",      op_full_statistics  },
};
#define NUM_OPERATIONS  (int)(sizeof(operations) / sizeof(operations[0]))

/* ================================================================
 * Dispatcher — selects and calls an operation by index
 * ================================================================ */
static void dispatch_operation(Dataset *ds) {
    if (!ds || ds->count == 0) {
        printf("  Dataset is empty. Load or create a dataset first.\n");
        return;
    }

    printf("\n  --- Operations ---\n");
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        printf("  %2d. %s\n", i + 1, operations[i].label);
    }
    printf("   0. Cancel\n");

    int choice = input_int_range("  Select operation: ", 0, NUM_OPERATIONS);
    if (choice == 0) return;

    /* Retrieve function pointer and verify it is not NULL */
    OperationFunc fn = operations[choice - 1].func;
    if (!fn) {
        printf("  Error: NULL function pointer at index %d.\n", choice - 1);
        return;
    }

    fn(ds);   /* dispatch */
}

/* ================================================================
 * Handlers for non-operation menu items
 * ================================================================ */

static void handler_create_manual(Dataset *ds) {
    printf("\n  Enter values one by one (type 'done' or non-number to finish):\n");
    char buf[64];
    int added = 0;
    while (1) {
        printf("  Value %d: ", ds->count + 1);
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        double val;
        if (sscanf(buf, "%lf", &val) != 1) break;
        if (ds_append(ds, val) == 0) added++;
    }
    printf("  Added %d value(s). Dataset now has %d element(s).\n", added, ds->count);
}

static void handler_load_file(Dataset *ds) {
    char filename[MAX_FILENAME];
    input_string("  Filename to load: ", filename, MAX_FILENAME);
    ds_load_file(ds, filename);
}

static void handler_save_file(Dataset *ds) {
    if (ds->count == 0) { printf("  Nothing to save.\n"); return; }
    char filename[MAX_FILENAME];
    input_string("  Save to filename: ", filename, MAX_FILENAME);
    ds_save_file(ds, filename);
}

static void handler_delete_reset(Dataset *ds) {
    if (ds->count == 0) { printf("  Dataset is already empty.\n"); return; }
    printf("  Reset dataset? All %d values will be lost. (1=Yes 0=No): ", ds->count);
    int confirm = input_int_range("", 0, 1);
    if (confirm) {
        ds_reset(ds);
        printf("  Dataset cleared.\n");
    } else {
        printf("  Cancelled.\n");
    }
}

/* ================================================================
 * Main menu
 * ================================================================ */

static void print_menu(const Dataset *ds) {
    printf("\n");
    printf("  +======================================+\n");
    printf("  |     Data Analysis Toolkit  v1.0      |\n");
    printf("  +======================================+\n");
    printf("  |  Dataset: %5d element(s)            |\n", ds ? ds->count : 0);
    printf("  +--------------------------------------+\n");
    printf("  |  1. Create dataset (manual input)    |\n");
    printf("  |  2. Load dataset from file           |\n");
    printf("  |  3. Display dataset                  |\n");
    printf("  |  4. Select processing operation      |\n");
    printf("  |  5. Save dataset / results to file   |\n");
    printf("  |  6. Reset / clear dataset            |\n");
    printf("  |  0. Exit                             |\n");
    printf("  +======================================+\n");
}

int main(void) {
    Dataset *ds = ds_create();
    if (!ds) {
        fprintf(stderr, "Failed to allocate dataset. Exiting.\n");
        return EXIT_FAILURE;
    }

    int running = 1;
    while (running) {
        print_menu(ds);
        int choice = input_int_range("  Choice: ", 0, 6);

        switch (choice) {
            case 1: handler_create_manual(ds); break;
            case 2: handler_load_file(ds);     break;
            case 3: ds_print(ds);              break;
            case 4: dispatch_operation(ds);    break;
            case 5: handler_save_file(ds);     break;
            case 6: handler_delete_reset(ds);  break;
            case 0:
                running = 0;
                break;
        }
    }

    ds_destroy(ds);
    printf("  All memory freed. Goodbye.\n");
    return EXIT_SUCCESS;
}
