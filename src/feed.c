#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>

#define USER_FIFO_TEMPLATE "/tmp/feed_%s"

char user_fifo[100];
int main_fifo_fd = -1;

// Thread function to receive messages from the manager
void *receive_messages(void *arg) {
    (void)arg; // Avoid unused parameter warning

    int user_fifo_fd = open(user_fifo, O_RDONLY | O_NONBLOCK);
    if (user_fifo_fd == -1) {
        perror("Erro ao abrir FIFO do utilizador");
        return NULL;
    }

    fd_set read_fds;
    char buffer[MSG_MAX_LENGTH + TOPIC_NAME_LENGTH + 50]; // Increased size for formatting

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(user_fifo_fd, &read_fds);

        int ret = select(user_fifo_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ret > 0 && FD_ISSET(user_fifo_fd, &read_fds)) {
            int bytes_read = read(user_fifo_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                printf("\nMensagem recebida: %s\n", buffer);

                // Check for "exit" message
                if (strcmp(buffer, "exit") == 0) {
                    printf("Encerrando feed do utilizador...\n");
                    break;
                }

                printf("Digite um comando: ");
                fflush(stdout);
            }
        }
    }

    close(user_fifo_fd);
    return NULL;
}

// Function to request persistent messages for a topic
void request_persistent_messages(const char *username, const char *topic) {
    Message msg = {0};
    strcpy(msg.username, username);
    snprintf(msg.content, sizeof(msg.content), "show %s", topic);
    write(main_fifo_fd, &msg, sizeof(Message));
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: ./feed <username>\n");
        return 1;
    }

    char *username = argv[1];
    snprintf(user_fifo, sizeof(user_fifo), USER_FIFO_TEMPLATE, username);

    printf("Feed iniciado para o utilizador: %s\n", username);
    create_named_pipe(user_fifo);

    main_fifo_fd = open(MAIN_FIFO, O_WRONLY);
    if (main_fifo_fd == -1) {
        perror("Erro ao abrir FIFO principal");
        return 1;
    }

    // Register user with the manager
    Message msg = {0};
    strcpy(msg.username, username);
    snprintf(msg.content, sizeof(msg.content), "register");
    write(main_fifo_fd, &msg, sizeof(Message));
    printf("Utilizador %s registado com sucesso no servidor.\n", username);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, receive_messages, NULL);

    while (1) {
        printf("Digite um comando (topics, msg, subscribe, unsubscribe, exit): ");
        char command[20];
        scanf("%19s", command);

        if (strcmp(command, "topics") == 0) {
            // Request topics from the manager
            strcpy(msg.username, username);
            snprintf(msg.content, sizeof(msg.content), "topics");
            write(main_fifo_fd, &msg, sizeof(msg));
        } else if (strcmp(command, "msg") == 0) {
            printf("Digite uma mensagem (<topico> <duração> <mensagem>): ");
            char topic[TOPIC_NAME_LENGTH];
            int duration;
            char content[MSG_MAX_LENGTH];
            scanf("%19s %d %299[^\n]", topic, &duration, content);
            content[strcspn(content, "\n")] = '\0';
            if (snprintf(msg.content, sizeof(msg.content), "msg %s %d %s", topic, duration, content) >= (int)sizeof(msg.content)) {
                printf("Erro: Mensagem muito longa, tente novamente.\n");
                continue;
            }
            strcpy(msg.username, username);
        } else if (strcmp(command, "subscribe") == 0) {
            printf("Digite o tópico: ");
            char topic[TOPIC_NAME_LENGTH];
            scanf("%19s", topic);
            snprintf(msg.content, sizeof(msg.content), "subscribe %s", topic);
            strcpy(msg.username, username);

            // Request persistent messages for the topic
            request_persistent_messages(username, topic);
        } else if (strcmp(command, "unsubscribe") == 0) {
            printf("Digite o tópico: ");
            char topic[TOPIC_NAME_LENGTH];
            scanf("%19s", topic);
            snprintf(msg.content, sizeof(msg.content), "unsubscribe %s", topic);
            strcpy(msg.username, username);
        } else if (strcmp(command, "exit") == 0) {
            snprintf(msg.content, sizeof(msg.content), "exit");
            strcpy(msg.username, username);
            write(main_fifo_fd, &msg, sizeof(msg));
            break;
        } else {
            printf("Comando desconhecido. Tente novamente.\n");
            continue;
        }

        write(main_fifo_fd, &msg, sizeof(msg));
    }

    // Clean up resources
    pthread_join(thread_id, NULL);
    close(main_fifo_fd);
    unlink(user_fifo);
    printf("Feed encerrado com sucesso.\n");
    return 0;
}
