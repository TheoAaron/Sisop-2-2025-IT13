#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <libgen.h>
#include <sys/wait.h>

#define STARTER_DIR "starter_kit"
#define QUARANTINE_DIR "quarantine"
#define LOG_FILE "activity.log"
#define PID_FILE ".pid"
#define MAX_PATH_LEN 512
#define MAX_LOG_LEN 512
#define MAX_NAME_LEN 256
#define ENC_HEADER "ENCRYPTED"
#define ENC_HEADER_LEN 9

void write_log(const char *msg) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%d-%m-%Y][%H:%M:%S", t);
    fprintf(log, "[%s] - %s\n", timestamp, msg);
    fclose(log);
}

char* base64_decode(const char* input) {
    static char output[MAX_NAME_LEN];
    memset(output, 0, sizeof(output));

    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;

    pid_t pid = fork();
    if (pid == -1) return NULL;

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        char *argv[] = {"base64", "-d", NULL};
        char *envp[] = {NULL};

        int inpipe[2];
        pipe(inpipe);
        pid_t inpid = fork();
        if (inpid == 0) {
            close(inpipe[1]);
            dup2(inpipe[0], STDIN_FILENO);
            close(inpipe[0]);
            execve("/usr/bin/base64", argv, envp);
            exit(1);
        } else {
            close(inpipe[0]);
            write(inpipe[1], input, strlen(input));
            close(inpipe[1]);
            waitpid(inpid, NULL, 0);
            exit(0);
        }
    } else {
        close(pipefd[1]);
        read(pipefd[0], output, MAX_NAME_LEN - 1);
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        output[strcspn(output, "\r\n")] = 0;
        return output;
    }
}


int is_base64_filename(const char *name) {
    int len = strlen(name);
    if (len % 4 != 0) return 0;
    for (int i = 0; i < len; ++i) {
        if (!((name[i] >= 'A' && name[i] <= 'Z') ||
              (name[i] >= 'a' && name[i] <= 'z') ||
              (name[i] >= '0' && name[i] <= '9') ||
              name[i] == '+' || name[i] == '/' || name[i] == '=')) {
            return 0;
        }
    }
    return 1;
}

int is_encrypted(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return 0;
    char header[ENC_HEADER_LEN + 1] = {0};
    fread(header, 1, ENC_HEADER_LEN, fp);
    fclose(fp);
    return strncmp(header, ENC_HEADER, ENC_HEADER_LEN) == 0;
}

void encrypt_and_move(const char *src_path, const char *dest_path) {
    FILE *src = fopen(src_path, "rb");
    FILE *dst = fopen(dest_path, "wb");
    if (!src || !dst) return;

    fwrite(ENC_HEADER, 1, ENC_HEADER_LEN, dst);
    int ch;
    while ((ch = fgetc(src)) != EOF) {
        fputc(ch ^ 0xAA, dst);
    }

    fclose(src);
    fclose(dst);
    remove(src_path);
}

void decrypt_and_move(const char *src_path, const char *dest_path) {
    FILE *src = fopen(src_path, "rb");
    FILE *dst = fopen(dest_path, "wb");
    if (!src || !dst) return;

    fseek(src, ENC_HEADER_LEN, SEEK_SET);
    int ch;
    while ((ch = fgetc(src)) != EOF) {
        fputc(ch ^ 0xAA, dst);
    }

    fclose(src);
    fclose(dst);
    remove(src_path);
}

void daemon_loop() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) {
        FILE *pidf = fopen(PID_FILE, "w");
        fprintf(pidf, "%d", pid);
        fclose(pidf);
        return;
    }
    setsid();
    chdir(".");
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    while (1) {
        FILE *flag = fopen(".decrypt_now", "r");
        if (flag) {
            fclose(flag);
            remove(".decrypt_now");
            DIR *d = opendir(STARTER_DIR);
            if (d) {
                struct dirent *dir;
                while ((dir = readdir(d)) != NULL) {
                    if (dir->d_type == DT_REG && is_base64_filename(dir->d_name)) {
                        char old_path[MAX_PATH_LEN], new_path[MAX_PATH_LEN];
                        snprintf(old_path, MAX_PATH_LEN, "%s/%s", STARTER_DIR, dir->d_name);
                        char *decoded = base64_decode(dir->d_name);
                        if (strlen(decoded) == 0) continue;
                        snprintf(new_path, MAX_PATH_LEN, "%s/%s", STARTER_DIR, decoded);
                        rename(old_path, new_path);
                    }
                }
                closedir(d);
                char msg[MAX_LOG_LEN];
                snprintf(msg, sizeof(msg), "Decryption triggered by user.");
                write_log(msg);
            }
        }
        sleep(5);
    }
}

