#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>

// variaveis globais para comunicação entre threads e processos
pid_t child_pid;
int finished = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
float cpu_quota_limit = 0.0;  // quota de CPU para a execução (segundos)
float memory_quota_limit = 0.0;   // quota de memória para a execução (kB)

// thread para monitoramento do timeout do processo filho
void* monitor(void* arg) {
    int timeout = *(int*)arg;
    struct timespec sleep_time;

    for (int i = 0; i < timeout; i++) {
        sleep_time.tv_sec = 1;
        sleep_time.tv_nsec = 0;
        nanosleep(&sleep_time, NULL);

        // se mutex_lock bloqueado -> processo terminou, então sair da thread
        pthread_mutex_lock(&mutex);
        if (finished) {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_lock(&mutex);
    if (!finished) {
        printf("\nTimeout atingido! Matando processo...\n");
        fflush(stdout);
        kill(child_pid, SIGKILL);
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main() {
    char program[256];
    int timeout;
    float cpu_quota;
    float memory_quota;

    printf("=== FMS Monitor ===\n");

    while (1) {
        printf("\nPrograma (ou 'sair' para terminar): ");
        fgets(program, sizeof(program), stdin); // fgets para ler o nome do programa

        // remover o \n do final da string
        size_t len = strlen(program);
        if (len > 0 && program[len - 1] == '\n') {
            program[len - 1] = '\0';
        }

        if (strcmp(program, "sair") == 0) {
            break;
        }

        printf("Timeout (s): ");
        scanf("%d", &timeout);
        getchar(); // consome o \n deixado pelo scanf

        printf("Quota de CPU (s): ");
        scanf("%f", &cpu_quota);
        getchar(); // consome o \n deixado pelo scanf

        printf("Quota de Memória (kB): ");
        scanf("%f", &memory_quota);
        getchar(); // consome o \n deixado pelo scanf

        finished = 0; // reseta a flag finished para o próximo programa
        cpu_quota_limit = cpu_quota;       // armazena quota de CPU nas variáveis globais
        memory_quota_limit = memory_quota; // armazena quota de memória nas variáveis globais

        pid_t pid = fork(); // criar processo filho para executar o programa

        if (pid == 0) {
            // processo filho
            execlp(program, program, NULL); // executar o programa
            perror("Erro ao executar");
            exit(1);
        } else if (pid > 0) {
            // processo pai
            child_pid = pid;

            // bloco para iniciar thread de monitoramento
            pthread_t tid;
            pthread_create(&tid, NULL, monitor, &timeout); // inicia a thread de monitoramento timeout

            // bloco para uso de recursos
            int status;
            struct rusage usage;
            wait4(child_pid, &status, 0, &usage); // aguarda o processo filho e pegar uso de recursos

            // bloco para sinalizar que o processo terminou
            pthread_mutex_lock(&mutex);
            finished = 1;
            pthread_mutex_unlock(&mutex);

            pthread_join(tid, NULL); // aguarda a thread de monitoramento terminar

            // bloco para log de uso de recursos
            printf("\nPrograma finalizado.\n");
            printf("CPU user: %ld.%06ld s\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
            printf("CPU system: %ld.%06ld s\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
            printf("Memória máxima (maxrss): %ld kB\n", usage.ru_maxrss);

            // calcular CPU total em segundos
            float cpu_total = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0 + (usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0);

            // validação de quotas
            printf("\n--- Validação de Quotas ---\n");
            printf("CPU: %.6f s / %.6f s", cpu_total, cpu_quota_limit);
            if (cpu_total > cpu_quota_limit) {
                printf("EXCEDIDA\n");
            } else {
                printf("OK\n");
            }

            printf("Memória: %f kB / %f kB", (float)usage.ru_maxrss, memory_quota_limit);
            if (usage.ru_maxrss > memory_quota_limit) {
                printf("EXCEDIDA\n");
            } else {
                printf("OK\n");
            }
        } else {
            perror("Erro ao fazer fork");
        }
    }

    printf("\nFMS finalizado.\n");
    return 0;
}