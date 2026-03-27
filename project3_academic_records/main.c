/* ================================================================
 * Course Performance and Academic Records Analyzer
 * main.c  —  menu-driven interface and program entry point
 * ================================================================ */

#include "records.h"
#include <errno.h>

/* Forward declarations for menu handlers */
static void menu_add_student(StudentDB *db);
static void menu_display_all(StudentDB *db);
static void menu_display_one(StudentDB *db);
static void menu_update_student(StudentDB *db);
static void menu_delete_student(StudentDB *db);
static void menu_search(StudentDB *db);
static void menu_sort(StudentDB *db);
static void menu_analytics(StudentDB *db);
static void menu_save(StudentDB *db);
static void menu_load(StudentDB *db);
static void print_main_menu(void);
static void print_separator(void);

/* ================================================================
 * Entry point
 * ================================================================ */

int main(void) {
    StudentDB *db = db_create();
    if (!db) {
        fprintf(stderr, "Failed to initialise database. Exiting.\n");
        return EXIT_FAILURE;
    }

    printf("\n");
    printf("  =============================================\n");
    printf("  Course Performance & Academic Records System\n");
    printf("  =============================================\n");
    printf("  Database ready. Use the menu below.\n");

    int choice;
    while (1) {
        print_main_menu();
        choice = input_int("  Select option: ", 0, 10);

        switch (choice) {
            case 1:  menu_add_student(db);    break;
            case 2:  menu_display_all(db);    break;
            case 3:  menu_display_one(db);    break;
            case 4:  menu_update_student(db); break;
            case 5:  menu_delete_student(db); break;
            case 6:  menu_search(db);         break;
            case 7:  menu_sort(db);           break;
            case 8:  menu_analytics(db);      break;
            case 9:  menu_save(db);           break;
            case 10: menu_load(db);           break;
            case 0:
                printf("\n  Saving before exit...\n");
                db_save(db, DATA_FILE);
                db_destroy(db);
                printf("  Goodbye.\n\n");
                return EXIT_SUCCESS;
        }
    }
}

/* ================================================================
 * Menu rendering
 * ================================================================ */

static void print_separator(void) {
    printf("  ---------------------------------------------\n");
}

static void print_main_menu(void) {
    printf("\n");
    print_separator();
    printf("  MAIN MENU\n");
    print_separator();
    printf("  1.  Add new student record\n");
    printf("  2.  Display all records\n");
    printf("  3.  Display single record\n");
    printf("  4.  Update student record\n");
    printf("  5.  Delete student record\n");
    printf("  6.  Search records\n");
    printf("  7.  Sort records\n");
    printf("  8.  Analytics & reports\n");
    printf("  9.  Save to file\n");
    printf("  10. Load from file\n");
    printf("  0.  Save & exit\n");
    print_separator();
}

/* ================================================================
 * Add student
 * ================================================================ */

static void menu_add_student(StudentDB *db) {
    Student *s = student_alloc();
    if (!s) return;

    printf("\n  --- Add New Student ---\n");

    s->id = input_unique_id(db, "  Student ID (1-9999999): ");
    input_string("  Full name       : ", s->name, MAX_NAME_LEN);
    s->age = input_int("  Age (10-120)    : ", 10, 120);
    input_string("  Course/Programme: ", s->course, MAX_COURSE_LEN);

    s->numSubjects = input_int("  Number of subjects (1-10): ", 1, MAX_SUBJECTS);
    printf("  Enter grades (0-100) for each subject:\n");
    for (int i = 0; i < s->numSubjects; i++) {
        char prompt[32];
        snprintf(prompt, sizeof(prompt), "    Subject %d: ", i + 1);
        s->grades[i] = input_float(prompt, 0.0f, 100.0f);
    }

    s->gpa = student_calc_gpa(s);

    if (db_add(db, s) == 0) {
        printf("  Student added successfully (GPA: %.2f).\n", s->gpa);
    } else {
        student_free(s);
        printf("  Failed to add student.\n");
    }
}

/* ================================================================
 * Display
 * ================================================================ */

static void menu_display_all(StudentDB *db) {
    printf("\n  --- All Records ---\n");
    report_print_all(db);
}

static void menu_display_one(StudentDB *db) {
    if (db->count == 0) { printf("  No records.\n"); return; }
    int id = input_int("  Enter student ID: ", 1, 9999999);
    Student *s = db_find_by_id(db, id);
    if (s) {
        student_print(s);
    } else {
        printf("  No student with ID %d found.\n", id);
    }
}

/* ================================================================
 * Update
 * ================================================================ */

static void menu_update_student(StudentDB *db) {
    if (db->count == 0) { printf("  No records to update.\n"); return; }

    int id = input_int("  Enter ID of student to update: ", 1, 9999999);
    Student *s = db_update(db, id);
    if (!s) {
        printf("  Student ID %d not found.\n", id);
        return;
    }

    printf("  Current record:\n");
    student_print(s);
    printf("\n  Enter new values (press Enter to keep current):\n");

    /* Name */
    char buf[MAX_NAME_LEN];
    printf("  Name [%s]: ", s->name);
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (strlen(buf) > 0) strncpy(s->name, buf, MAX_NAME_LEN - 1);
    }

    /* Age */
    printf("  Age  [%d]: ", s->age);
    fflush(stdout);
    char agebuf[16];
    if (fgets(agebuf, sizeof(agebuf), stdin)) {
        int newage;
        if (sscanf(agebuf, "%d", &newage) == 1 && newage >= 10 && newage <= 120)
            s->age = newage;
    }

    /* Course */
    printf("  Course [%s]: ", s->course);
    fflush(stdout);
    char cbuf[MAX_COURSE_LEN];
    if (fgets(cbuf, sizeof(cbuf), stdin)) {
        cbuf[strcspn(cbuf, "\n")] = '\0';
        if (strlen(cbuf) > 0) strncpy(s->course, cbuf, MAX_COURSE_LEN - 1);
    }

    /* Grades */
    int rg = input_int("  Re-enter grades? (1=Yes 0=No): ", 0, 1);
    if (rg) {
        s->numSubjects = input_int("  Number of subjects (1-10): ", 1, MAX_SUBJECTS);
        for (int i = 0; i < s->numSubjects; i++) {
            char prompt[32];
            snprintf(prompt, sizeof(prompt), "    Subject %d: ", i + 1);
            s->grades[i] = input_float(prompt, 0.0f, 100.0f);
        }
    }

    s->gpa = student_calc_gpa(s);
    printf("  Record updated (new GPA: %.2f).\n", s->gpa);
}