void move_file(const char *from_dir, const char *to_dir, int logmove) {
    DIR *d = opendir(from_dir);
    if (!d) return;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_REG) {
            char old_path[MAX_PATH_LEN], new_path[MAX_PATH_LEN];
            snprintf(old_path, MAX_PATH_LEN, "%s/%s", from_dir, dir->d_name);
            snprintf(new_path, MAX_PATH_LEN, "%s/%s", to_dir, dir->d_name);

            if (strcmp(to_dir, QUARANTINE_DIR) == 0) {
                if (!is_encrypted(old_path))
                    encrypt_and_move(old_path, new_path);
                else
                    rename(old_path, new_path);
            } else {
                if (is_encrypted(old_path))
                    decrypt_and_move(old_path, new_path);
                else
                    rename(old_path, new_path);
            }

            if (logmove) {
                char msg[MAX_LOG_LEN];
                snprintf(msg, MAX_LOG_LEN, "%s - Successfully moved to %s directory.",
                         dir->d_name, strcmp(to_dir, QUARANTINE_DIR) == 0 ? "quarantine" : "starter kit");
                write_log(msg);
            }
        }
    }
    closedir(d);
}

void eradicate_files() {
    DIR *d = opendir(QUARANTINE_DIR);
    if (!d) return;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_REG) {
            char filepath[MAX_PATH_LEN], msg[MAX_LOG_LEN];
            snprintf(filepath, MAX_PATH_LEN, "%s/%s", QUARANTINE_DIR, dir->d_name);
            remove(filepath);
            snprintf(msg, MAX_LOG_LEN, "%s - Successfully deleted.", dir->d_name);
            write_log(msg);
        }
    }
    closedir(d);
}

void shutdown_daemon() {
    FILE *pidf = fopen(PID_FILE, "r");
    if (!pidf) {
        printf("Daemon not found.");
        return;
    }
    int pid;
    fscanf(pidf, "%d", &pid);
    fclose(pidf);
    if (kill(pid, SIGTERM) == 0) {
        char msg[MAX_LOG_LEN];
        snprintf(msg, MAX_LOG_LEN, "Successfully shut off decryption process with PID %d.", pid);
        write_log(msg);
        remove(PID_FILE);
    } else {
        printf("Failed to shutdown daemon.");
    }
}

void print_usage() {
    printf("Usage:\n");
    printf("  ./starterkit --decrypt\n");
    printf("  ./starterkit --quarantine\n");
    printf("  ./starterkit --return\n");
    printf("  ./starterkit --eradicate\n");
    printf("  ./starterkit --shutdown\n");
}

int main(int argc, char *argv[]) {
    daemon_loop();
    mkdir(STARTER_DIR, 0755);
    mkdir(QUARANTINE_DIR, 0755);

    if (argc != 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--decrypt") == 0) {
        FILE *f = fopen(".decrypt_now", "w");
        if (f) fclose(f);
    } else if (strcmp(argv[1], "--quarantine") == 0) {
        move_file(STARTER_DIR, QUARANTINE_DIR, 1);
    } else if (strcmp(argv[1], "--return") == 0) {
        move_file(QUARANTINE_DIR, STARTER_DIR, 1);
    } else if (strcmp(argv[1], "--eradicate") == 0) {
        eradicate_files();
    } else if (strcmp(argv[1], "--shutdown") == 0) {
        shutdown_daemon();
    } else {
        print_usage();
    }

    return 0;
}
