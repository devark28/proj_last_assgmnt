#ifndef RECORDS_H
#define RECORDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <strings.h>

/* ================================================================
 * Course Performance and Academic Records Analyzer
 * records.h  —  types, constants, and function declarations
 * ================================================================ */

/* ---------------------------------------------------------------
 * Limits and constants
 * --------------------------------------------------------------- */
#define MAX_SUBJECTS       10
#define MAX_NAME_LEN       64
#define MAX_COURSE_LEN     64
#define INITIAL_CAPACITY   8
#define DATA_FILE          "students.dat"
#define FILE_MAGIC         0xACAD0001u   /* used to verify file integrity */

/* ---------------------------------------------------------------
 * Student record
 * --------------------------------------------------------------- */
typedef struct {
    int   id;
    char  name[MAX_NAME_LEN];
    int   age;
    char  course[MAX_COURSE_LEN];
    int   numSubjects;
    float grades[MAX_SUBJECTS];
    float gpa;                   /* computed from grades */
} Student;

/* ---------------------------------------------------------------
 * Dynamic student database
 * --------------------------------------------------------------- */
typedef struct {
    Student **data;     /* array of pointers to heap-allocated Students */
    int       count;    /* current number of records */
    int       capacity; /* allocated slot count */
} StudentDB;

/* ---------------------------------------------------------------
 * DB lifecycle
 * --------------------------------------------------------------- */
StudentDB *db_create(void);
void       db_destroy(StudentDB *db);

/* ---------------------------------------------------------------
 * CRUD operations
 * --------------------------------------------------------------- */
int      db_add(StudentDB *db, Student *s);
int      db_delete(StudentDB *db, int id);
Student *db_update(StudentDB *db, int id);   /* returns pointer for in-place edit */

/* ---------------------------------------------------------------
 * Search
 * --------------------------------------------------------------- */
Student *db_find_by_id(const StudentDB *db, int id);
Student *db_find_by_name(const StudentDB *db, const char *name);   /* case-insensitive */

/* ---------------------------------------------------------------
 * Sorting  (all modify db->data in-place)
 * --------------------------------------------------------------- */
void db_sort_by_gpa_desc(StudentDB *db);
void db_sort_by_name_asc(StudentDB *db);
void db_sort_by_id_asc(StudentDB *db);

/* ---------------------------------------------------------------
 * File I/O
 * --------------------------------------------------------------- */
int db_save(const StudentDB *db, const char *filename);
int db_load(StudentDB *db, const char *filename);

/* ---------------------------------------------------------------
 * Analytics / reporting
 * --------------------------------------------------------------- */
void report_print_all(const StudentDB *db);
void report_class_stats(const StudentDB *db);
void report_top_n(const StudentDB *db, int n);
void report_best_per_course(const StudentDB *db);
void report_median_gpa(StudentDB *db);   /* sorts by GPA internally */

/* ---------------------------------------------------------------
 * Student helpers
 * --------------------------------------------------------------- */
Student *student_alloc(void);
void     student_free(Student *s);
float    student_calc_gpa(Student *s);
void     student_print(const Student *s);
void     student_print_short(const Student *s);

/* ---------------------------------------------------------------
 * Input helpers
 * --------------------------------------------------------------- */
int   input_int(const char *prompt, int min, int max);
float input_float(const char *prompt, float min, float max);
void  input_string(const char *prompt, char *buf, int maxlen);
int   input_unique_id(const StudentDB *db, const char *prompt);

#endif /* RECORDS_H */
