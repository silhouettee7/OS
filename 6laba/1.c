#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#define MAX_PROCESSES 1000

typedef struct {
    int pid;
    int ppid;
    char cmd[256];
} Process;

Process processes[MAX_PROCESSES];
int process_count = 0;

void find_all_descendants(int root_pid, int depth) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].ppid == root_pid) {
            for (int j = 0; j < depth; j++) printf("  ");
            printf("├─ PID=%d, PPID=%d\n", processes[i].pid, processes[i].ppid);
            find_all_descendants(processes[i].pid, depth + 1);
        }
    }
}

void print_time_and_pids(const char* process_name) {
    struct timeval tv;
    struct tm* tm_info;
    char time_buffer[26];
    char ms_buffer[10];
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", tm_info);
    snprintf(ms_buffer, sizeof(ms_buffer), ":%03ld", tv.tv_usec / 1000);
    
    printf("%s: PID = %d, PPID = %d, Время = %s%s\n",
           process_name, getpid(), getppid(), time_buffer, ms_buffer);
    fflush(stdout);
}

int main() {
    pid_t pid1, pid2;
    
    print_time_and_pids("Родительский процесс");
    
    pid1 = fork();
    
    if (pid1 < 0) {
        perror("Ошибка при первом вызове fork()");
        exit(1);
    }
    
    if (pid1 == 0) {
        print_time_and_pids("Дочерний процесс 1");
        exit(0);
    }
    
    pid2 = fork();
    
    if (pid2 < 0) {
        perror("Ошибка при втором вызове fork()");
        exit(1);
    }
    
    if (pid2 == 0) {
        print_time_and_pids("Дочерний процесс 2");
        exit(0);
    }

    FILE *fp = popen("ps -x -o pid,ppid,cmd --no-headers", "r");
    if (fp == NULL) {
        perror("Ошибка при выполнении ps");
        exit(1);
    }
    
    char line[512];
    process_count = 0;
    
    while (fgets(line, sizeof(line), fp) != NULL && process_count < MAX_PROCESSES) {
        sscanf(line, "%d %d %255[^\n]", 
               &processes[process_count].pid,
               &processes[process_count].ppid,
               processes[process_count].cmd);
        process_count++;
    }
    pclose(fp);
    
    printf("\nРезультат ps -x :\n");
    printf("Всего процессов в системе: %d\n", process_count);
    printf("\nДерево наших процессов:\n");
    
    printf("└─ PID=%d (корень)\n", getpid());
    find_all_descendants(getpid(), 1);
    
    wait(NULL);
    wait(NULL);
    
    return 0;
}