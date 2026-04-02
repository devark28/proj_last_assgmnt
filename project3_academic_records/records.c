#include "records.h"
#include <errno.h>

// trim leading/trailing whitespace in-place
static void trim(char *s) {
    if (!s) return;
    char *end;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
}

// case-insensitive partial string search
static char *stristr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            const char *h = haystack, *n = needle;
            while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
                h++; n++;
            }
            if (!*n) return (char *)haystack;
        }
    }
    return NULL;
}

StudentDB *db_create(void) {
    StudentDB *db = malloc(sizeof(StudentDB));
    if (!db) { perror("malloc StudentDB"); return NULL; }
    db->data = malloc(INITIAL_CAPACITY * sizeof(Student *));
    if (!db->data) { perror("malloc db->data"); free(db); return NULL; }
    db->count    = 0;
    db->capacity = INITIAL_CAPACITY;
    return db;
}

void db_destroy(StudentDB *db) {
    if (!db) return;
    for (int i = 0; i < db->count; i++) student_free(db->data[i]);
    free(db->data);
    free(db);
}

int db_add(StudentDB *db, Student *s) {
    if (!db || !s) return -1;
    if (db_find_by_id(db, s->id)) {
        fprintf(stderr, "Error: student ID %d already exists.\n", s->id);
        return -1;
    }
    if (db->count >= db->capacity) {
        int newcap = db->capacity * 2;
        Student **tmp = realloc(db->data, newcap * sizeof(Student *));
        if (!tmp) { perror("realloc db->data"); return -1; }
        db->data     = tmp;
        db->capacity = newcap;
    }
    db->data[db->count++] = s;
    return 0;
}

int db_delete(StudentDB *db, int id) {
    if (!db) return -1;
    for (int i = 0; i < db->count; i++) {
        if (db->data[i]->id == id) {
            student_free(db->data[i]);
            for (int j = i; j < db->count - 1; j++) db->data[j] = db->data[j + 1];
            db->count--;
            return 0;
        }
    }
    return -1;
}

// db_update returns a live pointer so the caller edits the record in-place
Student *db_update(StudentDB *db, int id) {
    return db_find_by_id(db, id);
}

Student *db_find_by_id(const StudentDB *db, int id) {
    if (!db) return NULL;
    for (int i = 0; i < db->count; i++)
        if (db->data[i]->id == id) return db->data[i];
    return NULL;
}

// linear search with case-insensitive partial match
Student *db_find_by_name(const StudentDB *db, const char *name) {
    if (!db || !name) return NULL;
    for (int i = 0; i < db->count; i++)
        if (stristr(db->data[i]->name, name)) return db->data[i];
    return NULL;
}

// bubble sort descending by GPA (manual as required)
void db_sort_by_gpa_desc(StudentDB *db) {
    if (!db || db->count < 2) return;
    for (int i = 0; i < db->count - 1; i++) {
        for (int j = 0; j < db->count - 1 - i; j++) {
            if (db->data[j]->gpa < db->data[j + 1]->gpa) {
                Student *tmp    = db->data[j];
                db->data[j]     = db->data[j + 1];
                db->data[j + 1] = tmp;
            }
        }
    }
}

// selection sort ascending by name
void db_sort_by_name_asc(StudentDB *db) {
    if (!db || db->count < 2) return;
    for (int i = 0; i < db->count - 1; i++) {
        int minIdx = i;
        for (int j = i + 1; j < db->count; j++)
            if (strcasecmp(db->data[j]->name, db->data[minIdx]->name) < 0) minIdx = j;
        if (minIdx != i) {
            Student *tmp     = db->data[i];
            db->data[i]      = db->data[minIdx];
            db->data[minIdx] = tmp;
        }
    }
}

// insertion sort ascending by ID
void db_sort_by_id_asc(StudentDB *db) {
    if (!db || db->count < 2) return;
    for (int i = 1; i < db->count; i++) {
        Student *key = db->data[i];
        int j = i - 1;
        while (j >= 0 && db->data[j]->id > key->id) { db->data[j + 1] = db->data[j]; j--; }
        db->data[j + 1] = key;
    }
}

int db_save(const StudentDB *db, const char *filename) {
    if (!db || !filename) return -1;
    FILE *fp = fopen(filename, "wb");
    if (!fp) { perror("fopen (save)"); return -1; }
    unsigned int magic = FILE_MAGIC;
    if (fwrite(&magic,    sizeof(magic),    1, fp) != 1) goto write_err;
    if (fwrite(&db->count, sizeof(db->count), 1, fp) != 1) goto write_err;
    for (int i = 0; i < db->count; i++)
        if (fwrite(db->data[i], sizeof(Student), 1, fp) != 1) goto write_err;
    fclose(fp);
    printf("Saved %d record(s) to '%s'.\n", db->count, filename);
    return 0;
write_err:
    perror("fwrite");
    fclose(fp);
    return -1;
}

