/*
 * Arquivo intercontrollersim.c - Simula um controlador de interrupções
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h> // Para criar threads
#include <errno.h>

#define IRQ0_INTERVAL 1 // Intervalo de 1 segundo para IRQ0
#define IRQ1_INTERVAL 3 // Intervalo de 3 segundos para IRQ1

pid_t kernel_pid;
pthread_t irq0_thread;
pthread_t irq1_thread;
volatile int running = 1;

void handle_sigterm(int sig) {
    printf("InterControllerSim: Recebido SIGTERM, encerrando...\n");
    fflush(stdout);
    running = 0;
    pthread_cancel(irq0_thread);
    pthread_cancel(irq1_thread);
    exit(0);
}

void *irq0_handler_thread(void *arg) {
    while (running) {
        sleep(IRQ0_INTERVAL);
        if (!running) break;
        // Enviar um sinal SIGALRM para o KernelSim para simular IRQ0 (fim do time slice)
        printf("InterControllerSim: Enviando IRQ0 (SIGALRM) para o KernelSim (PID %d)\n", kernel_pid);
        fflush(stdout);
        if (kill(kernel_pid, SIGALRM) == -1) {
            perror("Erro ao enviar SIGALRM para o KernelSim");
        }
    }
    return NULL;
}

void *irq1_handler_thread(void *arg) {
    while (running) {
        sleep(IRQ1_INTERVAL);
        if (!running) break;
        // Enviar um sinal SIGUSR1 para o KernelSim para simular IRQ1 (I/O completado)
        printf("InterControllerSim: Enviando IRQ1 (SIGUSR1) para o KernelSim (PID %d)\n", kernel_pid);
        fflush(stdout);
        if (kill(kernel_pid, SIGUSR1) == -1) {
            perror("Erro ao enviar SIGUSR1 para o KernelSim");
        }
    }
    return NULL;
}

int main() {
    // Configurar o manipulador para SIGTERM
    signal(SIGTERM, handle_sigterm);

    // Ler o PID do KernelSim a partir do arquivo "kernel_pid"
    while (1) {
        FILE *fp = fopen("kernel_pid", "r");
        if (fp) {
            if (fscanf(fp, "%d", &kernel_pid) == 1) {
                fclose(fp);
                break;
            } else {
                fclose(fp);
            }
        }
        // Espera um pouco antes de tentar novamente
        usleep(100000); // Espera 100ms antes de tentar novamente
    }

    printf("InterControllerSim: PID do KernelSim lido do arquivo: %d\n", kernel_pid);
    fflush(stdout);

    // Criar uma thread para gerar IRQ0 (SIGALRM) a cada IRQ0_INTERVAL segundos
    if (pthread_create(&irq0_thread, NULL, irq0_handler_thread, NULL) != 0) {
        perror("Erro ao criar a thread de IRQ0");
        exit(1);
    }

    // Criar uma thread para gerar IRQ1 (SIGUSR1) a cada IRQ1_INTERVAL segundos
    if (pthread_create(&irq1_thread, NULL, irq1_handler_thread, NULL) != 0) {
        perror("Erro ao criar a thread de IRQ1");
        exit(1);
    }

    // Loop infinito para manter o InterControllerSim ativo
    while (running) {
        pause(); // Espera por sinais (mesmo que não esteja tratando nenhum)
    }

    return 0;
}

