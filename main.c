/*
 * Arquivo main.c - Programa principal que inicia o KernelSim e o InterControllerSim
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

pid_t pid_kernel, pid_inter;

void handle_sigint(int sig) {
    printf("Main: Recebido SIGINT, encerrando processos filhos...\n");
    fflush(stdout);
    kill(pid_kernel, SIGTERM);
    kill(pid_inter, SIGTERM);
    exit(0);
}

int main() {
    // Remove qualquer arquivo kernel_pid existente
    unlink("kernel_pid");

    // Iniciar o KernelSim
    pid_kernel = fork();
    if (pid_kernel == -1) {
        perror("Erro ao criar processo para KernelSim");
        exit(1);
    } else if (pid_kernel == 0) {
        // Código do processo filho - KernelSim
        char *args[] = {"./kernelsim", NULL};
        execv("./kernelsim", args);
        perror("Erro ao executar o KernelSim");
        exit(1);
    }

    // Aguarda até que o arquivo "kernel_pid" seja criado pelo KernelSim
    printf("Main: Aguardando o KernelSim criar o arquivo 'kernel_pid'...\n");
    fflush(stdout);
    pid_t kernel_pid_read;
    while (1) {
        FILE *fp = fopen("kernel_pid", "r");
        if (fp) {
            if (fscanf(fp, "%d", &kernel_pid_read) == 1) {
                fclose(fp);
                printf("Main: PID do KernelSim lido do arquivo: %d\n", kernel_pid_read);
                fflush(stdout);
                break;
            } else {
                fclose(fp);
            }
        }
        usleep(100000); // Espera 100ms antes de tentar novamente
    }
    printf("Main: Arquivo 'kernel_pid' encontrado e PID lido. Iniciando o InterControllerSim.\n");
    fflush(stdout);

    // Iniciar o InterControllerSim
    pid_inter = fork();
    if (pid_inter == -1) {
        perror("Erro ao criar processo para InterControllerSim");
        kill(pid_kernel, SIGTERM);
        exit(1);
    } else if (pid_inter == 0) {
        // Código do processo filho - InterControllerSim
        char *args[] = {"./intercontrollersim", NULL};
        execv("./intercontrollersim", args);
        perror("Erro ao executar o InterControllerSim");
        exit(1);
    }

    // Configurar o manipulador para SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    // Manter o programa principal ativo enquanto os processos filhos estão em execução
    while (1) {
        // Você pode adicionar código aqui para monitorar os processos filhos, se desejar
        sleep(1);
    }

    return 0;
}