int db_load(StudentDB *db, const char *filename) {
    if (!db || !filename) return -1;
    FILE *fp = fopen(filename, "rb");
    if (!fp) { fprintf(stderr, "Cannot open '%s': %s\n", filename, strerror(errno)); return -1; }

    unsigned int magic = 0;
    int count = 0;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != FILE_MAGIC) {
        fprintf(stderr, "Error: '%s' is not a valid records file.\n", filename);
        fclose(fp); return -1;
    }
    if (fread(&count, sizeof(count), 1, fp) != 1 || count < 0) {
        fprintf(stderr, "Error: corrupt record count.\n");
        fclose(fp); return -1;
    }
    for (int i = 0; i < count; i++) {
        Student *s = student_alloc();
        if (!s) { fclose(fp); return -1; }
        if (fread(s, sizeof(Student), 1, fp) != 1) {
            fprintf(stderr, "Error reading record %d.\n", i + 1);
            student_free(s); fclose(fp); return -1;
        }
        s->gpa = student_calc_gpa(s);  // recompute in case of file inconsistency
        if (db_add(db, s) != 0) student_free(s);
    }
    fclose(fp);
    printf("Loaded %d record(s) from '%s'.\n", count, filename);
    return 0;
}

void report_print_all(const StudentDB *db) {
    if (!db || db->count == 0) { printf("  No records.\n"); return; }
    printf("\n%-6s  %-22s  %-4s  %-20s  %5s  %s\n",
           "ID", "Name", "Age", "Course", "Subj", "GPA");
    printf("%-6s  %-22s  %-4s  %-20s  %5s  %s\n",
           "------", "----------------------", "----",
           "--------------------", "-----", "-----");
    for (int i = 0; i < db->count; i++) {
        const Student *s = db->data[i];
        printf("%-6d  %-22s  %-4d  %-20s  %5d  %.2f\n",
               s->id, s->name, s->age, s->course, s->numSubjects, s->gpa);
    }
    printf("  Total: %d record(s)\n", db->count);
}

void report_class_stats(const StudentDB *db) {
    if (!db || db->count == 0) { printf("  No records.\n"); return; }
    float sum = 0.0f, highest = db->data[0]->gpa, lowest = db->data[0]->gpa;
    Student *top_s = db->data[0], *bot_s = db->data[0];
    for (int i = 0; i < db->count; i++) {
        float g = db->data[i]->gpa;
        sum += g;
        if (g > highest) { highest = g; top_s = db->data[i]; }
        if (g < lowest)  { lowest  = g; bot_s = db->data[i]; }
    }
    printf("\n  === Class Statistics ===\n");
    printf("  Records    : %d\n",   db->count);
    printf("  Average GPA: %.2f\n", sum / db->count);
    printf("  Highest GPA: %.2f  (%s, ID %d)\n", highest, top_s->name, top_s->id);
    printf("  Lowest GPA : %.2f  (%s, ID %d)\n", lowest,  bot_s->name, bot_s->id);
}

void report_top_n(const StudentDB *db, int n) {
    if (!db || db->count == 0) { printf("  No records.\n"); return; }
    if (n <= 0) { printf("  N must be positive.\n"); return; }
    if (n > db->count) n = db->count;

    // temp copy of pointers sorted descending by GPA
    Student **tmp = malloc(db->count * sizeof(Student *));
    if (!tmp) { perror("malloc"); return; }
    memcpy(tmp, db->data, db->count * sizeof(Student *));
    for (int i = 0; i < db->count - 1; i++)
        for (int j = 0; j < db->count - 1 - i; j++)
            if (tmp[j]->gpa < tmp[j + 1]->gpa) {
                Student *sw = tmp[j]; tmp[j] = tmp[j+1]; tmp[j+1] = sw;
            }

    printf("\n  === Top %d Student(s) ===\n", n);
    printf("  %-4s  %-22s  %-20s  %s\n", "Rank", "Name", "Course", "GPA");
    for (int i = 0; i < n; i++)
        printf("  %-4d  %-22s  %-20s  %.2f\n", i + 1, tmp[i]->name, tmp[i]->course, tmp[i]->gpa);
    free(tmp);
}

void report_best_per_course(const StudentDB *db) {
    if (!db || db->count == 0) { printf("  No records.\n"); return; }
    char  courses[64][MAX_COURSE_LEN];
    float best_gpa[64];
    int   best_idx[64];
    int   num_courses = 0;

    for (int i = 0; i < db->count; i++) {
        const char *c = db->data[i]->course;
        int found = -1;
        for (int j = 0; j < num_courses; j++)
            if (strcasecmp(courses[j], c) == 0) { found = j; break; }
        if (found == -1) {
            if (num_courses >= 64) continue;
            strncpy(courses[num_courses], c, MAX_COURSE_LEN - 1);
            best_gpa[num_courses] = db->data[i]->gpa;
            best_idx[num_courses] = i;
            num_courses++;
        } else if (db->data[i]->gpa > best_gpa[found]) {
            best_gpa[found] = db->data[i]->gpa;
            best_idx[found] = i;
        }
    }
    printf("\n  === Best Student per Course ===\n");
    printf("  %-22s  %-22s  %s\n", "Course", "Student", "GPA");
    for (int i = 0; i < num_courses; i++) {
        Student *s = db->data[best_idx[i]];
        printf("  %-22s  %-22s  %.2f\n", courses[i], s->name, s->gpa);
    }
}

