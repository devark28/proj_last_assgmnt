/*
 * Multi-threaded Web Scraper
 * ============================
 * Fetches multiple URLs concurrently using POSIX threads (pthreads).
 * Each thread uses popen() to invoke the system's curl command and
 * writes the downloaded content to a separate output file.
 *
 * Dependencies: pthreads, curl CLI (standard on most Linux systems)
 * Compile:  see Makefile  (gcc -lpthread)
 *
 * Usage:
 *   ./scraper                        # uses built-in default URL list
 *   ./scraper <url1> <url2> ...      # fetch the specified URLs
 *
 * Output files: scraped_pages/page_01.txt, page_02.txt, ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

/* ----------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------- */
#define MAX_URLS        32
#define MAX_URL_LEN    512
#define OUTPUT_DIR     "scraped_pages"
#define READ_CHUNK     4096    /* bytes read per fread call */
#define CURL_TIMEOUT   30      /* seconds before curl gives up */

/* ----------------------------------------------------------------
 * Per-thread argument / result struct
 * ---------------------------------------------------------------- */
typedef struct {
    int    thread_id;
    char   url[MAX_URL_LEN];
    char   output_path[300];
    int    success;          /* 1 = fetched and saved, 0 = error */
    size_t bytes_received;
    char   error_msg[256];
} ThreadArgs;

/* ----------------------------------------------------------------
 * Thread function — fetches one URL via popen(curl)
 * ---------------------------------------------------------------- */
static void *scrape_url(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    char        cmd[MAX_URL_LEN + 128];
    FILE       *curl_pipe;
    FILE       *out_fp;
    char        chunk[READ_CHUNK];
    size_t      bytes_read;
    size_t      total = 0;

    printf("[Thread %02d] Starting  %s\n", args->thread_id, args->url);

    /*
     * Build the curl command:
     *   -s          silent (no progress bar)
     *   -L          follow redirects
     *   --max-time  abort if transfer takes too long
     *   -A          set a user-agent string
     *   --fail      return non-zero exit on HTTP errors (4xx/5xx)
     */
    snprintf(cmd, sizeof(cmd),
             "curl -s -L --max-time %d -A \"MultiScraper/1.0\" --fail \"%s\" 2>&1",
             CURL_TIMEOUT, args->url);

    curl_pipe = popen(cmd, "r");
    if (!curl_pipe) {
        snprintf(args->error_msg, sizeof(args->error_msg),
                 "popen() failed: %s", strerror(errno));
        args->success = 0;
        pthread_exit(args);
    }

    out_fp = fopen(args->output_path, "w");
    if (!out_fp) {
        snprintf(args->error_msg, sizeof(args->error_msg),
                 "cannot open output file: %s", strerror(errno));
        pclose(curl_pipe);
        args->success = 0;
        pthread_exit(args);
    }

    /* Write a small header to the output file */
    time_t now = time(NULL);
    fprintf(out_fp, "=== Scraped Page ===\n");
    fprintf(out_fp, "URL      : %s\n", args->url);
    fprintf(out_fp, "Thread   : %d\n", args->thread_id);
    fprintf(out_fp, "Time     : %s", ctime(&now));
    fprintf(out_fp, "====================\n\n");

    /* Stream data from curl into the file */
    while ((bytes_read = fread(chunk, 1, sizeof(chunk), curl_pipe)) > 0) {
        fwrite(chunk, 1, bytes_read, out_fp);
        total += bytes_read;
    }

    fclose(out_fp);
    int exit_code = pclose(curl_pipe);

    if (exit_code != 0 && total == 0) {
        snprintf(args->error_msg, sizeof(args->error_msg),
                 "curl exited with code %d (URL may be unreachable)", exit_code);
        args->success = 0;
    } else {
        args->bytes_received = total;
        args->success        = 1;
        printf("[Thread %02d] Done     — %zu bytes -> %s\n",
               args->thread_id, total, args->output_path);
    }

    pthread_exit(args);
}

/* ----------------------------------------------------------------
 * Create output directory if it doesn't exist
 * ---------------------------------------------------------------- */
