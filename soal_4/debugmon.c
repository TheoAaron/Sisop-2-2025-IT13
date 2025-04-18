#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <sys/wait.h>

#define LOG_DIR "/home/ubuntu/debugmon_logs/"
#define MAX_CMDLINE_LEN 1024
#define TIME_BUF_SIZE 64

int is_numeric(const char *str) {
    for (int i = 0; str[i]; i++) {
        if (!isdigit((unsigned char)str[i])) return 0;
    }
    return 1;
}

void get_formatted_time(char *buffer, size_t size, const char *format) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, format, tm_info);
}

void log_global_process_status(const char *process_name, const char *status) {
    char time_str[TIME_BUF_SIZE];
    get_formatted_time(time_str, sizeof(time_str), "[%d:%m:%Y-%H:%M:%S]");

    char path[256];
    snprintf(path, sizeof(path), LOG_DIR "debugmon.log");

    FILE *fp = fopen(path, "a");
    if (!fp) return;
    fprintf(fp, "%s\t%s\t%s\n", time_str, process_name, status);
    fclose(fp);
}

void list_processes(const char *username, FILE *out) {
    DIR *proc = opendir("/proc");
    if (!proc) {
        fprintf(out, "Failed to open /proc\n");
        return;
    }

    char hdr_time[TIME_BUF_SIZE];
    get_formatted_time(hdr_time, sizeof(hdr_time), "%Y-%m-%d %H:%M:%S");
    fprintf(out, "\n=== PROCESS LIST %s ===\n", hdr_time);
    fprintf(out, "PID\tCOMMAND\tCPU_TIME\tMEM_KB\n");

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (!is_numeric(entry->d_name)) continue;

        // filter by owner
        char status_path[256], line[256];
        snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
        FILE *sfp = fopen(status_path, "r");
        if (!sfp) continue;
        uid_t uid = -1; int matched = 0;
        while (fgets(line, sizeof(line), sfp)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%u", &uid);
                struct passwd *pw = getpwuid(uid);
                if (pw && strcmp(pw->pw_name, username) == 0) matched = 1;
                break;
            }
        }
        fclose(sfp);
        if (!matched) continue;

        // baca cmdline
        char cmd[MAX_CMDLINE_LEN] = "[unknown]";
        snprintf(status_path, sizeof(status_path), "/proc/%s/cmdline", entry->d_name);
        FILE *cfp = fopen(status_path, "r");
        if (cfp) {
            size_t n = fread(cmd, 1, MAX_CMDLINE_LEN-1, cfp);
            fclose(cfp);
            for (size_t i = 0; i < n; i++) if (cmd[i] == '\0') cmd[i] = ' ';
            cmd[n] = '\0';
        }

        // baca CPU times
        unsigned long ut = 0, st = 0;
        snprintf(status_path, sizeof(status_path), "/proc/%s/stat", entry->d_name);
        FILE *tfp = fopen(status_path, "r");
        if (tfp) {
            fscanf(tfp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                   &ut, &st);
            fclose(tfp);
        }

        // baca mem (pages)
        int pages = 0;
        snprintf(status_path, sizeof(status_path), "/proc/%s/statm", entry->d_name);
        FILE *mfp = fopen(status_path, "r");
        if (mfp) {
            fscanf(mfp, "%d", &pages);
            fclose(mfp);
        }

        unsigned long cpu_time = ut + st;
        int mem_kb = pages * (getpagesize()/1024);

        fprintf(out, "%s\t%s\t%lu\t%d\n",
                entry->d_name, cmd, cpu_time, mem_kb);

        log_global_process_status(cmd, "RUNNING");
    }

    closedir(proc);
}

void stop_daemon(const char *username) {
    char pid_path[256];
    snprintf(pid_path, sizeof(pid_path), "/home/ubuntu/debugmon_%s.pid", username);

    FILE *pf = fopen(pid_path, "r");
    if (!pf) {
        printf("Daemon not running for %s\n", username);
        return;
    }
    pid_t pid;
    if (fscanf(pf, "%d", &pid) != 1) {
        fclose(pf);
        printf("Corrupt PID file for %s\n", username);
        return;
    }
    fclose(pf);

    if (kill(pid, SIGTERM) == 0) {
        printf("Stopped daemon for %s (PID %d)\n", username, pid);
        remove(pid_path);
    } else {
        perror("Failed to stop daemon");
    }
}

