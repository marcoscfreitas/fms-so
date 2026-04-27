#include <stdio.h> // lib para entrada/saída padrão
#include <stdlib.h> // lib para funções de alocação, controle de processos, etc.
#include <unistd.h> // lib para chamadas de sistema POSIX (fork, exec, etc.)
#include <sys/wait.h> // lib para espera de processos filhos
#include <sys/resource.h> // lib para gerenciamento de recursos do sistema
#include <pthread.h> // lib para programação com threads
#include <signal.h> // lib para manipulação de sinais
#include <string.h> // lib para manipulação de strings
#include <time.h> // lib para manipulação de tempo

// constantes de faturamento para o cálculo de custos
const float CUSTO_CPU_POR_SEGUNDO = 2.0;
const float CUSTO_MEMORIA_POR_KB = 0.01;

// variaveis globais compartilhadas entre threads e processos, mutex para gerenciar acesso a essas variáveis
pid_t child_pid;
int finished;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// limites configurados pelo usuário, crédito prépago, quota de cpu e memória
float credits_limit;
float cpu_quota_limit;
float memory_quota_limit;

// variaveis para armazenar o uso de recursos
float cpu_total_used;
float memory_max_used;
float creditos;

// função responsável por monitorar o processo filho e matar se atingir o timeout
void* monitor(void* arg) {
    int timeout = *(int*)arg;
    struct timespec sleep_time;

    // loop de monitoramento do processo, continua até o timeout ser atingido ou o processo filho terminar
    for (int i = 0; i < timeout; i++) {
        // usa o sleep de 1 segundo para verificar o status do processo
        sleep_time.tv_sec = 1;
        sleep_time.tv_nsec = 0;
        nanosleep(&sleep_time, NULL);

        // mutex para trancar o acesso à variável 'finished' e verificar se o processo filho já terminou
        pthread_mutex_lock(&mutex);
        if (finished) {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        pthread_mutex_unlock(&mutex);
    }

    // caso o timeout seja atingido e o processo ainda esteja rodando, envia o sinal SIGKILL para mata-lo
    pthread_mutex_lock(&mutex);
    if (!finished) {
        printf("\nTimeout atingido! Matando processo...\n");
        fflush(stdout);
        kill(child_pid, SIGKILL);
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

// função principal, exibe menu, configura limite, executa programas, monitora recursos e calcula custos
int main() {
    // variaveis locais da função
    char program[256];
    int timeout = 0;
    float credits = 0.0;
    float cpu_quota = 0.0;
    float memory_quota = 0.0;
    int quota_exceeded = 0;
    int modo_prepago = 0;
    float custo_acumulado = 0.0;

    printf("=== FMS Monitor ===\n");

    // bloco de seleção de modo de operação
    printf("\nEscolha o modo de operação:\n");
    printf("1 - Pré-Pago (com créditos iniciais)\n");
    printf("2 - Pós-Pago (acumula custos)\n");
    printf("Opção (1 ou 2): ");
    scanf("%d", &modo_prepago);
    getchar();

    // bloco de configuração dos limites e créditos
    printf("\nConfiguração dos Limites:\n");
    
    if (modo_prepago == 1) {
        printf("Créditos Pré-Pagos: ");
        scanf("%f", &credits);
        getchar();
    } else {
        credits = 0.0;
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

    // passa valores para variáveis globais
    credits_limit = credits;
    cpu_quota_limit = cpu_quota;
    memory_quota_limit = memory_quota;
    
    // se for prepago inicia com os créditos inseridos, caso contrário não usa
    if (modo_prepago == 1) {
        creditos = credits;
    } else {
        creditos = 0.0;
    }

    // exibe os limites configurados para o usuário antes de iniciar o loop de execução
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

    // bloco com loop infinito principal de execução dos programas, monitoramento e cálculo de custos
    while (1) {
        // validação para encerrar o FMS caso seja modo pré-pago e os créditos acabaram
        if (modo_prepago == 1 && credits_limit > 0 && creditos <= 0) {
            printf("\n*** Créditos esgotados! Encerrando FMS. ***\n");
            break;
        }

        // mostra créditos disponiveis ou custo acumulado antes de solicitar o programa
        if (modo_prepago == 1) {
            printf("\n[Créditos Disponíveis: %.2f] ", creditos);
        } else {
            printf("\n[Custo Acumulado: %.2f] ", custo_acumulado);
        }
        printf("Programa (ou 'sair' para terminar): ");
        
        // lê o nome do programa a ser executado, usando fgets
        fgets(program, sizeof(program), stdin);

        // remove o \n do final da string lida por fgets, se existir
        size_t len = strlen(program);
        if (len > 0 && program[len - 1] == '\n') {
            program[len - 1] = '\0';
        }

        // se o usuário digitar 'sair', o loop principal é encerrado e o FMS finaliza
        if (strcmp(program, "sair") == 0) {
            break;
        }

        // incializa variáveis para monitoramento e controle do processo filho
        finished = 0;
        time_t inicio_exec = time(NULL);

        // usa o fork para criar um processo filho que irá executar o programa solicitado pelo usuário
        pid_t pid = fork();

        // se o fork falhar, exibe mensagem de erro e continua para próxima iteração do loop
        if (pid == 0) {
            execlp(program, program, NULL);
            perror("Erro ao executar");
            exit(127);
        } else if (pid > 0) {
            //se o fork for bem-sucedido, o processo continua, armazenando o PID do filho para monitoramento
            child_pid = pid;

            // cria thread de monitoramento para acompanhar o processo filho e seu timeout
            pthread_t tid;
            pthread_create(&tid, NULL, monitor, &timeout);

            // wait4() é usado em vez de wait() para obter informações detalhadas sobre o uso de recursos do processo filho, rusage é preenchida com os dados de uso de recursos do processo filho quando ele termina
            int status;
            struct rusage usage;
            wait4(child_pid, &status, 0, &usage);

            // sinaliza para thread de monitoramento que o processo acabou, utilizando mutex para o acesso seguro à variável 'finished'
            pthread_mutex_lock(&mutex);
            finished = 1;
            pthread_mutex_unlock(&mutex);

            // espera thread de monitoramento acabar antes de continuar
            pthread_join(tid, NULL);

            // valida se o processo foi executado ou se houve erro, código de saída 127 programa não encontrado
            int exec_failed = 0;
            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                if (exit_code == 127) {
                    exec_failed = 1;
                }
            }

            // se houve falha na execução, exibe erro e não desconta nada, caso contrário, processa o uso de recursos
            if (exec_failed) {
                printf("\n*** Erro: falha ao lançar o binário. Não será descontada quota. ***\n");
            } else {
                // pega o tempo final da execução do programa
                time_t fim_exec = time(NULL);
                
                // mostra os recursos utilizados, obtidos através da struct rusage preenchida por wait4()
                printf("\nPrograma finalizado.\n");
                printf("\n--- Uso de Recursos ---\n");
                printf("CPU user: %ld.%06ld s\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
                printf("CPU system: %ld.%06ld s\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
                printf("Memória máxima (maxrss): %ld kB\n", usage.ru_maxrss);
                printf("Tempo total de execução: %ld segundos\n", fim_exec - inicio_exec);

                // cálculo da cpu total da execução (user + system), convertendo microsegundos para segundos
                float cpu_exec = (usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0) + 
                                  (usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0);

                // cálculo de custo de cpu e memória baseado nas constantes
                float custo_cpu = cpu_exec * CUSTO_CPU_POR_SEGUNDO;
                float custo_memoria = usage.ru_maxrss * CUSTO_MEMORIA_POR_KB;
                float custo_total = custo_cpu + custo_memoria;

                // mantém registro do custo total da cpu utilizada e maximo de memoria para relatorio
                cpu_total_used += cpu_exec;
                if ((float)usage.ru_maxrss > memory_max_used) {
                    memory_max_used = (float)usage.ru_maxrss;
                }

                // ATUALIZAR: Créditos/custo conforme modo de operação
                if (modo_prepago == 1) {
                    creditos -= custo_total;   // Pré-pago: DESCONTA os créditos disponíveis
                } else {
                    custo_acumulado += custo_total;  // Pós-pago: ACUMULA o custo total
                }

                // relatório de validação das quotas, caso alguma quota tenha excedido, encerra o programa
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

                // mostra o custo da execução de cpu, memória e total de créditos
                printf("\n--- Custo da Execução ---\n");
                printf("CPU: %.2f créditos (%.6f s × %.2f)\n", custo_cpu, cpu_exec, CUSTO_CPU_POR_SEGUNDO);
                printf("Memória: %.2f créditos (%ld kB × %.2f)\n", custo_memoria, usage.ru_maxrss, CUSTO_MEMORIA_POR_KB);
                printf("Total: %.2f créditos\n", custo_total);
                
                // dependendo do modo selecionado, mostra créditos restantes ou custo acumulado
                if (modo_prepago == 1) {
                    printf("Créditos restantes: %.2f\n", creditos);
                    
                    // se acabar os créditos, encerra o FMS
                    if (creditos <= 0.0) {
                        printf("\n*** Créditos esgotados! ***\n");
                        break;
                    }
                } else {
                    printf("Custo acumulado: %.2f\n", custo_acumulado);
                }

                // se o programa exceder alguma quota, encerra o FMS
                if (quota_exceeded) {
                    printf("\n*** Quota excedida! Encerrando FMS. ***\n");
                    break;
                }
            }
        }
    }

    // relatório final que exibe o saldo restante ou custo total da operação
    printf("\n--- Relatório Final ---\n");
    if (modo_prepago == 1) {
        printf("Créditos restantes: %.2f\n", creditos);
    } else {
        printf("Custo total acumulado: %.2f\n", custo_acumulado);
    }
    printf("FMS finalizado.\n");
    return 0;
}