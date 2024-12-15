#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

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

