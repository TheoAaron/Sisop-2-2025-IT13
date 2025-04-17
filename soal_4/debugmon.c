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
        if (!isdigit(str[i])) return 0;
    }
    return 1;
}

void get_formatted_time(char *buffer, size_t size, const char *format) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, format, tm_info);
}

void log_global_process_status(const char *process_name, const char *status) {
    char time_str[32];
    get_formatted_time(time_str, sizeof(time_str), "%d:%m:%Y-%H:%M:%S");

    char log_path[256];
    snprintf(log_path, sizeof(log_path), LOG_DIR "debugmon.log");

    FILE *log_fp = fopen(log_path, "a");
    if (!log_fp) return;

    fprintf(log_fp, "[%s]_%s_%s\n", time_str, process_name, status);
    fclose(log_fp);
}

void list_processes(const char *username, FILE *out) {
    DIR *proc = opendir("/proc");
    if (!proc) {
        fprintf(out, "Failed to open /proc\n");
        return;
    }
    
    struct dirent *entry;
    char time_buf[TIME_BUF_SIZE];
    get_formatted_time(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S");
    
    fprintf(out, "\n=== PROCESS LIST %s ===\n", time_buf);
    fprintf(out, "%-10s %-30s %-12s %-10s\n", 
            "PID", "COMMAND", "CPU TIME", "MEM (KB)");

    while ((entry = readdir(proc)) != NULL) {
        if (!is_numeric(entry->d_name)) continue;

        char path[256];
        sprintf(path, "/proc/%s/status", entry->d_name);
        
        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        uid_t uid = -1;
        int matched = 0;
        char line[256];
        
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%u", &uid);
                struct passwd *pwd = getpwuid(uid);
                if (pwd && strcmp(pwd->pw_name, username) == 0) matched = 1;
                break;
            }
        }
        fclose(fp);
        
        if (!matched) continue;

        sprintf(path, "/proc/%s/cmdline", entry->d_name);
        FILE *cmd_fp = fopen(path, "r");
        char cmd[MAX_CMDLINE_LEN] = "[unknown]";
        
        if (cmd_fp) {
            size_t n = fread(cmd, 1, MAX_CMDLINE_LEN-1, cmd_fp);
            fclose(cmd_fp);
            
            for (size_t i = 0; i < n; i++) {
                if (cmd[i] == '\0') cmd[i] = ' ';
            }
            cmd[n] = '\0';
            
            while (n > 0 && cmd[n-1] == ' ') cmd[--n] = '\0';
        }

        unsigned long utime = 0, stime = 0;
        sprintf(path, "/proc/%s/stat", entry->d_name);
        FILE *stat_fp = fopen(path, "r");
        
        if (stat_fp) {
            fscanf(stat_fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                   &utime, &stime);
            fclose(stat_fp);
        }

        int pages = 0;
        sprintf(path, "/proc/%s/statm", entry->d_name);
        FILE *mem_fp = fopen(path, "r");
        
        if (mem_fp) {
            fscanf(mem_fp, "%d", &pages);
            fclose(mem_fp);
        }

        fprintf(out, "%-10s %-30s %-12lu %-10d\n",
                entry->d_name,
                cmd,
                utime + stime,
                pages * 4);

        log_global_process_status(cmd, "RUNNING");
    }
    closedir(proc);
}

void stop_daemon(const char *username) {
    char pid_path[256];
    sprintf(pid_path, "/home/ubuntu/debugmon_%s.pid", username);

    FILE *pid_fp = fopen(pid_path, "r");
    if (!pid_fp) {
        printf("Daemon not running for %s\n", username);
        return;
    }
    
    pid_t pid;
    if (fscanf(pid_fp, "%d", &pid) != 1) {
        fclose(pid_fp);
        printf("Corrupt PID file for %s\n", username);
        return;
    }
    fclose(pid_fp);

    if (kill(pid, 0) == -1) {
        if (errno == ESRCH) {
            printf("Daemon process %d does not exist\n", pid);
        } else {
            perror("Process check failed");
        }
        remove(pid_path);
        return;
    }

    if (kill(pid, SIGTERM) == 0) {
        printf("Stopped daemon for %s (PID %d)\n", username, pid);
        remove(pid_path);
    } else {
        perror("Failed to stop daemon");
    }
}

