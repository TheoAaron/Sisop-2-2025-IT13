# Sisop Modul 2

# Soal 1

## Deskripsi Program
Pada praktikum ini, saya disuruh untuk membuat sebuah sistem menggunakkan bahasa C yang berbasis command-line yang dirancang untuk mengunduh, memfilter, menggabungkan, dan mendekode file teks dari sebuah arsip ZIP. Program ini disuruh untuk mengfilter file normal dengan file-file yang memiliki format nama spesifik (contoh: `a.txt`, `1.txt`).

## Fungsi Utama
1. **Download dan Ekstraksi**: Mengunduh file ZIP dari Google Drive dan mengekstraknya ke folder `Clues`.
    ```
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
    ```
    Pada kode tersebut, setiap kali kode dirun, maka akan mengecek apakah folder "Clues" sudah terdownload atau belum. Jika sudah, maka tidak akan terdownload lagi. Jika belum, maka akan membuka link menuju file yang telah disediakan lalu mendownload, unzip, dan menghapus file zip yang terdapat dalam link teresbut.
    
2. **Filter File**: Memfilter file-file yang valid (berformat `[a-z,A-Z,0-9].txt`) ke folder `Filtered`.
    ```
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
    ```
    Pada kode tersebut, saya pertama-tama membuat direktori bernama "Filtered" untuk menyimpan file yang sudah difilter. Cara saya melakukan        search pada file adalah dengan cara mengloop melalui setiap direktori yang ada dan membaca tiap file yang ada dalam direktori tersebut. 
    Filter dapat dilakukan dengan memmanggil fungsi untuk memerika file apakah nama file valid dengan format yang telah diberikan (a-z,A-Z,0-9).
    Jika valid, maka dipindahkan ke folder "Filtered". Jika tidak valid, maka akan dihapus.
   
4. **Penggabungan File**: Menggabungkan isi file-file teks yang telah difilter ke dalam satu file `Combined.txt`.
    ```
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
    ```
    Pada kode ini, pertama-tama, saya membuka direktori "Filtered" lalu membuat sebuah variabel untuk menyimpan file yang namanya diawali dengan angka dan huruf, dan membuat counter untuk masing masing filenya. Lalu saya membaca file dalam direktori dan menyimpan isinya dalam memori heap. Pengurutan dilakukan menggunakan qsort untuk masing-masing huruf dan angka. Kemudian, penggabungannya dapat dilakukan dengan cara memasukkan file angka dulu lalu file huruf secara berulang sampai tidak ada lagi file yang tersisa.

5. **Dekode ROT13**: Mendekode isi file `Combined.txt` menggunakan algoritma ROT13 dan menyimpan hasilnya ke `Decoded.txt`.
    ```
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
    ```
Pada kode ini, saya sisa membuka file Combined.txt yang berisi sebuah string gabungan yang telah digabungkan sebelumnya dan menggunakan ROT13 untuk mencipher isi dari file tersebut dan memasukkannya ke dalam file bernama "Decoded.txt" yang merupakan jawaban dari problem ini.

### Fungsi Pendukung
- **`run_exec()`**: Menjalankan perintah eksternal dengan `fork()` dan `execvp()`.
  ```
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
  ```
- **`is_valid_file()`**: Memeriksa validitas nama file.
  ```
  int is_valid_file(const char *filename) {
      return strlen(filename) == 5 &&
           isalnum(filename[0]) &&
           strcmp(filename + 1, ".txt") == 0;
  }
  ```
- **`move_file()`**: Memindahkan file ke direktori tujuan.
  ```
  void move_file(const char *src, const char *dest_dir) {
      char dest_path[512];
      snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, strrchr(src, '/') + 1);
      rename(src, dest_path);
  }
  ```
- **`rot13_char()`**: Mendekode karakter menggunakan ROT13.
  ```
  char rot13_char(char c) {
      if ('a' <= c && c <= 'z') return 'a' + (c - 'a' + 13) % 26;
      if ('A' <= c && c <= 'Z') return 'A' + (c - 'A' + 13) % 26;
      return c;
  }
  ```

### Cara Penggunaan
Program dapat dijalankan dengan opsi berikut:
```bash
./action                # Download dan ekstrak Clues.zip
./action -m Filter      # Filter file .txt valid ke folder Filtered/
./action -m Combine     # Gabung isi file .txt ke Combined.txt
./action -m Decode      # Decode ROT13 ke Decoded.txt
```

# Soal 2

## ⚙️ Fungsi Utama <a name="fungsi-utama-starterkit"></a>

