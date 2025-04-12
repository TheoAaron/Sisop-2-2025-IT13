#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <pwd.h>
#include <fcntl.h>

#define LOG_FILE "debugmon.log"
#define PID_FILE "debugmon.pid"
#define BLOCK_FILE "/tmp/debugmon_block"

// Get username dari UID
char *get_username_from_uid(int uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) return pw->pw_name;
    return NULL;
}

// Logging
void write_log(const char *process_name, const char *status) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    char time_buf[100];
    strftime(time_buf, sizeof(time_buf), "[%d:%m:%Y]-[%H:%M:%S]", lt);
    fprintf(log, "%s_%s_%s\n", time_buf, process_name, status);
    fclose(log);
}

// Cek apakah username ada di block list
int is_user_blocked(const char *user) {
    FILE *block = fopen(BLOCK_FILE, "r");
    if (!block) return 0;

    char line[128];
    while (fgets(line, sizeof(line), block)) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, user) == 0) {
            fclose(block);
            return 1;
        }
    }

    fclose(block);
    return 0;
}

// Tambahkan user ke block list
void add_to_block(const char *user) {
    if (is_user_blocked(user)) return;
    FILE *block = fopen(BLOCK_FILE, "a");
    if (block) {
        fprintf(block, "%s\n", user);
        fclose(block);
    }
}

// Hapus user dari block list
void remove_from_block(const char *user) {
    FILE *block = fopen(BLOCK_FILE, "r");
    if (!block) return;

    FILE *tmp = fopen("/tmp/debugmon_block_tmp", "w");
    char line[128];

    while (fgets(line, sizeof(line), block)) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, user) != 0)
            fprintf(tmp, "%s\n", line);
    }

    fclose(block);
    fclose(tmp);
    rename("/tmp/debugmon_block_tmp", BLOCK_FILE);
}

// list <user>
void list_processes(const char *user) {
    DIR *dir = opendir("/proc");
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(entry->d_name[0])) continue;

        char status_path[256];
        snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
        FILE *fp = fopen(status_path, "r");
        if (!fp) continue;

        char line[256], name[100] = "", username[100] = "";
        int uid = -1;

        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Name:", 5) == 0)
                sscanf(line, "Name:\t%s", name);
            else if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%d", &uid);
                char *u = get_username_from_uid(uid);
                if (u) strncpy(username, u, sizeof(username));
                break;
            }
        }

        fclose(fp);

        if (strcmp(username, user) == 0) {
            printf("PID: %s\tProcess: %s\tUser: %s\n", entry->d_name, name, username);
        }
    }

    closedir(dir);
}

// daemon <user>
void start_daemon(const char *user) {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) {
        printf("Daemon started.\n");
        FILE *pf = fopen(PID_FILE, "w");
        if (pf) {
            fprintf(pf, "%d\n", pid);
            fclose(pf);
        }
        exit(EXIT_SUCCESS);
    }

    umask(0);
    setsid();
    chdir("/");
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    while (1) {
        if (is_user_blocked(user)) {
            sleep(10);
            continue;
        }

        DIR *dir = opendir("/proc");
        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL) {
            if (!isdigit(entry->d_name[0])) continue;

            char status_path[256];
            snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
            FILE *fp = fopen(status_path, "r");
            if (!fp) continue;

            char line[256], name[100] = "", username[100] = "";
            int uid = -1;

            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "Name:", 5) == 0)
                    sscanf(line, "Name:\t%s", name);
                else if (strncmp(line, "Uid:", 4) == 0) {
                    sscanf(line, "Uid:\t%d", &uid);
                    char *u = get_username_from_uid(uid);
                    if (u) strncpy(username, u, sizeof(username));
                    break;
                }
            }

            fclose(fp);

            if (strcmp(username, user) == 0) {
                write_log(name, "RUNNING");
            }
        }

        closedir(dir);
        sleep(10);
    }
}

// stop <user>
void stop_daemon(const char *user) {
    FILE *pf = fopen(PID_FILE, "r");
    if (!pf) {
        printf("Daemon not running.\n");
        return;
    }

    int pid;
    fscanf(pf, "%d", &pid);
    fclose(pf);

    if (kill(pid, SIGTERM) == 0) {
        printf("Stopped daemon (PID: %d)\n", pid);
        remove(PID_FILE);
    } else {
        perror("Failed to stop daemon");
    }
}

// fail <user>
void fail_processes(const char *user) {
    DIR *dir = opendir("/proc");
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(entry->d_name[0])) continue;

        char status_path[256];
        snprintf(status_path, sizeof(status_path), "/proc/%s/status", entry->d_name);
        FILE *fp = fopen(status_path, "r");
        if (!fp) continue;

        char line[256], name[100] = "", username[100] = "";
        int uid = -1;

        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Name:", 5) == 0)
                sscanf(line, "Name:\t%s", name);
            else if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%d", &uid);
                char *u = get_username_from_uid(uid);
                if (u) strncpy(username, u, sizeof(username));
                break;
            }
        }

        fclose(fp);

        if (strcmp(username, user) == 0) {
            kill(atoi(entry->d_name), SIGKILL);
            write_log(name, "FAILED");
        }
    }

    closedir(dir);
    add_to_block(user);
}

// revert <user>
void revert_user(const char *user) {
    remove_from_block(user);
    write_log(user, "REVERT_RUNNING");
}

// main
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: ./debugmon <command> <user>\n");
        printf("Commands: list | daemon | stop | fail | revert\n");
        return 1;
    }

    const char *command = argv[1];
    const char *user = argv[2];

    if (strcmp(command, "list") == 0) {
        list_processes(user);
    } else if (strcmp(command, "daemon") == 0) {
        start_daemon(user);
    } else if (strcmp(command, "stop") == 0) {
        stop_daemon(user);
    } else if (strcmp(command, "fail") == 0) {
        fail_processes(user);
    } else if (strcmp(command, "revert") == 0) {
        revert_user(user);
    } else {
        printf("Unknown command.\n");
        return 1;
    }

    return 0;
}
