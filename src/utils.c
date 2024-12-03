#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

void create_named_pipe(const char *path) {
    if (access(path, F_OK) == 0) {
        printf("FIFO existente encontrado. Removendo...\n");
        if (unlink(path) == -1) {
            perror("Erro ao remover FIFO existente");
            exit(EXIT_FAILURE);
        }
    }
    if (mkfifo(path, 0666) == -1) {
        perror("Erro ao criar Named Pipe");
        exit(EXIT_FAILURE);
    }
}

void init_semaphore(sem_t *semaphore) {
    if (sem_init(semaphore, 0, 1) == -1) {
        perror("Erro ao inicializar semáforo");
        exit(EXIT_FAILURE);
    }
}

void lock_semaphore(sem_t *semaphore) {
    if (sem_wait(semaphore) != 0) {
        perror("Erro ao bloquear semáforo");
        exit(EXIT_FAILURE);
    }
}

void unlock_semaphore(sem_t *semaphore) {
    if (sem_post(semaphore) != 0) {
        perror("Erro ao desbloquear semáforo");
        exit(EXIT_FAILURE);
    }
}
