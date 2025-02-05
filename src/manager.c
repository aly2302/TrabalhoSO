#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#define MAIN_FIFO "/tmp/manager_fifo"
const char *msg_file;


Topic topics[MAX_TOPICS];
User users[MAX_USERS];
int topic_count = 0;
int user_count = 0;

pthread_mutex_t topics_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;


void register_user(const char *username);
void remove_user(const char *username);
void list_topics();
void list_users();
void lock_topic(const char *topic_name);
void unlock_topic(const char *topic_name);
void show_topic_messages(const char *topic_name);
void subscribe_to_topic(const char *username, const char *topic_name);
void handle_message(Message *msg);
void *admin_commands(void *arg);
void *expire_messages(void *arg);

void handle_sigint(int sig) {
    (void)sig;   
    printf("\nEncerrando manager...\n");
    FILE *fp = fopen(msg_file, "w");
    if (fp) {
        for (int i = 0; i < topic_count; i++) {
            for (int j = 0; j < topics[i].message_count; j++) {
                if (topics[i].messages[j].duration > 0) {
                    fprintf(fp, "%s %s %d %s\n", topics[i].name, topics[i].messages[j].username, topics[i].messages[j].duration, topics[i].messages[j].content);
                }
            }
        }
        fclose(fp);
    }
    unlink(MAIN_FIFO);
    exit(0);
}


void load_persistent_messages() {
    pthread_mutex_lock(&topics_mutex);
    FILE *fp = fopen(msg_file, "r");
    if (fp) {
        char topic_name[TOPIC_NAME_LENGTH];
        char username[50];
        int remaining_time;
        char message_content[MSG_MAX_LENGTH];

        while (fscanf(fp, "%19s %49s %d %299[^\n]", topic_name, username, &remaining_time, message_content) == 4) {
            int topic_index = -1;
            for (int i = 0; i < topic_count; i++) {
                if (strcmp(topics[i].name, topic_name) == 0) {
                    topic_index = i;
                    break;
                }
            }
            if (topic_index == -1 && topic_count < MAX_TOPICS) {
                topic_index = topic_count++;
                strcpy(topics[topic_index].name, topic_name);
                topics[topic_index].is_locked = 0;
                topics[topic_index].message_count = 0;
                topics[topic_index].sub_count = 0;
            }
            if (topic_index != -1 && topics[topic_index].message_count < 5) {
                Message *msg = &topics[topic_index].messages[topics[topic_index].message_count++];
                strcpy(msg->username, username);
                strcpy(msg->content, message_content);
                msg->duration = remaining_time;
            }
        }
        fclose(fp);
    }
    pthread_mutex_unlock(&topics_mutex);
}


void register_user(const char *username) {
    pthread_mutex_lock(&users_mutex);
    if (user_count >= MAX_USERS) {
        printf("Limite máximo de utilizadores atingido.\n");
        pthread_mutex_unlock(&users_mutex);
        return;
    }
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            printf("Utilizador %s já está registado.\n", username);
            pthread_mutex_unlock(&users_mutex);
            return;
        }
    }
    strcpy(users[user_count].username, username);
    users[user_count].is_active = 1;
    user_count++;
    printf("Utilizador %s registado com sucesso.\n", username);
    pthread_mutex_unlock(&users_mutex);
}

void remove_user(const char *username) {
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i] = users[user_count - 1];
            user_count--;
            printf("Utilizador %s removido com sucesso.\n", username);

            // Notificar o feed do utilizador para encerrar
            char user_fifo[100];
            snprintf(user_fifo, sizeof(user_fifo), "/tmp/feed_%s", username);
            int user_fd = open(user_fifo, O_WRONLY | O_NONBLOCK);
            if (user_fd != -1) {
                const char *exit_msg = "exit";
                write(user_fd, exit_msg, strlen(exit_msg) + 1);
                close(user_fd);
            }
            pthread_mutex_unlock(&users_mutex);
            return;
        }
    }
    printf("Utilizador %s não encontrado.\n", username);
    pthread_mutex_unlock(&users_mutex);
}

