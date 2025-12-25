#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

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
    pid_t parent_pid = getpid();
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
    
    if (pid1 > 0){
        pid2 = fork();
        if (pid2 < 0) {
            perror("Ошибка при втором вызове fork()");
            exit(1);
        }
        
        if (pid2 == 0) {
            print_time_and_pids("Дочерний процесс 2");
            exit(0);
        }
    }
    if (pid1 > 0 && pid2 > 0)
    {
        sleep(1);
        char cmd[512];
        sprintf(cmd, 
            "ps -x -o pid,ppid,command | "
            "grep -E \"^ *%d |[[:space:]]+%d[[:space:]]\" | "
            "grep -v grep", 
            parent_pid, parent_pid);
        system(cmd);
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);

        return 0;
    } 
    return 0;
}