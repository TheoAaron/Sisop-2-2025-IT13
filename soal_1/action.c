#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>

#define ZIP_NAME "Clues.zip"
#define ZIP_URL "https://drive.google.com/uc?export=download&id=1xFn1OBJUuSdnApDseEczKhtNzyGekauK"

void run_exec(char *cmd, char *argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd, argv);
        perror("exec failed");
        exit(EXIT_FAILURE);
    } else {
        wait(NULL);
    }
}

void download_and_extract() {
    struct stat st = {0};

    if (stat("Clues", &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("📂 Folder 'Clues' sudah ada, skip download.\n");
        return;
    }

    printf("⬇️  Mengunduh Clues.zip...\n");
    char *wget_args[] = {"wget", "-q", "--show-progress", "-O", ZIP_NAME, ZIP_URL, NULL};
    run_exec("wget", wget_args);

    printf("📦 Mengekstrak Clues.zip...\n");
    char *unzip_args[] = {"unzip", "-o", ZIP_NAME, NULL};
    run_exec("unzip", unzip_args);

    unlink(ZIP_NAME);
}

int is_valid_file(const char *filename) {
    return strlen(filename) == 5 &&
           isalnum(filename[0]) &&
           strcmp(filename + 1, ".txt") == 0;
}

void move_file(const char *src, const char *dest_dir) {
    char dest_path[512];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, strrchr(src, '/') + 1);
    rename(src, dest_path);
}

void filter_files() {
    mkdir("Filtered", 0755);

    const char *dirs[] = {"Clues/ClueA", "Clues/ClueB", "Clues/ClueC", "Clues/ClueD"};

    for (int i = 0; i < 4; ++i) {
        DIR *dir = opendir(dirs[i]);
        if (!dir) continue;

        struct dirent *entry;
        char filepath[512];

        while ((entry = readdir(dir))) {
            if (entry->d_type == DT_REG) {
                snprintf(filepath, sizeof(filepath), "%s/%s", dirs[i], entry->d_name);
                if (is_valid_file(entry->d_name)) {
                    move_file(filepath, "Filtered");
                } else {
                    unlink(filepath);
                }
            }
        }
        closedir(dir);
    }

    printf("✅ File valid dipindahkan ke folder Filtered/.\n");
}

int cmp_name(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

int is_digit_file(const char *filename) {
    return filename[0] >= '0' && filename[0] <= '9';
}

int is_alpha_file(const char *filename) {
    return (filename[0] >= 'a' && filename[0] <= 'z') || (filename[0] >= 'A' && filename[0] <= 'Z');
}

void combine_files() {
    DIR *dir = opendir("Filtered");
    if (!dir) {
        printf("❌ Folder Filtered tidak ditemukan.\n");
        return;
    }

    struct dirent *entry;
    char *digit_files[100], *alpha_files[100];
    int dcount = 0, acount = 0;

    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG && is_valid_file(entry->d_name)) {
            if (is_digit_file(entry->d_name)) digit_files[dcount++] = strdup(entry->d_name);
            else if (is_alpha_file(entry->d_name)) alpha_files[acount++] = strdup(entry->d_name);
        }
    }
    closedir(dir);

    qsort(digit_files, dcount, sizeof(char *), cmp_name);
    qsort(alpha_files, acount, sizeof(char *), cmp_name);

    FILE *out = fopen("Combined.txt", "w");
    if (!out) {
        perror("❌ Gagal membuat Combined.txt\n");
        return;
    }

    int i = 0, j = 0;
    while (i < dcount || j < acount) {
        if (i < dcount) {
            char path[512];
            snprintf(path, sizeof(path), "Filtered/%s", digit_files[i]);
            FILE *f = fopen(path, "r");
            if (f) {
                char ch;
                while ((ch = fgetc(f)) != EOF) fputc(ch, out);
                fclose(f);
                unlink(path);
            }
            free(digit_files[i]);
            i++;
        }
        if (j < acount) {
            char path[512];
            snprintf(path, sizeof(path), "Filtered/%s", alpha_files[j]);
            FILE *f = fopen(path, "r");
            if (f) {
                char ch;
                while ((ch = fgetc(f)) != EOF) fputc(ch, out);
                fclose(f);
                unlink(path);
            }
            free(alpha_files[j]);
            j++;
        }
    }

    fclose(out);
    printf("✅ Combined.txt berhasil dibuat dan file .txt dihapus.\n");
}

char rot13_char(char c) {
    if ('a' <= c && c <= 'z') return 'a' + (c - 'a' + 13) % 26;
    if ('A' <= c && c <= 'Z') return 'A' + (c - 'A' + 13) % 26;
    return c;
}

void decode_file() {
    FILE *in = fopen("Combined.txt", "r");
    FILE *out = fopen("Decoded.txt", "w");
    if (!in || !out) {
        printf("❌ Combined.txt tidak ditemukan.\n");
        return;
    }

    char ch;
    while ((ch = fgetc(in)) != EOF) {
        fputc(rot13_char(ch), out);
    }

    fclose(in);
    fclose(out);
    printf("✅ Decode selesai. Lihat hasilnya di Decoded.txt\n");
}

void show_usage() {
    printf("📘 Penggunaan:");
    printf("./action                # Download dan ekstrak Clues.zip\n");
    printf("./action -m Filter      # Filter file .txt valid ke folder Filtered/\n");
    printf("./action -m Combine     # Gabung isi file .txt ke Combined.txt\n");
    printf("./action -m Decode      # Decode ROT13 ke Decoded.txt\n");
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        download_and_extract();
    } else if (argc == 3 && strcmp(argv[1], "-m") == 0) {
        if (strcmp(argv[2], "Filter") == 0) filter_files();
        else if (strcmp(argv[2], "Combine") == 0) combine_files();
        else if (strcmp(argv[2], "Decode") == 0) decode_file();
        else show_usage();
    } else {
        show_usage();
    }
    return 0;
}
