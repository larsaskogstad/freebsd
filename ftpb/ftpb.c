#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

void get_current_date(char *date_str, size_t max_size) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(date_str, max_size, "%02d-%02d-%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
}

void count_files(const char *directory, int *file_count) {
    DIR *dir;
    struct dirent *entry;
    struct stat info;

    if (!(dir = opendir(directory))) return;

    while ((entry = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            count_files(path, file_count);
        } else {
            stat(path, &info);
            if (S_ISREG(info.st_mode)) (*file_count)++;
        }
    }
    closedir(dir);
}

long get_directory_size(const char *directory) {
    long total_size = 0;
    DIR *dir;
    struct dirent *entry;
    struct stat info;

    if (!(dir = opendir(directory))) return -1;

    while ((entry = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            total_size += get_directory_size(path);
        } else {
            if (stat(path, &info) == 0) {
                total_size += info.st_size;
            }
        }
    }
    closedir(dir);
    return total_size;
}

int create_tar_with_progress(const char *directory, const char *tar_filename) {
    int file_count = 0;
    count_files(directory, &file_count);
    if (file_count == 0) return -1;

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // In child process, redirect stdout to pipe
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);

        execlp("tar", "tar", "--exclude", tar_filename, "-czvf", tar_filename, "-C", directory, ".", NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        // In parent process, read progress from pipe
        close(pipe_fd[1]);
        FILE *pipe_stream = fdopen(pipe_fd[0], "r");
        if (!pipe_stream) {
            perror("fdopen");
            close(pipe_fd[0]);
            return -1;
        }

        char buffer[1024];
        int files_processed = 0;
        while (fgets(buffer, sizeof(buffer), pipe_stream) != NULL) {
            if (strstr(buffer, "tar: ./") != NULL) {
                files_processed++;
                printf("\rProgress: %.2f%%", (files_processed / (float)file_count) * 100);
                fflush(stdout);
            }
        }
        fclose(pipe_stream);
        printf("\n");

        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            long tar_size = get_directory_size(directory);
            printf("%s size: %ld bytes\n\n", tar_filename, tar_size);
            return 0;
        } else {
            fprintf(stderr, "Error: tar command failed\n");
            return -1;
        }
    }
}

size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
    return size * nmemb;
}

int upload_file_to_ftp(const char *ftp_url, const char *username, const char *password, const char *local_file) {
    CURL *curl;
    CURLcode res;
    FILE *file = fopen(local_file, "rb");

    if (!file) {
        fprintf(stderr, "Error: Could not open file %s for reading\n", local_file);
        return -1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: Could not initialize CURL\n");
        fclose(file);
        return -1;
    }

    char userpwd[256];
    snprintf(userpwd, sizeof(userpwd), "%s:%s", username, password);

    curl_easy_setopt(curl, CURLOPT_URL, ftp_url);
    curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR_RETRY);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    res = curl_easy_perform(curl);
    fclose(file);

    if (res != CURLE_OK) {
        fprintf(stderr, "Error: FTP upload failed: %s\n", curl_easy_strerror(res));
    } else {
        printf("%s uploaded to %s\n", local_file, ftp_url);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return res == CURLE_OK ? 0 : -1;
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <directory> <ftp_server> <port> <username> <password>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *directory = argv[1];
    char *ftp_server = argv[2];
    char *port = argv[3];
    char *username = argv[4];
    char *password = argv[5];

    char date_str[11];
    get_current_date(date_str, sizeof(date_str));

    char tar_filename[256];
    snprintf(tar_filename, sizeof(tar_filename), "%s-backup.tar.gz", date_str);

    printf("Creating tar.gz archive from directory: %s\n", directory);
    if (create_tar_with_progress(directory, tar_filename) != 0) {
        fprintf(stderr, "Error: Failed to create tar.gz file\n");
        return EXIT_FAILURE;
    }

    char ftp_url[512];
    snprintf(ftp_url, sizeof(ftp_url), "ftp://%s:%s/%s", ftp_server, port, tar_filename);

    printf("Uploading to FTP server: %s\n", ftp_server);
    printf("Port: %s\n", port);
    printf("Username: %s\n", username);
    // Do not print the password for security reasons
    // printf("Password: %s\n", password);

    if (upload_file_to_ftp(ftp_url, username, password, tar_filename) != 0) {
        fprintf(stderr, "Error: Failed to upload file to FTP server\n");
        return EXIT_FAILURE;
    }

    printf("Backup and upload completed successfully \n");
    return EXIT_SUCCESS;
}