### 1. `daemon_loop()`
```
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
```
Pada kode ini, saya pertama-tama melakukan proses forking untuk proses daemon. Jika sukses, maka menyimpan PID daemon ke `/tmp/starterkit.pid`. Lalu saya menggunakan teknik double-fork untuk detach dari terminal dan melakukan loop infinite pada daemon. Lalu dalam loop saya membuat sebuah temporary file dan mengecek keberadaan file tersebut setiap 5 detik untuk mengecek apakah file valid atau tidak. Kemudian saya mencatat setiap aktivitas yang terjadi ke activity.log

### 2. `encrypt_and_move()` dan `decrypt_and_move()`
```
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
```
Pada kode ini, saya ingin mendekripsi file yang terenkripsi. Hal ini dapat dilakukan dengan pertama mebuat sebuah file sumber untuk menyimpan file yang terenkripsi dan sebuah file tujuan untuk menaruh hasil dari file yang telah di dekripsi. Pertama-tama saya membuka file sumber dalam mode baca biner, dan membuat file tujuan dalam mode biner. Jika salah satu gagal terbuka, maka fungsi langsung berhenti.

Pada proses enkripsi, saya pertama-tama menambahkan header enkripsi sebagai penanda bahwa file tersebut terenkripsi. Lalu, saya membaca file sumber per byte (`fgetc(src)`) dan saya mengenkripsi setiap byte dengan operasi XOR terhadap `0xAA` hingga akhir dari file tersebut. Kemudian, saya menutup kedua file dan menghapus file sumbernya (`remove(src_path)`). 
```
Contoh Enkripsi:
    Byte asli: 01000001 (huruf 'A')
    XOR dengan 0xAA:
    01000001 ^ 10101010 = 11101011 (hasil enkripsi).
```
Pada proses dekripsi, saya pertama-tama membuka file sumber, tetapi file sumber kali ini sudah terenkripsi. Lalu, saya membaca file sumber per byte setelah header (`ENCRYPTED` yang hanya digunakan sebagai penanda) dan mendekripsi setiap byte dengan operasi XOR terhadap `0xAA`. Kemudian, saya menulis hasil dekripsi ke file tujuan yang telah didekripsi lalu menutup kedua file dan menghaps file sumbernya.
```
Contoh Dekripsi:
    Byte terenkripsi: 11101011
    XOR dengan 0xAA:
    11101011 ^ 10101010 = 01000001 (huruf 'A' asli).
```

### 3. `move_file()`
```
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
```
Pada kode ini, saya membuat sebuah program untuk memindahkan file dari suatu lokasi tertentu ke lokasi lainnya. Terdapat beberapa parameter input pada program tersebut:
```
Parameter Input
    from_dir: Direktori sumber file.
    to_dir: Direktori tujuan file.
    logmove: Flag untuk menentukan apakah perlu mencatat log (1 = ya, 0 = tidak).
```
Pertama-tama, saya membuka direktori sumber dan mengiterasikan tiap file dalam direktori tersebut lalu membuat path sumber dan tujuannya. Program ini digunakan dalam dua fitur dalam keseluruhan program ini, yaitu ketika user ingin melakukan aksi `quarantine` dan `return`. Jika user memasukkan input quarantine, maka akan mengecek apakah sudah terenkripsi atau belum, jika belum, maka akan memanggil fungsi `encrypt_and_move()`, dan jika sudah, cukup pindahkan dengan `rename()`. Jika user memasukkan input return, maka akan mengecek file apakah terenkripsi atau belum, jika sudah, maka akan memanggil fungsi `decrypt_and_move()`, dan jika tidak, maka cukup dipindahkan menggunakan `rename()`. Semua kativitas ini akan dicatat pada activity.log.

### 4. `eradicate_files()`
```
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
```
Pada kode ini, saya membuat sebuah program untuk menghapus secara permanen semua file yang berada dalam foldere "quarantine". Pertama, kita harus membuka folder quarantine itu sendiri dan mengiterasikan tiap file dalam folder tersebut. Lalu, kita akan membentuk suatu file path dan menghapus semua file tersebut secara permanen. Kemudian, kita akan mencatatnya ke dalam activity.log.

### 5. `write_log()`
```
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
```
Pada kode ini, saya membuat sebuah file untuk menyimpan semua aktivitas program dalam sebuah file bernama "activity.log". Pertama, saya membuat file bernama "activity.log" dan membuka file tersebut. Lalu, saya menyimpan data waktu secara langsung dalam variabel now menggunakan function `time()` dan `localtime()`. Saya juga membuat sebuah varibel kosong dan memisahkan tiap-tiap elemen dari waktu (tahun, bulan, tanggal. jam, menit, dan detik) dan menyimpannya dalam variabel "timestamp". Kemudian, saya sisa menyesuaikan format waktu dan memprint berdasarkan format yang benar.