void report_course_avg(const StudentDB *db) {
    if (!db || db->count == 0) { printf("  No records.\n"); return; }
    char  courses[64][MAX_COURSE_LEN];
    float sum_gpa[64];
    int   count_per[64];
    int   num_courses = 0;

    for (int i = 0; i < db->count; i++) {
        const char *c = db->data[i]->course;
        int found = -1;
        for (int j = 0; j < num_courses; j++)
            if (strcasecmp(courses[j], c) == 0) { found = j; break; }
        if (found == -1) {
            if (num_courses >= 64) continue;
            strncpy(courses[num_courses], c, MAX_COURSE_LEN - 1);
            sum_gpa[num_courses]   = db->data[i]->gpa;
            count_per[num_courses] = 1;
            num_courses++;
        } else {
            sum_gpa[found]   += db->data[i]->gpa;
            count_per[found] += 1;
        }
    }
    printf("\n  === Course Average Performance ===\n");
    printf("  %-22s  %9s  %s\n", "Course", "Students", "Avg GPA");
    for (int i = 0; i < num_courses; i++)
        printf("  %-22s  %9d  %.2f\n", courses[i], count_per[i], sum_gpa[i] / count_per[i]);
}

// sorts a temp copy of GPA values to find median without touching the db order
void report_median_gpa(StudentDB *db) {
    if (!db || db->count == 0) { printf("  No records.\n"); return; }
    float *gpas = malloc(db->count * sizeof(float));
    if (!gpas) { perror("malloc"); return; }
    for (int i = 0; i < db->count; i++) gpas[i] = db->data[i]->gpa;
    for (int i = 0; i < db->count - 1; i++)
        for (int j = 0; j < db->count - 1 - i; j++)
            if (gpas[j] > gpas[j + 1]) { float t = gpas[j]; gpas[j] = gpas[j+1]; gpas[j+1] = t; }
    float median = (db->count % 2 == 0)
                   ? (gpas[db->count / 2 - 1] + gpas[db->count / 2]) / 2.0f
                   : gpas[db->count / 2];
    printf("  Median GPA: %.2f  (over %d record(s))\n", median, db->count);
    free(gpas);
}

Student *student_alloc(void) {
    Student *s = calloc(1, sizeof(Student));
    if (!s) perror("calloc Student");
    return s;
}

void student_free(Student *s) { free(s); }

float student_calc_gpa(Student *s) {
    if (!s || s->numSubjects <= 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < s->numSubjects; i++) sum += s->grades[i];
    return sum / s->numSubjects;
}

void student_print(const Student *s) {
    if (!s) return;
    printf("\n  --------------------------------\n");
    printf("  ID      : %d\n",   s->id);
    printf("  Name    : %s\n",   s->name);
    printf("  Age     : %d\n",   s->age);
    printf("  Course  : %s\n",   s->course);
    printf("  GPA     : %.2f\n", s->gpa);
    printf("  Grades  : ");
    for (int i = 0; i < s->numSubjects; i++) {
        printf("%.1f", s->grades[i]);
        if (i < s->numSubjects - 1) printf(", ");
    }
    printf("\n  --------------------------------\n");
}

void student_print_short(const Student *s) {
    if (!s) return;
    printf("  [%d] %-22s  %-20s  GPA: %.2f\n", s->id, s->name, s->course, s->gpa);
}

int input_int(const char *prompt, int min, int max) {
    int val; char buf[64];
    while (1) {
        printf("%s", prompt); fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) continue;
        if (sscanf(buf, "%d", &val) == 1 && val >= min && val <= max) return val;
        printf("  Enter an integer between %d and %d.\n", min, max);
    }
}

float input_float(const char *prompt, float min, float max) {
    float val; char buf[64];
    while (1) {
        printf("%s", prompt); fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) continue;
        if (sscanf(buf, "%f", &val) == 1 && val >= min && val <= max) return val;
        printf("  Enter a number between %.1f and %.1f.\n", min, max);
    }
}

void input_string(const char *prompt, char *buf, int maxlen) {
    while (1) {
        printf("%s", prompt); fflush(stdout);
        if (!fgets(buf, maxlen, stdin)) continue;
        buf[strcspn(buf, "\n")] = '\0';
        trim(buf);
        if (strlen(buf) > 0) return;
        printf("  Input cannot be empty.\n");
    }
}

int input_unique_id(const StudentDB *db, const char *prompt) {
    while (1) {
        int id = input_int(prompt, 1, 9999999);
        if (!db_find_by_id(db, id)) return id;
        printf("  ID %d already taken.\n", id);
    }
}
