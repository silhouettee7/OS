#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#define MAX_PATH 1024
#define MAX_PATTERN 255
#define MAX_PROCS 100

typedef struct {
    char file[MAX_PATH];
    unsigned char pattern[MAX_PATTERN];
    int pattern_len;
    pid_t pid;
    int done;
} Task;

Task tasks[MAX_PROCS];
int running = 0;
int files_total = 0;
int max_procs;
unsigned char pattern[MAX_PATTERN];
int pattern_len;

void search_file(const char *filename, const unsigned char *pattern, int len) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("PID %d: Ошибка открытия %s: %s\n", getpid(), filename, strerror(errno));
        return;
    }
    
    unsigned char buf[8192];
    ssize_t read_bytes;
    off_t total = 0;
    int found = 0;
    
    printf("PID %d: Поиск в %s\n", getpid(), filename);
    
    while ((read_bytes = read(fd, buf, sizeof(buf))) > 0) {
        total += read_bytes;
        
        for (int i = 0; i <= read_bytes - len; i++) {
            int match = 1;
            for (int j = 0; j < len; j++) {
                if (buf[i + j] != pattern[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                found++;
                printf("PID %d: Совпадение в %s на позиции %ld\n",
                       getpid(), filename, total - read_bytes + i);
            }
        }
    }
    
    if (read_bytes < 0) {
        printf("PID %d: Ошибка чтения %s: %s\n", getpid(), filename, strerror(errno));
    }
    
    close(fd);
    
    printf("PID %d: %s обработан\n", getpid(), filename);
    printf("PID %d: Байт просмотрено: %ld\n", getpid(), total);
    printf("PID %d: Найдено: %d\n", getpid(), found);
    printf("PID %d: Завершение\n\n", getpid());
}

void child_job(const char *filename) {
    printf("Дочерний PID %d для файла: %s\n", getpid(), filename);
    search_file(filename, pattern, pattern_len);
    exit(0);
}

void wait_slot() {
    if (running >= max_procs) {
        int status;
        pid_t pid = wait(&status);
        
        for (int i = 0; i < files_total; i++) {
            if (tasks[i].pid == pid) {
                tasks[i].done = 1;
                printf("Главный: Процесс %d завершен\n", pid);
                running--;
                break;
            }
        }
    }
}

void process_file(const char *filepath) {
    wait_slot();
    
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork ошибка");
        return;
    }
    
    if (pid == 0) {
        child_job(filepath);
    } else {
        printf("Главный: Запущен %d для %s\n", pid, filepath);
        
        tasks[files_total].pid = pid;
        strncpy(tasks[files_total].file, filepath, MAX_PATH - 1);
        tasks[files_total].file[MAX_PATH - 1] = '\0';
        tasks[files_total].done = 0;
        files_total++;
        running++;
    }
}

void scan_directory(const char *dirname) {
    DIR *dir = opendir(dirname);
    if (!dir) {
        printf("Ошибка открытия %s: %s\n", dirname, strerror(errno));
        return;
    }
    
    struct dirent *entry;
    struct stat st;
    char fullpath[MAX_PATH];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirname, entry->d_name);
        
        if (stat(fullpath, &st) < 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            scan_directory(fullpath);
        } else if (S_ISREG(st.st_mode)) {
            printf("Главный: Файл %s (%ld байт)\n", fullpath, st.st_size);
            process_file(fullpath);
        }
    }
    
    closedir(dir);
}

int main(int argc, char *argv[]) {
    char directory[MAX_PATH];
    char input[MAX_PATTERN * 3];
    char proc_str[10];
    
    printf("Введите каталог: ");
    if (fgets(directory, sizeof(directory), stdin) == NULL) {
        printf("Ошибка ввода\n");
        return 1;
    }
    directory[strcspn(directory, "\n")] = 0;
    
    struct stat st;
    if (stat(directory, &st) < 0 || !S_ISDIR(st.st_mode)) {
        printf("Ошибка: %s не каталог\n", directory);
        return 1;
    }
    
    printf("Введите байты (hex): ");
    if (fgets(input, sizeof(input), stdin) == NULL) {
        printf("Ошибка ввода\n");
        return 1;
    }
    input[strcspn(input, "\n")] = 0;
    
    pattern_len = 0;
    char *ptr = input;
    while (*ptr != '\0' && pattern_len < MAX_PATTERN) {
        if (sscanf(ptr, "%2hhx", &pattern[pattern_len]) == 1) {
            pattern_len++;
            ptr += 2;
        } else {
            printf("Ошибка hex\n");
            return 1;
        }
    }
    
    if (pattern_len == 0) {
        printf("Паттерн пуст\n");
        return 1;
    }
    
    printf("Длина: %d байт\n", pattern_len);
    printf("Паттерн: ");
    for (int i = 0; i < pattern_len; i++) {
        printf("%02X ", pattern[i]);
    }
    printf("\n");
    
    printf("Введите макс процессов: ");
    if (fgets(proc_str, sizeof(proc_str), stdin) == NULL) {
        printf("Ошибка ввода\n");
        return 1;
    }
    max_procs = atoi(proc_str);
    if (max_procs <= 0 || max_procs > MAX_PROCS) {
        printf("Используется 100 по умолчанию\n");
        max_procs = 100;
    }

    printf("Макс процессов: %d\n\n", max_procs);
    
    scan_directory(directory);
    
    while (running > 0) {
        int status;
        pid_t pid = wait(&status);
        
        if (pid > 0) {
            for (int i = 0; i < files_total; i++) {
                if (tasks[i].pid == pid) {
                    tasks[i].done = 1;
                    running--;
                    break;
                }
            }
        }
    }

    printf("Обработано файлов: %d\n", files_total);
 
    return 0;
}