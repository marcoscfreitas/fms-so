#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <pthread.h>
#include <signal.h>

pid_t child_pid;
int finished = 0;

void* monitor(void* arg) {
    int timeout = *(int*)arg;

    sleep(timeout);

    if (!finished) {
        printf("\nTimeout atingido! Matando processo...\n");
        fflush(stdout);
        kill(child_pid, SIGKILL);
    }

    return NULL;
}

int main() {
    char program[256];
    int timeout;

    printf("Programa: ");
    scanf("%s", program);

    printf("Timeout (s): ");
    scanf("%d", &timeout);

    pid_t pid = fork();

    if (pid == 0) {
        execlp(program, program, NULL);
        perror("Erro ao executar");
        exit(1);
    } else {
        child_pid = pid;

        pthread_t tid;
        pthread_create(&tid, NULL, monitor, &timeout);
        pthread_detach(tid);

        waitpid(child_pid, NULL, 0);
        finished = 1;
        printf("Programa finalizado.\n");
    }

    struct rusage usage;
    getrusage(RUSAGE_CHILDREN, &usage);

    printf("CPU user: %ld\n", usage.ru_utime.tv_sec);
    printf("CPU system: %ld\n", usage.ru_stime.tv_sec);

    return 0;
}