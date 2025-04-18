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


# Soal 3


# Soal 4
