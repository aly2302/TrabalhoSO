#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAIN_FIFO "/tmp/manager_fifo"

Topic topics[MAX_TOPICS];
User users[MAX_USERS];
int topic_count = 0;
int user_count = 0;

void register_user(const char *username) {
    if (user_count >= MAX_USERS) {
        printf("Limite máximo de utilizadores atingido.\n");
        return;
    }
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            printf("Utilizador %s já está registado.\n", username);
            return;
        }
    }
    strcpy(users[user_count].username, username);
    users[user_count].is_active = 1;
    user_count++;
    printf("Utilizador %s registado com sucesso.\n", username);
}

void handle_message(Message *msg) {
    if (strcmp(msg->command, "register") == 0) {
        register_user(msg->username);
        printf("Utilizador conectado: %s\n", msg->username);
    } else if (strcmp(msg->command, "subscribe") == 0) {
        for (int i = 0; i < topic_count; i++) {
            if (strcmp(topics[i].name, msg->topic) == 0) {
                strcpy(topics[i].subscribers[topics[i].sub_count], msg->username);
                topics[i].sub_count++;
                printf("Utilizador %s inscrito no tópico %s.\n", msg->username, msg->topic);
                return;
            }
        }
        if (topic_count < MAX_TOPICS) {
            strcpy(topics[topic_count].name, msg->topic);
            strcpy(topics[topic_count].subscribers[0], msg->username);
            topics[topic_count].sub_count = 1;
            topics[topic_count].is_locked = 0;
            topics[topic_count].message_count = 0;
            topic_count++;
            printf("Tópico %s criado e utilizador %s inscrito.\n", msg->topic, msg->username);
        }
    } else if (strcmp(msg->command, "msg") == 0) {
        for (int i = 0; i < topic_count; i++) {
            if (strcmp(topics[i].name, msg->topic) == 0 && !topics[i].is_locked) {
                for (int j = 0; j < topics[i].sub_count; j++) {
                    if (strcmp(topics[i].subscribers[j], msg->username) != 0) {
                        char user_fifo[100];
                        sprintf(user_fifo, "/tmp/feed_%s", topics[i].subscribers[j]);
                        int user_fd = open(user_fifo, O_WRONLY | O_NONBLOCK);
                        if (user_fd >= 0) {
                            write(user_fd, msg->message, strlen(msg->message) + 1);
                            close(user_fd);
                        }
                    }
                }
                if (topics[i].message_count < 5 && msg->duration > 0) {
                    strcpy(topics[i].messages[topics[i].message_count], msg->message);
                    topics[i].message_count++;
                }
                printf("Mensagem enviada no tópico %s.\n", msg->topic);
                return;
            }
        }
        printf("Falha ao enviar mensagem. Tópico bloqueado ou inexistente.\n");
    } else if (strcmp(msg->command, "exit") == 0) {
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, msg->username) == 0) {
                users[i].is_active = 0;
                printf("Utilizador %s desconectado.\n", msg->username);
                return;
            }
        }
    }
}

void list_users() {
    printf("Utilizadores conectados:\n");
    for (int i = 0; i < user_count; i++) {
        if (users[i].is_active) {
            printf("- %s\n", users[i].username);
        }
    }
}

void list_topics() {
    printf("Tópicos existentes:\n");
    for (int i = 0; i < topic_count; i++) {
        printf("- %s (%d inscritos)\n", topics[i].name, topics[i].sub_count);
    }
}

int main() {
    unlink(MAIN_FIFO);
    create_named_pipe(MAIN_FIFO);

    printf("Manager iniciado e aguardando conexões...\n");

    while (1) {
        int manager_fd = open(MAIN_FIFO, O_RDONLY);
        if (manager_fd == -1) {
            perror("Erro ao abrir FIFO principal");
            continue;
        }

        Message msg;
        while (read(manager_fd, &msg, sizeof(Message)) > 0) {
            printf("Comando recebido: %s de %s\n", msg.command, msg.username);
            if (strcmp(msg.command, "list_users") == 0) {
                list_users();
            } else if (strcmp(msg.command, "topics") == 0) {
                list_topics();
            } else {
                handle_message(&msg);
            }
        }
        close(manager_fd);
    }

    unlink(MAIN_FIFO);
    return 0;
}