### 6. `shutdown_daemon()`
```
void shutdown_daemon() {
    FILE *pidf = fopen(PID_FILE, "r");
    if (!pidf) {
        printf("Daemon not running.\n");
        return;
    }
    
    int pid;
    if (fscanf(pidf, "%d", &pid) != 1) {
        fclose(pidf);
        printf("Invalid PID file.\n");
        return;
    }
    fclose(pidf);
    
    if (kill(pid, SIGTERM) == 0) {
        char msg[MAX_LOG_LEN];
        snprintf(msg, MAX_LOG_LEN, "Successfully shut off decryption process with PID %d.", pid);
        write_log(msg);
        remove(PID_FILE);
        printf("Daemon (PID %d) shutdown successfully.\n", pid);
    } else {
        printf("Failed to shutdown daemon (PID %d). Error: %s\n", pid, strerror(errno));
    }
}
```
Pada kode ini, saya membuat sebuah program untuk melakukan shutdown pada program daemon yang sedang berjalan dan membersihkan file PID yang tersimpan. Pertama, kita akan membaca file PID pada program kita (`/tmp/starterkit.pid`). Jika tidak terbuka, maka artinya program daemon sedang tidak berjalan dan akan mengeluarkan pesan daemon tidak berjalan. Lalu, program akan melakukan parsing PID untuk mengecek apakah format PID benar atau salah. Jika salah, akan mengeluarkan pesan error. Kemudian, program akan mengirimkan sinyal shutdown (SIGTERM) ke proses daemon. Jika berhasil, program akan menghapus file PID, menampilkan pesan sukses, dan mencatatnya ke activity.log. Jika tidak, maka akan mengeluarkan pesan eror.

## Fungsi Tambahan

### 1. `Download_and_extract()`
```
void download_and_extract() {
    struct stat st = {0};

    if (stat("starter_kit", &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("📂 Folder 'starter_kit' sudah ada, skip download.\n");
        return;
    }

    printf("⬇️  Mengunduh Clues.zip...\n");
    char *wget_args[] = {"wget", "-q", "--show-progress", "-O", ZIP_NAME, ZIP_URL, NULL};
    run_exec("wget", wget_args);

    printf("📦 Mengekstrak Clues.zip ke folder starter_kit...\n");
    mkdir("starter_kit", 0755);
    char *unzip_args[] = {"unzip", "-o", ZIP_NAME, "-d", "starter_kit", NULL};
    run_exec("unzip", unzip_args);

    printf("❌ Menghapus file zip...\n");
    if (remove(ZIP_NAME) == 0) {
        printf("🟢 Zip file berhasil dihapus.\n");
    } else {
        perror("🛑 Gagal menghapus file zip");
    }
}
```
Pada kode ini, sama seperti dengan kode pada nomor 1, ketika file dirun, akan melakukan pengecekan terhadap folder "starter_kit". Jika ada, maka akan melewati proses download dan memberitahu user bahwa folder sudah pernah didownload. Jika tidak ada, maka akan melakukan download dari file yang telah disiapkan, unzip file yang didownload, memasukkan file yang telah diunsip ke dalam folder bernama "starter_kit" dan menghapus file zip awal.

### 2. `base64_decode()`
```
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
```
Kode ini menggunakan sebuah function dalam sistem `/usr/bin/base64` untuk mendekode file yang terenkripsi.

## REVISI
Terdapat 2 kesalahan yang terdapat ketika demo, yaitu:
* starterkit tidak melakukan download zip ketika program dijalankan
* starterkit shutdown bener2 shutdown process

## Solusi

- Untuk solusi download file zip, saya sudah menambahkan kode untuk mendownload zip secara langsung dan melakukan unzip hingga menghapus file zip awal, yaitu sebagai berikut:
```
void download_and_extract() {
    struct stat st = {0};

    if (stat("starter_kit", &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("📂 Folder 'starter_kit' sudah ada, skip download.\n");
        return;
    }

    printf("⬇️  Mengunduh Clues.zip...\n");
    char *wget_args[] = {"wget", "-q", "--show-progress", "-O", ZIP_NAME, ZIP_URL, NULL};
    run_exec("wget", wget_args);

    printf("📦 Mengekstrak Clues.zip ke folder starter_kit...\n");
    mkdir("starter_kit", 0755);
    char *unzip_args[] = {"unzip", "-o", ZIP_NAME, "-d", "starter_kit", NULL};
    run_exec("unzip", unzip_args);

    printf("❌ Menghapus file zip...\n");
    if (remove(ZIP_NAME) == 0) {
        printf("🟢 Zip file berhasil dihapus.\n");
    } else {
        perror("🛑 Gagal menghapus file zip");
    }
}
```

- Untuk solusi kesalahan kedua, ternyata path file pid yang dideklarasikan tidak sesuai dengan yang dipakai di fungsi lain.

