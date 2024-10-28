/*
 * Arquivo kernelsim.c - Simula um kernel escalonador
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define NUM_PROCESSES 3

pid_t processos[NUM_PROCESSES];
int current_process = 0;
int processos_bloqueados[NUM_PROCESSES] = {0}; // Array para marcar processos bloqueados por I/O
int processos_ativos[NUM_PROCESSES] = {1, 1, 1}; // Array para marcar processos ainda ativos
int fila_io[NUM_PROCESSES]; // Fila para processos bloqueados em I/O
int fila_io_inicio = 0;
int fila_io_fim = 0;

void enqueue_io(int index) {
    fila_io[fila_io_fim] = index;
    fila_io_fim = (fila_io_fim + 1) % NUM_PROCESSES;
}

int dequeue_io() {
    if (fila_io_inicio == fila_io_fim) {
        // Fila vazia
        return -1;
    }
    int index = fila_io[fila_io_inicio];
    fila_io_inicio = (fila_io_inicio + 1) % NUM_PROCESSES;
    return index;
}

void handle_irq0(int sig) {
    // Handler para simular a interrupção do time slice (IRQ0)
    if (processos_ativos[current_process] == 0 || processos_bloqueados[current_process]) {
        // Se o processo atual já terminou ou está bloqueado, simplesmente retorne
        return;
    }

    printf("KernelSim: Time slice do processo %d terminou. Enviando SIGUSR1.\n", processos[current_process]);
    fflush(stdout);
    kill(processos[current_process], SIGUSR1); // Envia sinal SIGUSR1 para interromper o processo

    // Encontrar o próximo processo disponível
    int next_process = current_process;
    int found = 0;
    for (int i = 0; i < NUM_PROCESSES; i++) {
        next_process = (next_process + 1) % NUM_PROCESSES;
        if (processos_ativos[next_process] && !processos_bloqueados[next_process]) {
            found = 1;
            break;
        }
    }
    if (found) {
        current_process = next_process;
        printf("KernelSim: Ativando processo %d com SIGCONT.\n", processos[current_process]);
        fflush(stdout);
        kill(processos[current_process], SIGCONT);
    } else {
        printf("KernelSim: Nenhum processo disponível para executar.\n");
        fflush(stdout);
    }
}

void handle_irq1(int sig) {
    // Handler para simular a interrupção de I/O completado (IRQ1)
    int index = dequeue_io();
    if (index != -1) {
        processos_bloqueados[index] = 0;
        printf("KernelSim: I/O completado. Desbloqueando processo %d.\n", processos[index]);
        fflush(stdout);
    } else {
        printf("KernelSim: Nenhum processo aguardando I/O.\n");
        fflush(stdout);
    }
}

void handle_syscall(int sig, siginfo_t *siginfo, void *context) {
    // Handler para lidar com a "syscall" de I/O feita pelos processos
    pid_t pid = siginfo->si_pid;
    printf("KernelSim: Processo %d solicitou I/O, marcando como bloqueado.\n", pid);
    fflush(stdout);

    // Encontrar o índice do processo que fez a syscall
    int index = -1;
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (processos[i] == pid) {
            processos_bloqueados[i] = 1;
            enqueue_io(i); // Adicionar à fila de I/O
            index = i;
            break;
        }
    }

    // Enviar SIGUSR1 para o processo para que ele salve seu estado e pare
    kill(pid, SIGUSR1);

    // Encontrar o próximo processo disponível para executar
    int next_process = current_process;
    int found = 0;
    for (int i = 0; i < NUM_PROCESSES; i++) {
        next_process = (next_process + 1) % NUM_PROCESSES;
        if (processos_ativos[next_process] && !processos_bloqueados[next_process]) {
            found = 1;
            break;
        }
    }
    if (found) {
        current_process = next_process;
        printf("KernelSim: Ativando processo %d com SIGCONT.\n", processos[current_process]);
        fflush(stdout);
        kill(processos[current_process], SIGCONT);
    } else {
        printf("KernelSim: Nenhum processo disponível para executar.\n");
        fflush(stdout);
    }
}

void handle_sigchld(int sig) {
    // Handler para detectar quando um processo filho termina
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < NUM_PROCESSES; i++) {
            if (processos[i] == pid) {
                processos_ativos[i] = 0;
                printf("KernelSim: Processo %d terminou. Marcando como inativo.\n", pid);
                fflush(stdout);
                break;
            }
        }
    }
}

void handle_sigterm(int sig) {
    printf("KernelSim: Recebido SIGTERM, encerrando...\n");
    fflush(stdout);
    // Encerra os processos filhos
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (processos_ativos[i]) {
            kill(processos[i], SIGTERM);
        }
    }
    // Remove o arquivo kernel_pid
    unlink("kernel_pid");
    exit(0);
}

int main() {
    // Remove o arquivo kernel_pid se existir
    unlink("kernel_pid");

    // Escrever o PID do KernelSim em um arquivo para que os processos possam ler
    pid_t kernel_pid = getpid();
    FILE *fp = fopen("kernel_pid", "w");
    if (!fp) {
        perror("Erro ao criar arquivo para escrever PID do KernelSim");
        exit(1);
    }
    fprintf(fp, "%d\n", kernel_pid);
    fclose(fp);

    printf("KernelSim: PID escrito no arquivo kernel_pid com sucesso.\n");
    fflush(stdout);

    // Configurar os handlers para os sinais de time slice (SIGALRM), I/O completado (SIGUSR1), syscall de I/O (SIGUSR2), SIGCHLD, e SIGTERM
    struct sigaction sa_irq0;
    sa_irq0.sa_handler = handle_irq0;
    sigemptyset(&sa_irq0.sa_mask);
    sa_irq0.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa_irq0, NULL) == -1) {
        perror("Erro ao configurar o handler para SIGALRM");
        exit(1);
    }

    struct sigaction sa_irq1;
    sa_irq1.sa_handler = handle_irq1;
    sigemptyset(&sa_irq1.sa_mask);
    sa_irq1.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa_irq1, NULL) == -1) {
        perror("Erro ao configurar o handler para SIGUSR1");
        exit(1);
    }

    struct sigaction sa_syscall;
    sa_syscall.sa_sigaction = handle_syscall;
    sigemptyset(&sa_syscall.sa_mask);
    sa_syscall.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGUSR2, &sa_syscall, NULL) == -1) {
        perror("Erro ao configurar o handler para SIGUSR2");
        exit(1);
    }

    struct sigaction sa_chld;
    sa_chld.sa_handler = handle_sigchld;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa_chld, NULL) == -1) {
        perror("Erro ao configurar o handler para SIGCHLD");
        exit(1);
    }

    struct sigaction sa_term;
    sa_term.sa_handler = handle_sigterm;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM, &sa_term, NULL) == -1) {
        perror("Erro ao configurar o handler para SIGTERM");
        exit(1);
    }

    // Criar os processos filhos (A1, A2, A3, etc...)
    for (int i = 0; i < NUM_PROCESSES; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Código do processo filho
            char *args[] = {"./process", NULL};
            execv("./process", args);
            perror("Erro ao executar o processo");
            exit(1);
        } else if (pid > 0) {
            // Código do processo pai (KernelSim)
            processos[i] = pid;
        } else {
            perror("Erro ao criar processo filho");
            exit(1);
        }
    }

    // Esperar um pouco para garantir que todos os processos foram criados
    sleep(1);

    // Ativar o primeiro processo
    printf("KernelSim: Ativando processo %d com SIGCONT.\n", processos[current_process]);
    fflush(stdout);
    kill(processos[current_process], SIGCONT);

    // Loop infinito para simular o KernelSim
    while (1) {
        pause(); // Espera pelo próximo sinal enviado pelo InterControllerSim ou pelos processos
    }

    return 0;
}