static void ensure_output_dir(void) {
    struct stat st;
    if (stat(OUTPUT_DIR, &st) == -1) {
        if (mkdir(OUTPUT_DIR, 0755) != 0) {
            perror("mkdir " OUTPUT_DIR);
            exit(EXIT_FAILURE);
        }
        printf("Created output directory: %s/\n", OUTPUT_DIR);
    }
}

/* ----------------------------------------------------------------
 * Check that curl is available on the system
 * ---------------------------------------------------------------- */
static void check_curl(void) {
    if (system("curl --version > /dev/null 2>&1") != 0) {
        fprintf(stderr, "Error: 'curl' is not installed or not in PATH.\n");
        fprintf(stderr, "Install it with: sudo apt-get install curl\n");
        exit(EXIT_FAILURE);
    }
}

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(int argc, char *argv[]) {

    static const char *default_urls[] = {
        "https://example.com",
        "https://httpbin.org/get",
        "https://jsonplaceholder.typicode.com/posts/1",
        "https://www.iana.org/domains/reserved",
        "https://httpbin.org/uuid",
        NULL
    };

    const char **urls;
    int num_urls;

    if (argc > 1) {
        if (argc - 1 > MAX_URLS) {
            fprintf(stderr, "Too many URLs (max %d).\n", MAX_URLS);
            return EXIT_FAILURE;
        }
        urls     = (const char **)&argv[1];
        num_urls = argc - 1;
    } else {
        urls     = default_urls;
        num_urls = 0;
        while (default_urls[num_urls]) num_urls++;
    }

    check_curl();
    ensure_output_dir();

    pthread_t  *threads = malloc(num_urls * sizeof(pthread_t));
    ThreadArgs *args    = malloc(num_urls * sizeof(ThreadArgs));

    if (!threads || !args) {
        perror("malloc");
        free(threads);
        free(args);
        return EXIT_FAILURE;
    }

    printf("Launching %d scraper thread(s)...\n\n", num_urls);

    /* ---- Create one thread per URL ---- */
    for (int i = 0; i < num_urls; i++) {
        args[i].thread_id      = i + 1;
        args[i].success        = 0;
        args[i].bytes_received = 0;
        args[i].error_msg[0]   = '\0';

        strncpy(args[i].url, urls[i], MAX_URL_LEN - 1);
        args[i].url[MAX_URL_LEN - 1] = '\0';

        snprintf(args[i].output_path, sizeof(args[i].output_path),
                 "%s/page_%02d.txt", OUTPUT_DIR, i + 1);

        int rc = pthread_create(&threads[i], NULL, scrape_url, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed for thread %d: %s\n",
                    i + 1, strerror(rc));
            snprintf(args[i].error_msg, sizeof(args[i].error_msg),
                     "pthread_create: %s", strerror(rc));
        }
    }

    /* ---- Wait for all threads to finish ---- */
    for (int i = 0; i < num_urls; i++) {
        pthread_join(threads[i], NULL);
    }

    /* ---- Print summary ---- */
    printf("\n");
    printf("+================================================================+\n");
    printf("|                      SCRAPE SUMMARY                           |\n");
    printf("+----+--------+------------+---------------------------------+\n");
    printf("| ID | Status |  Bytes     | URL                             |\n");
    printf("+----+--------+------------+---------------------------------+\n");

    int ok = 0;
    for (int i = 0; i < num_urls; i++) {
        char short_url[34];
        strncpy(short_url, args[i].url, 33);
        short_url[33] = '\0';

        printf("| %2d | %-6s | %10zu | %-31s |\n",
               args[i].thread_id,
               args[i].success ? "OK" : "FAIL",
               args[i].bytes_received,
               short_url);

        if (!args[i].success && args[i].error_msg[0] != '\0') {
            printf("|    Error: %-53s |\n", args[i].error_msg);
        }

        if (args[i].success) ok++;
    }

    printf("+----+--------+------------+---------------------------------+\n");
    printf("  Result : %d / %d URL(s) fetched successfully\n", ok, num_urls);
    printf("  Output : %s/\n\n", OUTPUT_DIR);

    free(threads);
    free(args);

    return (ok == num_urls) ? EXIT_SUCCESS : EXIT_FAILURE;
}