void list_topics() {
    pthread_mutex_lock(&topics_mutex);
    printf("Lista de tópicos:\n");
    for (int i = 0; i < topic_count; i++) {
        printf("- %s%s\n", topics[i].name, topics[i].is_locked ? " (bloqueado)" : "");
    }
    pthread_mutex_unlock(&topics_mutex);
}


void remove_topic(const char *topic_name) {
    pthread_mutex_lock(&topics_mutex);

    int topic_index = -1;

    // Encontrar o tópico
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            topic_index = i;
            break;
        }
    }

    if (topic_index == -1) {
        printf("Tópico %s não encontrado.\n", topic_name);
        pthread_mutex_unlock(&topics_mutex);
        return;
    }

    for (int i = 0; i < topics[topic_index].sub_count; i++) {
        char user_fifo[100];
        snprintf(user_fifo, sizeof(user_fifo), "/tmp/feed_%s", topics[topic_index].subscribers[i]);
        int user_fd = open(user_fifo, O_WRONLY | O_NONBLOCK);
        if (user_fd >= 0) {
            const char *removal_msg = "Tópico removido.";
            write(user_fd, removal_msg, strlen(removal_msg) + 1);
            close(user_fd);
        }
    }

    for (int i = topic_index; i < topic_count - 1; i++) {
        topics[i] = topics[i + 1];
    }
    topic_count--;

    printf("Tópico %s removido com sucesso.\n", topic_name);

    pthread_mutex_unlock(&topics_mutex);
}


void list_users() {
    pthread_mutex_lock(&users_mutex);
    printf("Lista de utilizadores:\n");
    for (int i = 0; i < user_count; i++) {
        printf("- %s\n", users[i].username);
    }
    pthread_mutex_unlock(&users_mutex);
}

void lock_topic(const char *topic_name) {
    pthread_mutex_lock(&topics_mutex);
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            topics[i].is_locked = 1;
            printf("Tópico %s bloqueado com sucesso.\n", topic_name);
            pthread_mutex_unlock(&topics_mutex);
            return;
        }
    }
    printf("Tópico %s não encontrado.\n", topic_name);
    pthread_mutex_unlock(&topics_mutex);
}

void unlock_topic(const char *topic_name) {
    pthread_mutex_lock(&topics_mutex);
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            topics[i].is_locked = 0;
            printf("Tópico %s desbloqueado com sucesso.\n", topic_name);
            pthread_mutex_unlock(&topics_mutex);
            return;
        }
    }
    printf("Tópico %s não encontrado.\n", topic_name);
    pthread_mutex_unlock(&topics_mutex);
}

void list_topics_for_feed(const char *username) {
    pthread_mutex_lock(&topics_mutex);

    // Create the topics list
    char topics_list[1024] = "Topics:\n";
    for (int i = 0; i < topic_count; i++) {
        char line[128];
        snprintf(line, sizeof(line), "- %s%s\n", topics[i].name, topics[i].is_locked ? " (locked)" : "");
        strcat(topics_list, line);
    }

    pthread_mutex_unlock(&topics_mutex);

    // Send the list to the requesting feed
    char user_fifo[100];
    snprintf(user_fifo, sizeof(user_fifo), "/tmp/feed_%s", username);
    int user_fd = open(user_fifo, O_WRONLY | O_NONBLOCK);
    if (user_fd >= 0) {
        write(user_fd, topics_list, strlen(topics_list) + 1);
        close(user_fd);
    } else {
        perror("Erro ao abrir FIFO do feed");
    }
}




void show_topic_messages(const char *topic_name) {
    pthread_mutex_lock(&topics_mutex);
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            printf("Mensagens do tópico %s:\n", topic_name);
            for (int j = 0; j < topics[i].message_count; j++) {
                printf("- %s: %s (restam %d segundos)\n", topics[i].messages[j].username, topics[i].messages[j].content, topics[i].messages[j].duration);
            }
            pthread_mutex_unlock(&topics_mutex);
            return;
        }
    }
    printf("Tópico %s não encontrado.\n", topic_name);
    pthread_mutex_unlock(&topics_mutex);
}

