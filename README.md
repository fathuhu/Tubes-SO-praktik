
# Server TCP Sederhana dengan Logging

## Gambaran Umum
Tugas besar ini adalah implementasi server TCP sederhana yang dapat menerima banyak koneksi klien, mencatat aktivitas mereka, dan menyediakan fungsi echo dasar. Server berjalan pada port 8080 dan membuat proses anak untuk menangani setiap klien secara independen.


## Authors

- [@fathuhu](https://www.github.com/fathuhu)

Kode di atas adalah implementasi server TCP sederhana dalam C yang menggunakan model *multi-process* untuk menangani banyak klien secara bersamaan. Berikut adalah penjelasan dari setiap bagian kode:

---

### 1. **Header dan Konstanta**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#define PORT 8080
#define BUFFER_SIZE 1024
#define LOG_FILE "server_log.txt"
```
- **Header File**:
  - `stdio.h`, `stdlib.h`, `string.h`: Untuk operasi I/O dasar dan manipulasi string.
  - `unistd.h`: Untuk fungsi sistem Unix seperti `fork` dan `close`.
  - `sys/socket.h`, `netinet/in.h`, `arpa/inet.h`: Untuk operasi jaringan, termasuk pembuatan socket.
  - `signal.h`: Untuk menangani sinyal (seperti SIGCHLD).
  - `fcntl.h`: Untuk operasi kontrol file (tidak digunakan di kode ini).
  - `time.h`: Untuk mencatat waktu log aktivitas.
- **Konstanta**:
  - `PORT`: Port tempat server akan mendengarkan koneksi.
  - `BUFFER_SIZE`: Ukuran buffer untuk membaca/mengirim data.
  - `LOG_FILE`: Nama file tempat log aktivitas disimpan.

---

### 2. **Fungsi untuk Menangani Zombie Process**
```c
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
```
- Menangani sinyal `SIGCHLD` untuk mencegah *zombie process* (proses anak yang telah selesai tetapi masih tercatat dalam tabel proses).
- `waitpid(-1, NULL, WNOHANG)` digunakan untuk membersihkan semua proses anak tanpa memblokir proses induk.

---

### 3. **Fungsi untuk Log Aktivitas**
```c
void log_activity(const char *client_ip, int client_port, const char *message) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return;
    }
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Hapus newline di akhir waktu

    fprintf(log_file, "[%s] %s:%d - %s\n", time_str, client_ip, client_port, message);
    fclose(log_file);
}
```
- Membuka file log (`LOG_FILE`) dalam mode *append*.
- Menulis log dengan format: `[waktu] IP:Port - Pesan`.
- Waktu diperoleh dari `time` dan diformat menggunakan `ctime`.

---

### 4. **Pembuatan dan Konfigurasi Server**
```c
if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Socket failed");
    exit(EXIT_FAILURE);
}
if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt failed");
    close(server_fd);
    exit(EXIT_FAILURE);
}
address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;
address.sin_port = htons(PORT);

if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
}
if (listen(server_fd, 10) < 0) {
    perror("Listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
}
```
- Membuat socket (`socket`).
- Mengatur opsi *reuse* untuk alamat dan port menggunakan `setsockopt`.
- Mengikat (`bind`) socket ke alamat IP dan port (`INADDR_ANY` untuk semua antarmuka).
- Memulai server untuk mendengarkan koneksi dengan antrian maksimal 10 koneksi.

---

### 5. **Loop Utama untuk Menerima dan Menangani Klien**
```c
while (1) {
    if ((client_fd = accept(server_fd, (struct sockaddr*)&address, &addr_len)) < 0) {
        perror("Accept failed");
        continue;
    }
    printf("New connection from %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    log_activity(inet_ntoa(address.sin_addr), ntohs(address.sin_port), "Connected");

    pid_t pid = fork();
    if (pid == 0) {
        close(server_fd);
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_read = read(client_fd, buffer, BUFFER_SIZE);
            if (bytes_read <= 0) {
                printf("Client %s:%d disconnected\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                log_activity(inet_ntoa(address.sin_addr), ntohs(address.sin_port), "Disconnected");
                break;
            }
            printf("Received from %s:%d - %s", inet_ntoa(address.sin_addr), ntohs(address.sin_port), buffer);
            log_activity(inet_ntoa(address.sin_addr), ntohs(address.sin_port), buffer);
            send(client_fd, buffer, strlen(buffer), 0);
        }
        close(client_fd);
        exit(0);
    } else if (pid > 0) {
        close(client_fd);
    } else {
        perror("Fork failed");
    }
}
```
- **Accept**:
  - Menerima koneksi klien dan mencatat alamat IP dan port.
- **Fork**:
  - Proses *fork* dibuat untuk menangani setiap klien. Proses anak menangani komunikasi, sementara proses induk tetap mendengarkan klien baru.
- **Proses Anak**:
  - Membaca data dari klien menggunakan `read`.
  - Jika klien memutus koneksi, loop berhenti, dan klien ditutup.
  - Mengirim kembali data yang diterima ke klien (*echo server*).
- **Proses Induk**:
  - Menutup socket klien untuk melepaskan sumber daya.

---

### 6. **Penanganan Koneksi**
- Proses anak menangani satu klien secara independen.
- Zombie process dicegah dengan menangani `SIGCHLD`.
- Aktivitas dicatat di file log.

---

### 7. **Penutupan**
```c
close(server_fd);
return 0;
```
Socket server ditutup saat program berakhir.
## Tech Stack

**Client:** Netcat (nc), Telnet, atau alat berbasis terminal lainnya untuk mengirim dan menerima data melalui koneksi TCP.  
**Server:** C (menggunakan pustaka seperti `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>` untuk pengelolaan koneksi jaringan TCP).  
**Debugging dan Profiling Tools:**  
- **Valgrind**: Untuk mendeteksi kebocoran memori dan memastikan penggunaan memori yang benar.  
- **strace**: Untuk melacak panggilan sistem (*system calls*) dan sinyal yang terjadi selama eksekusi program.