void fail_user(const char *username) {
    DIR *proc = opendir("/proc");
    if (!proc) {
        perror("Failed to open /proc");
        return;
    }

    char log_path[256];
    sprintf(log_path, LOG_DIR "debugmon_%s.log", username);
    FILE *log_fp = fopen(log_path, "a");
    
    if (!log_fp) {
        perror("Failed to open log file");
        closedir(proc);
        return;
    }

    char time_buf[TIME_BUF_SIZE];
    get_formatted_time(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S");
    fprintf(log_fp, "\n=== FAILED PROCESSES %s ===\n", time_buf);
    fprintf(log_fp, "%-10s %-30s %-20s\n", "PID", "COMMAND", "STATUS");

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (!is_numeric(entry->d_name)) continue;

        char path[256];
        sprintf(path, "/proc/%s/status", entry->d_name);
        
        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        uid_t uid = -1;
        int matched = 0;
        char line[256];
        
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%u", &uid);
                struct passwd *pwd = getpwuid(uid);
                if (pwd && strcmp(pwd->pw_name, username) == 0) matched = 1;
                break;
            }
        }
        fclose(fp);

        if (!matched) continue;

        sprintf(path, "/proc/%s/cmdline", entry->d_name);
        FILE *cmd_fp = fopen(path, "r");
        char cmd[MAX_CMDLINE_LEN] = "[unknown]";
        
        if (cmd_fp) {
            size_t n = fread(cmd, 1, MAX_CMDLINE_LEN-1, cmd_fp);
            fclose(cmd_fp);
            
            for (size_t i = 0; i < n; i++) {
                if (cmd[i] == '\0') cmd[i] = ' ';
            }
            cmd[n] = '\0';
            
            while (n > 0 && cmd[n-1] == ' ') cmd[--n] = '\0';
        }

        pid_t pid = atoi(entry->d_name);
        int kill_result = kill(pid, SIGKILL);
        
        if (kill_result == 0) {
            fprintf(log_fp, "%-10d %-30s %-20s\n", pid, cmd, "TERMINATED");
            log_global_process_status(cmd, "FAILED");
        } else {
            if (errno == ESRCH) {
                fprintf(log_fp, "%-10d %-30s %-20s\n", pid, cmd, "ALREADY DEAD");
            } else {
                fprintf(log_fp, "%-10d %-30s %-20s (%s)\n", 
                        pid, cmd, "FAILED", strerror(errno));
            }
            log_global_process_status(cmd, "FAILED");
        }
    }

    closedir(proc);
    fclose(log_fp);
    printf("All processes for %s terminated and logged\n", username);
}

void daemon_mode(const char *username) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
    
    if (pid > 0) {
        char pid_path[256];
        sprintf(pid_path, "/home/ubuntu/debugmon_%s.pid", username);
        
        FILE *pid_fp = fopen(pid_path, "w");
        if (pid_fp) {
            fprintf(pid_fp, "%d", pid);
            fclose(pid_fp);
        }
        printf("Daemon started with PID %d\n", pid);
        return;
    }

    setsid();
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    while (1) {
        char log_path[256];
        sprintf(log_path, LOG_DIR "debugmon_%s.log", username);
        
        FILE *log_fp = fopen(log_path, "a");
        if (log_fp) {
            list_processes(username, log_fp);
            fflush(log_fp);
            fclose(log_fp);
        }
        sleep(5);
    }
}

void revert_user(const char *username) {
    char log_path[256];
    sprintf(log_path, LOG_DIR "debugmon_%s.log", username);

    FILE *log_fp = fopen(log_path, "r");
    if (!log_fp) {
        printf("No log file found for %s\n", username);
        return;
    }

    char time_buf[TIME_BUF_SIZE];
    get_formatted_time(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S");

    FILE *temp_fp = tmpfile();
    int in_failed_section = 0;
    int reverted_count = 0;
    char line[512];

    while (fgets(line, sizeof(line), log_fp)) {
        if (strstr(line, "=== FAILED PROCESSES")) {
            in_failed_section = 1;
            continue;
        }

        if (in_failed_section) {
            if (strstr(line, "PID") || strlen(line) < 10) continue;

            char pid_str[32], cmd[256], status[256];
            pid_str[0] = cmd[0] = status[0] = '\0';

            int ret = sscanf(line, "%31s\t%255[^\t]\t%255[^\n]", pid_str, cmd, status);
            if (ret < 2) continue;

            // Trim trailing whitespace
            int len = strlen(cmd);
            while (len > 0 && isspace((unsigned char)cmd[len - 1])) {
                cmd[len - 1] = '\0';
                len--;
            }

            // Debug print to stderr
            fprintf(stderr, "Revert command: '%s'\n", cmd);

            pid_t pid = fork();
            if (pid == 0) {
                execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
                perror("execl failed");
                exit(EXIT_FAILURE);
            } else if (pid > 0) {
                reverted_count++;
            } else {
                perror("fork failed");
            }
        }

        fputs(line, temp_fp);
    }
    fclose(log_fp);

    rewind(temp_fp);
    log_fp = fopen(log_path, "w");
    while (fgets(line, sizeof(line), temp_fp)) {
        fputs(line, log_fp);
    }

    fprintf(log_fp, "\n=== REVERT PROCESSES %s ===\n", time_buf);
    fprintf(log_fp, "Restored %d processes\n", reverted_count);

    fclose(log_fp);
    fclose(temp_fp);
    printf("Successfully reverted %d processes for %s\n", reverted_count, username);
}

int main(int argc, char *argv[]) {
    // Tangani SIGCHLD untuk menghindari zombie
    signal(SIGCHLD, SIG_IGN);

    if (argc < 3) {
        printf("Usage: %s <list|daemon|stop|fail|revert> <username>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        list_processes(argv[2], stdout);
    }
    else if (strcmp(argv[1], "daemon") == 0) {
        daemon_mode(argv[2]);
    }
    else if (strcmp(argv[1], "stop") == 0) {
        stop_daemon(argv[2]);
    }
    else if (strcmp(argv[1], "fail") == 0) {
        fail_user(argv[2]);
    }
    else if (strcmp(argv[1], "revert") == 0) {
        revert_user(argv[2]);
    }
    else {
        printf("Invalid command\n");
    }

    return 0;
}