/* ================================================================
 * Delete
 * ================================================================ */

static void menu_delete_student(StudentDB *db) {
    if (db->count == 0) { printf("  No records to delete.\n"); return; }

    int id = input_int("  Enter ID of student to delete: ", 1, 9999999);
    Student *s = db_find_by_id(db, id);
    if (!s) { printf("  Student ID %d not found.\n", id); return; }

    printf("  About to delete: [%d] %s — are you sure? (1=Yes 0=No): ", id, s->name);
    fflush(stdout);
    int confirm = input_int("", 0, 1);
    if (confirm) {
        if (db_delete(db, id) == 0)
            printf("  Deleted.\n");
        else
            printf("  Deletion failed.\n");
    } else {
        printf("  Cancelled.\n");
    }
}

/* ================================================================
 * Search
 * ================================================================ */

static void menu_search(StudentDB *db) {
    if (db->count == 0) { printf("  No records.\n"); return; }

    printf("\n  Search by:\n");
    printf("    1. Student ID\n");
    printf("    2. Name\n");
    int opt = input_int("  Option: ", 1, 2);

    if (opt == 1) {
        int id = input_int("  Enter ID: ", 1, 9999999);
        Student *s = db_find_by_id(db, id);
        if (s) student_print(s);
        else   printf("  Not found.\n");
    } else {
        char name[MAX_NAME_LEN];
        input_string("  Enter name (partial match OK): ", name, MAX_NAME_LEN);
        /* Print all matches */
        int found = 0;
        for (int i = 0; i < db->count; i++) {
            /* reuse stristr logic via find_by_name for first; iterate for all */
            char lower_name[MAX_NAME_LEN], lower_query[MAX_NAME_LEN];
            strncpy(lower_name,  db->data[i]->name, MAX_NAME_LEN - 1);
            strncpy(lower_query, name,               MAX_NAME_LEN - 1);
            for (int k = 0; lower_name[k]; k++)  lower_name[k]  = tolower((unsigned char)lower_name[k]);
            for (int k = 0; lower_query[k]; k++) lower_query[k] = tolower((unsigned char)lower_query[k]);
            if (strstr(lower_name, lower_query)) {
                student_print_short(db->data[i]);
                found++;
            }
        }
        if (!found) printf("  No match for '%s'.\n", name);
        else        printf("  Found %d match(es).\n", found);
    }
}

/* ================================================================
 * Sort
 * ================================================================ */

static void menu_sort(StudentDB *db) {
    if (db->count == 0) { printf("  No records.\n"); return; }

    printf("\n  Sort by:\n");
    printf("    1. GPA  (descending)\n");
    printf("    2. Name (A-Z)\n");
    printf("    3. ID   (ascending)\n");
    int opt = input_int("  Option: ", 1, 3);

    switch (opt) {
        case 1: db_sort_by_gpa_desc(db); printf("  Sorted by GPA (highest first).\n");  break;
        case 2: db_sort_by_name_asc(db); printf("  Sorted by name (A-Z).\n");           break;
        case 3: db_sort_by_id_asc(db);   printf("  Sorted by ID (ascending).\n");       break;
    }
    report_print_all(db);
}

/* ================================================================
 * Analytics
 * ================================================================ */

static void menu_analytics(StudentDB *db) {
    if (db->count == 0) { printf("  No records to analyse.\n"); return; }

    printf("\n  Analytics Menu:\n");
    printf("    1. Class statistics (avg / highest / lowest)\n");
    printf("    2. Median GPA\n");
    printf("    3. Top N students\n");
    printf("    4. Best student per course\n");
    printf("    5. All of the above\n");
    int opt = input_int("  Option: ", 1, 5);

    if (opt == 1 || opt == 5) report_class_stats(db);
    if (opt == 2 || opt == 5) report_median_gpa(db);
    if (opt == 3 || opt == 5) {
        int n = input_int("  How many top students to show? ", 1, db->count);
        report_top_n(db, n);
    }
    if (opt == 4 || opt == 5) report_best_per_course(db);
}

/* ================================================================
 * File I/O menu handlers
 * ================================================================ */

static void menu_save(StudentDB *db) {
    char filename[128];
    printf("  Save filename [%s]: ", DATA_FILE);
    fflush(stdout);
    if (fgets(filename, sizeof(filename), stdin)) {
        filename[strcspn(filename, "\n")] = '\0';
    }
    if (strlen(filename) == 0) strncpy(filename, DATA_FILE, sizeof(filename) - 1);
    db_save(db, filename);
}

static void menu_load(StudentDB *db) {
    char filename[128];
    printf("  Load filename [%s]: ", DATA_FILE);
    fflush(stdout);
    if (fgets(filename, sizeof(filename), stdin)) {
        filename[strcspn(filename, "\n")] = '\0';
    }
    if (strlen(filename) == 0) strncpy(filename, DATA_FILE, sizeof(filename) - 1);
    db_load(db, filename);
}