void subscribe_to_topic(const char *username, const char *topic_name) {
    pthread_mutex_lock(&topics_mutex);
    int topic_index = -1;
    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            topic_index = i;
            break;
        }
    }
    if (topic_index == -1) {
        if (topic_count >= MAX_TOPICS) {
            printf("Limite máximo de tópicos atingido.\n");
            pthread_mutex_unlock(&topics_mutex);
            return;
        }
        topic_index = topic_count++;
        strcpy(topics[topic_index].name, topic_name);
        topics[topic_index].is_locked = 0;
        topics[topic_index].message_count = 0;
        topics[topic_index].sub_count = 0;
    }

    for (int i = 0; i < topics[topic_index].sub_count; i++) {
        if (strcmp(topics[topic_index].subscribers[i], username) == 0) {
            printf("Utilizador %s já está inscrito no tópico %s.\n", username, topic_name);
            pthread_mutex_unlock(&topics_mutex);
            return;
        }
    }

    strcpy(topics[topic_index].subscribers[topics[topic_index].sub_count++], username);
    printf("Utilizador %s inscrito no tópico %s.\n", username, topic_name);

    for (int i = 0; i < topics[topic_index].message_count; i++) {
        char user_fifo[100];
        snprintf(user_fifo, sizeof(user_fifo), "/tmp/feed_%s", username);
        int user_fd = open(user_fifo, O_WRONLY | O_NONBLOCK);
        if (user_fd >= 0) {
            write(user_fd, topics[topic_index].messages[i].content, strlen(topics[topic_index].messages[i].content) + 1);
            close(user_fd);
        }
    }

    pthread_mutex_unlock(&topics_mutex);
}


void unsubscribe_from_topic(const char *username, const char *topic_name) {
    pthread_mutex_lock(&topics_mutex);

    int topic_index = -1;

    for (int i = 0; i < topic_count; i++) {
        if (strcmp(topics[i].name, topic_name) == 0) {
            topic_index = i;
            break;
        }
    }

    if (topic_index == -1) {
        printf("Tópico %s não encontrado.\n", topic_name);
        pthread_mutex_unlock(&topics_mutex);
        return;
    }

    int user_index = -1;

    for (int i = 0; i < topics[topic_index].sub_count; i++) {
        if (strcmp(topics[topic_index].subscribers[i], username) == 0) {
            user_index = i;
            break;
        }
    }

    if (user_index == -1) {
        printf("Utilizador %s não está inscrito no tópico %s.\n", username, topic_name);
        pthread_mutex_unlock(&topics_mutex);
        return;
    }

    for (int i = user_index; i < topics[topic_index].sub_count - 1; i++) {
        strcpy(topics[topic_index].subscribers[i], topics[topic_index].subscribers[i + 1]);
    }
    topics[topic_index].sub_count--;

    printf("Utilizador %s removido do tópico %s.\n", username, topic_name);

    pthread_mutex_unlock(&topics_mutex);
}


void handle_message(Message *msg) {
    if (strcmp(msg->content, "register") == 0) {
        register_user(msg->username);
    } else if (strncmp(msg->content, "subscribe", 9) == 0) {
        char topic_name[TOPIC_NAME_LENGTH];
        sscanf(msg->content + 10, "%19s", topic_name); 
        subscribe_to_topic(msg->username, topic_name);
    } else if (strncmp(msg->content, "unsubscribe", 11) == 0) {
        char topic_name[TOPIC_NAME_LENGTH];
        sscanf(msg->content + 12, "%19s", topic_name); 
        unsubscribe_from_topic(msg->username, topic_name);
    } else if (strncmp(msg->content, "msg", 3) == 0) {
        pthread_mutex_lock(&topics_mutex);
        char topic_name[TOPIC_NAME_LENGTH];
        int duration;
        char message_content[MSG_MAX_LENGTH];
        sscanf(msg->content + 4, "%19s %d %299[^\n]", topic_name, &duration, message_content);

        for (int i = 0; i < topic_count; i++) {
            if (strcmp(topics[i].name, topic_name) == 0 && !topics[i].is_locked) {
                for (int j = 0; j < topics[i].sub_count; j++) {
                    if (strcmp(topics[i].subscribers[j], msg->username) != 0) {
                        char user_fifo[100];
                        snprintf(user_fifo, sizeof(user_fifo), "/tmp/feed_%s", topics[i].subscribers[j]);
                        int user_fd = open(user_fifo, O_WRONLY | O_NONBLOCK);
                        if (user_fd >= 0) {
                            write(user_fd, message_content, strlen(message_content) + 1);
                            close(user_fd);
                        }
                    }
                }

                if (topics[i].message_count < 5) {
                    strcpy(topics[i].messages[topics[i].message_count].username, msg->username);
                    strcpy(topics[i].messages[topics[i].message_count].content, message_content);
                    topics[i].messages[topics[i].message_count].duration = duration;
                    topics[i].message_count++;
                }
                pthread_mutex_unlock(&topics_mutex);
                printf("Mensagem enviada ao tópico %s.\n", topic_name);
                return;
            }
        }
        pthread_mutex_unlock(&topics_mutex);
        printf("Falha ao enviar mensagem. Tópico bloqueado ou inexistente.\n");
    } else if (strcmp(msg->content, "topics") == 0) {
       list_topics_for_feed(msg->username);
    } else if (strcmp(msg->content, "exit") == 0) {
        remove_user(msg->username);
    } else {
        printf("Comando desconhecido recebido: %s\n", msg->content);
    }
}