void fail_user(const char *username) {
    DIR *proc = opendir("/proc");
    if (!proc) { perror("open /proc"); return; }

    char log_path[256];
    snprintf(log_path, sizeof(log_path), LOG_DIR "debugmon_%s.log", username);
    FILE *lf = fopen(log_path, "a");
    if (!lf) { perror("open log"); closedir(proc); return; }

    char tbuf[TIME_BUF_SIZE];
    get_formatted_time(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S");
    fprintf(lf, "\n=== FAILED PROCESSES %s ===\n", tbuf);
    fprintf(lf, "PID\tCOMMAND\tSTATUS\n");

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (!is_numeric(entry->d_name)) continue;

        // filter by owner
        char stpath[256], line[256];
        snprintf(stpath, sizeof(stpath), "/proc/%s/status", entry->d_name);
        FILE *sfp = fopen(stpath, "r");
        if (!sfp) continue;
        uid_t uid; int matched = 0;
        while (fgets(line, sizeof(line), sfp)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%u", &uid);
                struct passwd *pw = getpwuid(uid);
                if (pw && strcmp(pw->pw_name, username) == 0) matched = 1;
                break;
            }
        }
        fclose(sfp);
        if (!matched) continue;

        // baca cmdline
        char cmd[MAX_CMDLINE_LEN] = "[unknown]";
        snprintf(stpath, sizeof(stpath), "/proc/%s/cmdline", entry->d_name);
        FILE *cfp = fopen(stpath, "r");
        if (cfp) {
            size_t n = fread(cmd, 1, MAX_CMDLINE_LEN-1, cfp);
            fclose(cfp);
            for (size_t i = 0; i < n; i++) if (cmd[i] == '\0') cmd[i] = ' ';
            cmd[n] = '\0';
        }

        pid_t pid = atoi(entry->d_name);
        if (kill(pid, SIGKILL) == 0) {
            fprintf(lf, "%d\t%s\tTERMINATED\n", pid, cmd);
            log_global_process_status(cmd, "FAILED");
        } else {
            if (errno == ESRCH) {
                fprintf(lf, "%d\t%s\tALREADY_DEAD\n", pid, cmd);
            } else {
                fprintf(lf, "%d\t%s\tFAILED\n", pid, cmd);
            }
            log_global_process_status(cmd, "FAILED");
        }
    }

    fclose(lf);
    closedir(proc);
    printf("All processes for %s terminated and logged\n", username);
}

void daemon_mode(const char *username) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid > 0) {
        char pidfile[256];
        snprintf(pidfile, sizeof(pidfile), "/home/ubuntu/debugmon_%s.pid", username);
        FILE *pf = fopen(pidfile, "w");
        if (pf) { fprintf(pf, "%d", pid); fclose(pf); }
        printf("Daemon started with PID %d\n", pid);
        return;
    }

    setsid();
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    while (1) {
        FILE *lf = fopen(LOG_DIR "debugmon.log", "a");
        if (lf) {
            list_processes(username, lf);
            fclose(lf);
        }
        sleep(5);
    }
}

void revert_user(const char *username) {
    char log_path[256];
    snprintf(log_path, sizeof(log_path), LOG_DIR "debugmon_%s.log", username);

    FILE *lf = fopen(log_path, "r");
    if (!lf) {
        printf("No log file for %s\n", username);
        return;
    }
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        fclose(lf);
        return;
    }

    char line[2048], orig[2048];
    int in_failed = 0, reverted = 0;

    while (fgets(line, sizeof(line), lf)) {
        // simpan salinan utuh sebelum strtok
        strcpy(orig, line);

        if (strstr(line, "=== FAILED PROCESSES")) {
            in_failed = 1;
            fputs(orig, tmp);
            continue;
        }

        if (in_failed) {
            // lewati header kolom dan baris kosong
            if (strncmp(line, "PID\tCOMMAND\tSTATUS", 19) == 0 || line[0] == '\n') {
                fputs(orig, tmp);
                continue;
            }

            // parsing PID dan COMMAND saja
            char *saveptr;
            char *pid_tok = strtok_r(line, "\t\n", &saveptr);
            char *cmd_tok = strtok_r(NULL, "\t\n", &saveptr);
            // (status ada di strtok_r(NULL,...), tapi kita abaikan)

            if (pid_tok && cmd_tok && is_numeric(pid_tok)) {
                pid_t c = fork();
                if (c == 0) {
                    execl("/bin/sh", "sh", "-c", cmd_tok, (char *)NULL);
                    perror("execl");
                    exit(1);
                } else if (c > 0) {
                    waitpid(c, NULL, 0);
                    reverted++;
                } else {
                    perror("fork");
                }
            }
            fputs(orig, tmp);
        } else {
            fputs(orig, tmp);
        }
    }

    fclose(lf);

    // tambahkan ringkasan revert di akhir
    char tbuf[TIME_BUF_SIZE];
    get_formatted_time(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S");
    fprintf(tmp, "\n=== REVERT PROCESSES %s ===\nRestored %d processes\n",
            tbuf, reverted);

    // tulis kembali ke file asli
    rewind(tmp);
    lf = fopen(log_path, "w");
    if (lf) {
        while (fgets(line, sizeof(line), tmp)) {
            fputs(line, lf);
        }
        fclose(lf);
    }
    fclose(tmp);

    printf("Successfully reverted %d processes for %s\n", reverted, username);
}

int main(int argc, char *argv[]) {
    signal(SIGCHLD, SIG_IGN);

    if (argc < 3) {
        printf("Usage: %s <list|daemon|stop|fail|revert> <username>\n", argv[0]);
        return 1;
    }
    const char *cmd = argv[1], *user = argv[2];

    if      (strcmp(cmd, "list") == 0)   list_processes(user, stdout);
    else if (strcmp(cmd, "daemon") == 0) daemon_mode(user);
    else if (strcmp(cmd, "stop") == 0)   stop_daemon(user);
    else if (strcmp(cmd, "fail") == 0)   fail_user(user);
    else if (strcmp(cmd, "revert") == 0) revert_user(user);
    else                                  printf("Invalid command\n");

    return 0;
}