Pada fungsi `daemon_loop()`:
```
FILE *pidf = fopen(PID_FILE, "w"); // <--- PID_FILE = ".pid"
```
Sedangkan pada fungsi `shutdown_daemon()`:
```
FILE *fp = fopen("/tmp/starterkit.pid", "r"); // <--- ❌ Mismatch
```

- Solusinya sisa mengubah kode define diatas menjadi
```
#define PID_FILE "/tmp/starterkit.pid"
```
Sehingga, outputnya ketika kita melakukan `ps aux | grep starterkit` untuk mengecek keaktifan program berubah dari:

![Screenshot 2025-04-17 222118](https://github.com/user-attachments/assets/bbcdd3ee-f114-4b1b-a562-b8fab7180ade)

menjadi:

![Screenshot 2025-04-18 164058](https://github.com/user-attachments/assets/c255227c-8e95-4fea-88fd-4705a2ef9fd3)

# Soal 3
![Image](https://github.com/user-attachments/assets/1921a38f-d315-4801-ae06-188992c9c04a)

kode di atas menjelaskan fungsi generate hash, untuk set random hash hexadecimal

![Image](https://github.com/user-attachments/assets/2ae357ef-eff4-4cba-9540-96e217efc570)

untuk kode di atas menjelaskan fungsi proses miner, dengan mengatur nama miner secara dinamis terlebih dahulu. Kemudian melakukan while loop true dengan set random waktu untuk sleep antara 3 - 30 detik. Langkah berikutnya memanggil fungsi generate hash. dalam fungsi tersebut juga mengambil waktu saat ini yang disimpan dalam string timestamp, lalu akan set nama dinamis dan menulis ke log file.

![Image](https://github.com/user-attachments/assets/93ab8095-c05d-4f56-82c4-7425b4a03929)

kode di atas merupakan fungsi untuk menyebarkan binary nya dengan nama malware. Melakukan while loop dengan kondisi `while ((entry = readdir(dir)) != NULL)` untuk membaca dir. setelah itu, fungsi while loop tsb akan melewati direktori . dan .. agar tidak terkena penyebaran biner nya sendiri. `snprintf` untuk set target path dalam penyebaran biner. Lalu melakukan penyalinan dengan memanggil `cp`.

![Image](https://github.com/user-attachments/assets/3c503dec-fe78-4463-a05c-bf1bc022a9fb)

kode di atas untuk fungsi encrypt file. Fungsi tsb diawali dengan membaca file tersebut dengan `fopen`. lalu melakukan pengukuran file. `buffer[i] ^= XOR_KEY;` digunakan untuk mengubah buffer menjadi encrypt dengan XOR KEY. `fwrite` digunakan untuk menulis encrypt ke file sebelumnya.

![Image](https://github.com/user-attachments/assets/e855955c-d449-44a7-99b5-65c83f8ca8de)

kode di atas untuk fungsi proses direktori dalam proses encrypt file. Langkah pertama nya yaitu set fullpath nya. dan cek apakah fullpath merupakan direktori atau file, jika direktori maka akan running `process_directory(full_path);` apabila file maka akan running `encrypt_file(full_path);`

`void wannacryptor_process()` kode disamping untuk set mulai direktori sekarang dalam encrypt file.

`int main(int argc, char *argv[])` masuk ke main process dengan parameter untuk mengganti nama anak proses.

![Image](https://github.com/user-attachments/assets/ffeb512f-15eb-4892-8601-64f65954876c)

kode di atas untuk membuat parent process

![Image](https://github.com/user-attachments/assets/e564eb96-bb2d-4eba-8161-19ef18c863f5)

kode di atas untuk memanggil fungsi wannacryptor dengan menamainya menjadi wannacryptor proses di daemon.

![Image](https://github.com/user-attachments/assets/e7ef3988-1779-4241-8e37-4599ce008f42)

kode di atas untuk memanggil fungsi trojan(menyebarkan biner).

![Image](https://github.com/user-attachments/assets/9586fa1d-edda-48b8-9a5d-f6f40c854a5c)

membuat anak proses dengan nama rodok.exe

![Image](https://github.com/user-attachments/assets/68331ec4-336b-4ccd-aab9-3897b558341f)

membuat anak proses dari rodok.exe dengan nama dinamis miner proses. Kemudian memanggil fungsi mining.

##REVISI
mohon maaf mas sebelumnya kode saya yang menyebarkan biner itu bisa, tetapi kemarin waktu demo itu ngga bisa karena linux saya kena malware saya sendiri. 

![Image](https://github.com/user-attachments/assets/e7f5b45b-b7ae-4e46-a507-04cdd7963475)

buktinya seperti di atas mas

# Soal 4
