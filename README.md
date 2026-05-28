# FMS Monitor — Financial Management System

## Descrição

O **FMS Monitor** é um sistema desenvolvido em linguagem C para a disciplina de **Sistemas Operacionais**, com o objetivo de monitorar a execução de programas no sistema operacional Linux utilizando conceitos fundamentais de:

* Processos
* Threads
* Sinais
* Gerenciamento de recursos
* Controle de quotas
* Monitoramento de CPU e memória

O sistema funciona como um gerenciador simples de execução de processos, permitindo definir limites de uso de recursos e controlar custos computacionais em modos de operação pré-pago e pós-pago.

---

# Funcionalidades

* Execução de programas externos utilizando `fork()` e `execlp()`
* Monitoramento de processos filhos
* Controle de timeout
* Encerramento automático de processos com `SIGKILL`
* Monitoramento de:

  * Uso de CPU
  * Uso de memória
  * Tempo total de execução
* Sistema de quotas:

  * Quota de CPU
  * Quota de memória
* Sistema de cobrança:

  * Modo Pré-Pago
  * Modo Pós-Pago
* Relatório final de execução
* Uso de threads para monitoramento concorrente
* Sincronização com mutex (`pthread_mutex_t`)

---

# Conceitos de Sistemas Operacionais Utilizados

## Processos

O sistema cria processos filhos utilizando:

```c
fork()
```

Cada processo filho executa um programa externo através de:

```c
execlp()
```

---

## Threads

Uma thread é criada para monitorar o tempo de execução do processo:

```c
pthread_create()
```

A thread verifica continuamente se o timeout foi atingido.

---

## Sinais

Caso o processo ultrapasse o tempo limite definido, o sistema envia:

```c
SIGKILL
```

utilizando:

```c
kill(child_pid, SIGKILL);
```

---

## Monitoramento de Recursos

O sistema utiliza:

```c
wait4()
```

para coletar informações de uso de recursos através da estrutura:

```c
struct rusage
```

Os dados monitorados incluem:

* CPU user time
* CPU system time
* Memória máxima utilizada (`maxrss`)

---

## Sincronização

Para evitar problemas de concorrência entre threads, foi utilizado:

```c
pthread_mutex_t
```

garantindo acesso seguro às variáveis compartilhadas.

---

# Sistema de Custos

O sistema implementa uma cobrança baseada no uso de recursos.

## Fórmulas

### Custo de CPU

```text
Custo CPU = tempo_cpu × 2.0
```

### Custo de Memória

```text
Custo Memória = memória_kB × 0.01
```

### Custo Total

```text
Custo Total = custo_cpu + custo_memoria
```

---

# Modos de Operação

## Pré-Pago

O usuário define uma quantidade inicial de créditos.

* Cada execução desconta créditos
* Quando os créditos acabam, o sistema encerra

---

## Pós-Pago

O sistema acumula os custos das execuções.

* Não há limite inicial de créditos
* O custo total é exibido ao final

---

# Fluxo de Execução

1. Usuário escolhe o modo de operação
2. Define:

   * Créditos
   * Timeout
   * Limite de CPU
   * Limite de memória
3. Informa o programa a ser executado
4. O sistema:

   * Cria processo filho
   * Monitora execução
   * Calcula custos
   * Valida quotas
5. Gera relatório da execução

---

# Exemplo de Uso

```bash
=== FMS Monitor ===

Escolha o modo de operação:
1 - Pré-Pago
2 - Pós-Pago

Opção: 1

Créditos Pré-Pagos: 500
CPU Total - Limite (s): 10
Timeout (s): 5
Quota de Memória (kB): 100000

Programa: ls
```

---

# Compilação

## Linux

Compile utilizando:

```bash
gcc fms.c -o fms -pthread
```

---

# Execução

```bash
./fms
```

---

# Estrutura do Projeto

```text
.
├── fms.c
└── README.md
```

---

# Bibliotecas Utilizadas

| Biblioteca     | Função                |
| -------------- | --------------------- |
| stdio.h        | Entrada e saída       |
| stdlib.h       | Funções gerais        |
| unistd.h       | Chamadas POSIX        |
| sys/wait.h     | Controle de processos |
| sys/resource.h | Uso de recursos       |
| pthread.h      | Threads               |
| signal.h       | Manipulação de sinais |
| string.h       | Strings               |
| time.h         | Controle de tempo     |

---

# Regras de Encerramento

O FMS é encerrado quando:

* O usuário digita `sair`
* Os créditos acabam
* A quota de CPU é excedida
* A quota de memória é excedida
* O timeout é atingido

---

# Exemplo de Relatório

```text
--- Uso de Recursos ---
CPU user: 0.002000 s
CPU system: 0.001000 s
Memória máxima: 2048 kB

--- Custo da Execução ---
CPU: 0.01 créditos
Memória: 20.48 créditos
Total: 20.49 créditos
```

---

# Objetivo Acadêmico

Este projeto tem como finalidade aplicar conceitos práticos de Sistemas Operacionais, especialmente:

* Gerenciamento de processos
* Programação concorrente
* Monitoramento de recursos
* Comunicação entre processos
* Controle de execução
* Escalonamento e limites de recursos


Projeto desenvolvido para a disciplina de **Sistemas Operacionais**.
