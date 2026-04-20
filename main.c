#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>

// constantes de custo
const float CUSTO_CPU_POR_SEGUNDO = 2.0;
const float CUSTO_MEMORIA_POR_KB = 0.01;

// variaveis globais para comunicação entre threads e processos
pid_t child_pid;
int finished = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
float cpu_credits_limit = 0.0;  // créditos pré-pagos em CPU (segundos)
float cpu_quota_limit = 0.0;  // quota de CPU por programa (segundos)
float memory_quota_limit = 0.0;   // quota de memória para a execução (kB)
float cpu_total_used = 0.0;  // acúmulo de CPU utilizada (segundos)
float memory_max_used = 0.0;  // máximo de memória utilizada (kB)
float creditos = 0.0;  // créditos restantes

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
    float cpu_credits;
    float cpu_quota;
    float memory_quota;
    int quota_exceeded = 0;
    int modo_prepago = 0;  // 1 = pré-pago, 0 = pós-pago
    float custo_acumulado = 0.0;  // para pós-pago

    printf("=== FMS Monitor ===\n");

    // menu de escolha: pré-pago ou pós-pago
    printf("\nEscolha o modo de operação:\n");
    printf("1 - Pré-Pago (com créditos iniciais)\n");
    printf("2 - Pós-Pago (acumula custos)\n");
    printf("Opção (1 ou 2): ");
    scanf("%d", &modo_prepago);
    getchar();

    if (modo_prepago != 1) {
        modo_prepago = 0;  // qualquer opção diferente de 1 = pós-pago
    }

    // solicitar limites globais
    printf("\nConfiguração dos Limites:\n");
    
    if (modo_prepago == 1) {
        printf("Créditos Pré-Pagos: ");
        scanf("%f", &cpu_credits);
        getchar();
    } else {
        cpu_credits = 0.0;  // pós-pago não tem créditos iniciais
    }

    printf("CPU Total - Limite (s): ");
    scanf("%f", &cpu_quota);
    getchar();

    printf("Timeout (s): ");
    scanf("%d", &timeout);
    getchar();

    printf("Quota de Memória (kB): ");
    scanf("%f", &memory_quota);
    getchar();

    // armazenar quotas nas variáveis globais
    cpu_credits_limit = cpu_credits;
    cpu_quota_limit = cpu_quota;
    memory_quota_limit = memory_quota;
    
    if (modo_prepago == 1) {
        creditos = cpu_credits;  // pré-pago: inicializa com créditos
    } else {
        creditos = 0.0;  // pós-pago: sem créditos iniciais
    }

    printf("\n--- Limites Configurados ---\n");
    if (modo_prepago == 1) {
        printf("MODO: Pré-Pago\n");
        printf("Créditos Disponíveis: %.2f\n", creditos);
    } else {
        printf("MODO: Pós-Pago\n");
        printf("Custo Acumulado: %.2f\n", custo_acumulado);
    }
    printf("CPU Total (limite): %.6f s\n", cpu_quota_limit);
    printf("Timeout: %d s\n", timeout);
    printf("Memória: %.0f kB\n\n", memory_quota_limit);

    while (1) {
        // verificar se há créditos disponíveis (apenas em pré-pago)
        if (modo_prepago == 1 && cpu_credits_limit > 0 && creditos <= 0) {
            printf("\n*** Créditos esgotados! Encerrando FMS. ***\n");
            break;
        }

        if (modo_prepago == 1) {
            printf("\n[Créditos Disponíveis: %.2f] ", creditos);
        } else {
            printf("\n[Custo Acumulado: %.2f] ", custo_acumulado);
        }
        printf("Programa (ou 'sair' para terminar): ");
        fgets(program, sizeof(program), stdin);

        // remover o \n do final da string
        size_t len = strlen(program);
        if (len > 0 && program[len - 1] == '\n') {
            program[len - 1] = '\0';
        }

        if (strcmp(program, "sair") == 0) {
            break;
        }

        finished = 0; // reseta a flag finished para o próximo programa
        time_t inicio_exec = time(NULL);  // marca tempo inicial de execução

        pid_t pid = fork(); // criar processo filho para executar o programa

        if (pid == 0) {
            // processo filho
            execlp(program, program, NULL); // executar o programa
            perror("Erro ao executar");
            exit(127);
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

            // verificar se processo falhou ao executar o binário
            int exec_failed = 0;
            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                if (exit_code == 127) {
                    exec_failed = 1;  // falha em executar o binário
                }
            }

            if (exec_failed) {
                printf("\n*** Erro: falha ao lançar o binário. Não será descontada quota. ***\n");
            } else {
                time_t fim_exec = time(NULL);  // marca tempo final de execução
                
                // bloco para log de uso de recursos
                printf("\nPrograma finalizado.\n");
                printf("\n--- Uso de Recursos ---\n");
                printf("CPU user: %ld.%06ld s\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
                printf("CPU system: %ld.%06ld s\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
                printf("Memória máxima (maxrss): %ld kB\n", usage.ru_maxrss);
                printf("Tempo total de execução: %ld segundos\n", fim_exec - inicio_exec);

                // calcular CPU total desta execução em segundos
                float cpu_exec = (usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0) + (usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0);

                // calcular custo da execução
                float custo_cpu = cpu_exec * CUSTO_CPU_POR_SEGUNDO;
                float custo_memoria = usage.ru_maxrss * CUSTO_MEMORIA_POR_KB;
                float custo_total = custo_cpu + custo_memoria;

                // acumular CPU e atualizar memória máxima (apenas se execução foi bem-sucedida)
                cpu_total_used += cpu_exec;
                if ((float)usage.ru_maxrss > memory_max_used) {
                    memory_max_used = (float)usage.ru_maxrss;
                }

                // atualizar créditos/custo baseado no modo
                if (modo_prepago == 1) {
                    creditos -= custo_total;  // pré-pago: desconta
                } else {
                    custo_acumulado += custo_total;  // pós-pago: acumula
                }

                // validação de quotas
                printf("\n--- Validação de Quotas ---\n");
                printf("CPU desta execução: %.6f s / %.6f s (limite) ", cpu_exec, cpu_quota_limit);
                if (cpu_quota_limit > 0 && cpu_exec > cpu_quota_limit) {
                    printf("EXCEDIDA\n");
                    quota_exceeded = 1;
                } else {
                    printf("OK\n");
                }

                printf("Memória máxima: %ld kB / %.0f kB ", usage.ru_maxrss, memory_quota_limit);
                if (memory_quota_limit > 0 && usage.ru_maxrss > memory_quota_limit) {
                    printf("EXCEDIDA\n");
                    quota_exceeded = 1;
                } else {
                    printf("OK\n");
                }

                // mostrar custo da execução
                printf("\n--- Custo da Execução ---\n");
                printf("CPU: %.2f créditos (%.6f s × %.2f)\n", custo_cpu, cpu_exec, CUSTO_CPU_POR_SEGUNDO);
                printf("Memória: %.2f créditos (%ld kB × %.2f)\n", custo_memoria, usage.ru_maxrss, CUSTO_MEMORIA_POR_KB);
                printf("Total: %.2f créditos\n", custo_total);
                
                if (modo_prepago == 1) {
                    printf("Créditos restantes: %.2f\n", creditos);
                    
                    if (creditos <= 0.0) {
                        printf("\n*** Créditos esgotados! ***\n");
                        break;
                    }
                } else {
                    printf("Custo acumulado: %.2f\n", custo_acumulado);
                }

                // encerrar se quota foi excedida
                if (quota_exceeded) {
                    printf("\n*** Quota excedida! Encerrando FMS. ***\n");
                    break;
                }
            }
        } else {
            perror("Erro ao fazer fork");
        }
    }

    printf("\n--- Relatório Final ---\n");
    if (modo_prepago == 1) {
        printf("Créditos restantes: %.2f\n", creditos);
    } else {
        printf("Custo total acumulado: %.2f\n", custo_acumulado);
    }
    printf("FMS finalizado.\n");
    return 0;
}