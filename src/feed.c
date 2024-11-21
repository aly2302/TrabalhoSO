#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>

#define USER_FIFO_TEMPLATE "/tmp/feed_%s"

char user_fifo[100];
int main_fifo_fd = -1;

void *receive_messages(void *arg) {
    (void)arg; // Evitar warning de parâmetro não utilizado

    int user_fifo_fd = open(user_fifo, O_RDONLY | O_NONBLOCK);
    if (user_fifo_fd == -1) {
        perror("Erro ao abrir FIFO do usuário");
        return NULL;
    }

    struct pollfd pfd = { .fd = user_fifo_fd, .events = POLLIN };
    char buffer[MSG_MAX_LENGTH];

    while (1) {
        int ret = poll(&pfd, 1, 500);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            int bytes_read = read(user_fifo_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                printf("\nMensagem recebida: %s\n", buffer);
                printf("Digite um comando: ");
                fflush(stdout);
            }
        }
    }

    close(user_fifo_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: ./feed <username>\n");
        return 1;
    }

    char *username = argv[1];
    snprintf(user_fifo, sizeof(user_fifo), USER_FIFO_TEMPLATE, username);

    printf("Feed iniciado para o usuário: %s\n", username);
    create_named_pipe(user_fifo);

    main_fifo_fd = open(MAIN_FIFO, O_WRONLY);
    if (main_fifo_fd == -1) {
        perror("Erro ao abrir FIFO principal");
        return 1;
    }

    // Enviar mensagem de registro ao servidor
    Message msg = {0};
    strcpy(msg.username, username);
    strcpy(msg.command, "register");
    write(main_fifo_fd, &msg, sizeof(Message));
    printf("Usuário %s registrado com sucesso no servidor.\n", username);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, receive_messages, NULL);

    while (1) {
        printf("Digite um comando (msg, subscribe, unsubscribe, exit): ");
        char command[20];
        scanf("%s", command);

        if (strcmp(command, "msg") == 0) {
            printf("Digite o tópico: ");
            scanf("%s", msg.topic);
            printf("Digite a mensagem: ");
            getchar(); // Consumir newline
            fgets(msg.message, MSG_MAX_LENGTH, stdin);
            msg.message[strcspn(msg.message, "\n")] = '\0';
            msg.duration = 0; // Mensagens não persistentes
            strcpy(msg.command, "msg");
        } else if (strcmp(command, "subscribe") == 0) {
            printf("Digite o tópico: ");
            scanf("%s", msg.topic);
            strcpy(msg.command, "subscribe");
        } else if (strcmp(command, "exit") == 0) {
            strcpy(msg.command, "exit");
            write(main_fifo_fd, &msg, sizeof(msg));
            break;
        } else {
            printf("Comando desconhecido. Tente novamente.\n");
            continue;
        }

        write(main_fifo_fd, &msg, sizeof(msg));
    }

    close(main_fifo_fd);
    unlink(user_fifo);
    return 0;
}
