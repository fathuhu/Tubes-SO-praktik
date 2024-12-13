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

// Fungsi untuk menangani proses zombie
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Fungsi untuk mencatat log aktivitas server
void log_activity(const char *client_ip, int client_port, const char *message, pid_t pid) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return;
    }
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Hapus newline di akhir waktu

    fprintf(log_file, "[%s] PID %d - %s:%d - %s\n", time_str, pid, client_ip, client_port, message);
    fclose(log_file);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addr_len = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Buat socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Atur opsi socket untuk reuse address dan port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Binding
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listening
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d with PID %d...\n", PORT, getpid());

    // Tangani SIGCHLD untuk mencegah zombie process
    signal(SIGCHLD, handle_sigchld);

    // Loop untuk menerima koneksi klien
    while (1) {
        // Terima koneksi
        if ((client_fd = accept(server_fd, (struct sockaddr*)&address, &addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        printf("New connection from %s:%d\n",
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        log_activity(inet_ntoa(address.sin_addr), ntohs(address.sin_port), "Connected", getpid());

        // Fork proses untuk menangani klien
        pid_t pid = fork();
        if (pid == 0) {
            // Proses anak
            close(server_fd); // Tutup socket server di proses anak
            printf("Handling client %s:%d with child PID %d\n",
                   inet_ntoa(address.sin_addr), ntohs(address.sin_port), getpid());

            while (1) {
                memset(buffer, 0, BUFFER_SIZE);
                int bytes_read = read(client_fd, buffer, BUFFER_SIZE);

                // Jika klien memutus koneksi
                if (bytes_read <= 0) {
                    printf("Client %s:%d disconnected (PID %d)\n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port), getpid());
                    log_activity(inet_ntoa(address.sin_addr), ntohs(address.sin_port), "Disconnected", getpid());
                    break;
                }

                printf("Received from %s:%d (PID %d) - %s",
                       inet_ntoa(address.sin_addr), ntohs(address.sin_port), getpid(), buffer);
                log_activity(inet_ntoa(address.sin_addr), ntohs(address.sin_port), buffer, getpid());

                // Kirim kembali pesan (echo)
                send(client_fd, buffer, strlen(buffer), 0);
            }
            close(client_fd);
            exit(0);
        } else if (pid > 0) {
            // Proses induk
            printf("Created child process with PID %d\n", pid);
            close(client_fd); // Tutup socket klien di proses induk
        } else {
            perror("Fork failed");
        }
    }

    close(server_fd);
    return 0;
}