void *admin_commands(void *arg) {
    (void)arg;
    char command[50];
    while (1) {
        printf("Admin > ");
        fflush(stdout);
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        command[strcspn(command, "\n")] = 0; 

        if (strcmp(command, "users") == 0) {
            list_users();
        } else if (strncmp(command, "remove_user ", 12) == 0) {
            remove_user(command + 12);
        } else if (strncmp(command, "remove_topic ", 13) == 0) { 
            remove_topic(command + 13);
        } else if (strcmp(command, "topics") == 0) {
            list_topics();
        } else if (strncmp(command, "show ", 5) == 0) {
            show_topic_messages(command + 5);
        } else if (strncmp(command, "lock ", 5) == 0) {
            lock_topic(command + 5);
        } else if (strncmp(command, "unlock ", 7) == 0) {
            unlock_topic(command + 7);
        } else if (strcmp(command, "close") == 0) {
            handle_sigint(SIGINT);
            break;
        } else {
            printf("Comando desconhecido.\n");
        }
    }
    pthread_exit(NULL);
}



void *expire_messages(void *arg) {
    (void)arg;
    while (1) {
        sleep(1);
        pthread_mutex_lock(&topics_mutex);
        for (int i = 0; i < topic_count; i++) {
            for (int j = 0; j < topics[i].message_count; j++) {
                if (topics[i].messages[j].duration > 0) {
                    topics[i].messages[j].duration--;
                } else {
                    for (int k = j; k < topics[i].message_count - 1; k++) {
                        topics[i].messages[k] = topics[i].messages[k + 1];
                    }
                    topics[i].message_count--;
                    j--;
                }
            }
        }
        pthread_mutex_unlock(&topics_mutex);
    }
    return NULL;
}

int main() {
    signal(SIGINT, handle_sigint);
    unlink(MAIN_FIFO);
    create_named_pipe(MAIN_FIFO);
    
    msg_file = getenv("MSG_FICH");
      if (!msg_file) {
        msg_file = "messages.txt"; 
    }
    load_persistent_messages();

    printf("Manager iniciado e à espera de feeds...\n");

    pthread_t admin_thread, expire_thread;
    pthread_create(&admin_thread, NULL, admin_commands, NULL);
    pthread_create(&expire_thread, NULL, expire_messages, NULL);

    while (1) {
        int manager_fd = open(MAIN_FIFO, O_RDONLY);
        if (manager_fd == -1) {
            perror("Erro ao abrir FIFO principal");
            continue;
        }

        Message msg;
        while (read(manager_fd, &msg, sizeof(Message)) > 0) {
            printf("Mensagem recebida de %s: %s\n", msg.username, msg.content);
            handle_message(&msg);
        }
        close(manager_fd);
    }

    pthread_join(admin_thread, NULL);
    pthread_join(expire_thread, NULL);
    unlink(MAIN_FIFO);
    return 0;
}
