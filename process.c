/*
 * Arquivo process.c - Simula um processo de aplicação
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define MAX_ITERATIONS 10

volatile sig_atomic_t PC = 0; // Declarar PC como sig_atomic_t para acesso seguro em sinais

void save_pc_state(int PC) {
    char filename[256];
    sprintf(filename, "pc_state_%d", getpid());
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Erro ao abrir arquivo para salvar estado do PC");
        exit(1);
    }
    fprintf(fp, "%d\n", PC);
    fclose(fp);
}

int load_pc_state() {
    char filename[256];
    sprintf(filename, "pc_state_%d", getpid());
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        if (errno == ENOENT) {
            return 0; // Se o arquivo não existir, inicializa PC como 0
        } else {
            perror("Erro ao abrir arquivo para ler estado do PC");
            exit(1);
        }
    }
    int PC;
    if (fscanf(fp, "%d", &PC) != 1) {
        fprintf(stderr, "Processo %d: Erro ao ler estado do PC do arquivo '%s'\n", getpid(), filename);
        fclose(fp);
        exit(1);
    }
    fclose(fp);
    return PC;
}

void handle_sigcont(int sig) {
    int loaded_pc = load_pc_state();
    printf("Processo %d retomado. PC = %d\n", getpid(), loaded_pc);
    fflush(stdout);
    PC = loaded_pc;
}

void handle_sigusr1(int sig) {
    printf("Processo %d recebendo SIGUSR1, salvando PC=%d e parando.\n", getpid(), PC);
    fflush(stdout);
    save_pc_state(PC);
    kill(getpid(), SIGSTOP);
}

void handle_sigterm(int sig) {
    printf("Processo %d: Recebido SIGTERM, encerrando.\n", getpid());
    fflush(stdout);
    // Remover o arquivo de estado do PC
    char filename[256];
    sprintf(filename, "pc_state_%d", getpid());
    unlink(filename);
    exit(0);
}

int main() {
    int kernel_pid;

    // Configurando os handlers para SIGCONT, SIGUSR1 e SIGTERM
    struct sigaction sa_cont;
    sa_cont.sa_handler = handle_sigcont;
    sigemptyset(&sa_cont.sa_mask);
    sa_cont.sa_flags = SA_RESTART;
    sigaction(SIGCONT, &sa_cont, NULL);

    struct sigaction sa_usr1;
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    signal(SIGTERM, handle_sigterm);

    // Ler o PID do KernelSim a partir de um arquivo
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

    printf("Processo %d: PID do KernelSim lido do arquivo: %d\n", getpid(), kernel_pid);
    fflush(stdout);

    srand(time(NULL) ^ (getpid() << 16));

    PC = load_pc_state(); // Carregar o estado do PC

    while (PC < MAX_ITERATIONS) {
        // Incrementa PC antes da iteração
        PC++;

        printf("Processo %d executando PC = %d\n", getpid(), PC);
        fflush(stdout);

        // Salva o estado do PC após incrementá-lo
        save_pc_state(PC);

        // Simula um quanta do processo
        sleep(1);

        // Em pontos aleatórios, faz uma "syscall" de leitura/escrita
        if (rand() % 4 == 0) { // Aproximadamente 25% das vezes
            printf("Processo %d fazendo uma syscall para I/O\n", getpid());
            fflush(stdout);
            // Enviar um sinal ao KernelSim para indicar que este processo está em I/O
            if (kill(kernel_pid, SIGUSR2) == -1) {
                perror("Erro ao enviar SIGUSR2 para o KernelSim");
                exit(1);
            }
            // O processo será suspenso pelo KernelSim
            pause(); // Espera ser retomado
        }
    }

    printf("Processo %d completou todas as iterações. Encerrando programa.\n", getpid());
    fflush(stdout);

    // Ignorar sinais após a conclusão
    signal(SIGCONT, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);

    // Remover o arquivo de estado do PC
    char filename[256];
    sprintf(filename, "pc_state_%d", getpid());
    unlink(filename);

    // Sair do programa
    exit(0);
}

