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



Topic topics[MAX_TOPICS];
User users[MAX_USERS];
int topic_count = 0;
int user_count = 0;
char *MSG_FILE = "messages.txt";

pthread_mutex_t topics_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function Prototypes
void register_user(const char *username);
void remove_user(const char *username);
void list_topics();
void list_users();
void lock_topic(const char *topic_name);
void unlock_topic(const char *topic_name);
void show_topic_messages(const char *topic_name);
void subscribe_to_topic(const char *username, const char *topic_name);
void unsubscribe_from_topic(const char* username, const char *topic_name);
void handle_message(Message *msg);
void *admin_commands(void *arg);
void *expire_messages(void *arg);
void save_persistent_messages();

// Signal handler for graceful termination
void handle_sigint(int sig) {
    (void)sig;   
    printf("\nEncerrando manager...\n");
    save_persistent_messages();
    unlink(MAIN_FIFO);
    exit(0);
}

void load_persistent_messages() {
    const char *msg_fich = getenv("MSG_FICH");
    if (msg_fich) {
        MSG_FILE = malloc(strlen(msg_fich) + 1);
        if (MSG_FILE) {
            strcpy(MSG_FILE, msg_fich);
        }
    }

    FILE *fp = fopen(MSG_FILE, "r");
    if (!fp) {
        printf("[INFO] No persistent messages file found or error loading it.\n");
        return;
    }

    char topic_name[TOPIC_NAME_LENGTH];
    char username[50];
    int remaining_time;
    char message_content[MSG_MAX_LENGTH];

    while (fscanf(fp, "%19s %49s %d %299[^\n]", topic_name, username, &remaining_time, message_content) == 4) {
        if (remaining_time <= 0) continue;

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
    printf("[INFO] Persistent messages loaded from %s\n", MSG_FILE);
}




void save_persistent_messages() {
    FILE *fp = fopen(MSG_FILE, "w");
    if (!fp) {
        perror("[ERROR] Could not open file to save persistent messages");
        return;
    }

    for (int i = 0; i < topic_count; i++) {
        for (int j = 0; j < topics[i].message_count; j++) {
            if (topics[i].messages[j].duration > 0) {
                fprintf(fp, "%s %s %d %s\n", topics[i].name, topics[i].messages[j].username, topics[i].messages[j].duration, topics[i].messages[j].content);
            }
        }
    }

    fclose(fp);
    printf("[INFO] Persistent messages saved to %s\n", MSG_FILE);
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
   

	// Send persistent messages to the new subscriber
    char user_fifo[100];
    sprintf(user_fifo, "/tmp/feed_%s", username);
    int user_fd = open(user_fifo, O_WRONLY | O_NONBLOCK);
    if (user_fd >= 0) {
        for (int i = 0; i < topics[topic_index].message_count; i++) {
            char notification[MSG_MAX_LENGTH + 100];
            snprintf(notification, sizeof(notification), "[%s]: %s", topics[topic_index].messages[i].username, topics[topic_index].messages[i].content);
            write(user_fd, notification, strlen(notification) + 1);
        }
        close(user_fd);
    }

    printf("Utilizador %s inscrito no tópico %s.\n", username, topic_name);
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
        printf("[ERROR] Topic %s does not exist.\n", topic_name);
        pthread_mutex_unlock(&topics_mutex);
        return;
    }

    // Find and remove the user from the topic's subscribers
    int user_found = 0;
    for (int i = 0; i < topics[topic_index].sub_count; i++) {
        if (strcmp(topics[topic_index].subscribers[i], username) == 0) {
            user_found = 1;
            for (int j = i; j < topics[topic_index].sub_count - 1; j++) {
                strcpy(topics[topic_index].subscribers[j], topics[topic_index].subscribers[j + 1]);
            }
            topics[topic_index].sub_count--;
            break;
        }
    }

    if (!user_found) {
        printf("[ERROR] User %s is not subscribed to topic %s.\n", username, topic_name);
    } else {
        printf("[INFO] User %s unsubscribed from topic %s.\n", username, topic_name);
    }

    pthread_mutex_unlock(&topics_mutex);
}




void handle_message(Message *msg) {
    if (strncmp(msg->content, "register", 8) == 0) {
        // Register the user
        register_user(msg->username);

    } else if (strncmp(msg->content, "subscribe", 9) == 0) {
        // Subscribe the user to a topic
        char topic_name[TOPIC_NAME_LENGTH];
        sscanf(msg->content + 10, "%19s", topic_name);
        subscribe_to_topic(msg->username, topic_name);

    } else if (strncmp(msg->content, "unsubscribe", 11) == 0) {
        // Unsubscribe the user from a topic
        char topic_name[TOPIC_NAME_LENGTH];
        sscanf(msg->content + 12, "%19s", topic_name);
        unsubscribe_from_topic(msg->username, topic_name);

    } else if (strncmp(msg->content, "msg", 3) == 0) {
        // Send a message to a topic
        pthread_mutex_lock(&topics_mutex);

        char topic_name[TOPIC_NAME_LENGTH];
        int duration;
        char message_content[MSG_MAX_LENGTH];
        sscanf(msg->content + 4, "%19s %d %299[^\n]", topic_name, &duration, message_content);

        for (int i = 0; i < topic_count; i++) {
            if (strcmp(topics[i].name, topic_name) == 0) {
                // Check if the user is subscribed to the topic
                int is_subscribed = 0;
                for (int j = 0; j < topics[i].sub_count; j++) {
                    if (strcmp(topics[i].subscribers[j], msg->username) == 0) {
                        is_subscribed = 1;
                        break;
                    }
                }

                if (!is_subscribed) {
                    printf("[ERROR] User %s is not subscribed to topic %s.\n", msg->username, topic_name);
                    pthread_mutex_unlock(&topics_mutex);
                    return;
                }

                if (topics[i].is_locked) {
                    printf("[ERROR] Topic %s is locked. Message not sent.\n", topic_name);
                    pthread_mutex_unlock(&topics_mutex);
                    return;
                }

                // Send the message to all subscribers
                for (int j = 0; j < topics[i].sub_count; j++) {
                    char user_fifo[100];
                    sprintf(user_fifo, "/tmp/feed_%s", topics[i].subscribers[j]);
                    int user_fd = open(user_fifo, O_WRONLY | O_NONBLOCK);
                    if (user_fd >= 0) {
                        char notification[MSG_MAX_LENGTH + 100];
                        snprintf(notification, sizeof(notification), "[%s]: %s", msg->username, message_content);
                        write(user_fd, notification, strlen(notification) + 1);
                        close(user_fd);
                    }
                }

                // Add the message to the topic's persistent storage
                if (topics[i].message_count < 5) {
                    Message *new_msg = &topics[i].messages[topics[i].message_count++];
                    strcpy(new_msg->username, msg->username);
                    strcpy(new_msg->content, message_content);
                    new_msg->duration = duration;
                }

                pthread_mutex_unlock(&topics_mutex);
                printf("[INFO] Message sent to topic %s by user %s.\n", topic_name, msg->username);
                return;
            }
        }

        pthread_mutex_unlock(&topics_mutex);
        printf("[ERROR] Topic %s does not exist.\n", topic_name);

    } else if (strncmp(msg->content, "exit", 4) == 0) {
        // Remove the user from the platform
        remove_user(msg->username);

    } else if (strncmp(msg->content, "topics", 6) == 0) {
        // List all available topics
        list_topics();

    } else if (strncmp(msg->content, "lock", 4) == 0) {
        // Lock a topic (admin command)
        char topic_name[TOPIC_NAME_LENGTH];
        sscanf(msg->content + 5, "%19s", topic_name);
        lock_topic(topic_name);

    } else if (strncmp(msg->content, "unlock", 6) == 0) {
        // Unlock a topic (admin command)
        char topic_name[TOPIC_NAME_LENGTH];
        sscanf(msg->content + 7, "%19s", topic_name);
        unlock_topic(topic_name);

    } else if (strncmp(msg->content, "show", 4) == 0) {
        // Show all messages of a topic (admin command)
        char topic_name[TOPIC_NAME_LENGTH];
        sscanf(msg->content + 5, "%19s", topic_name);
        show_topic_messages(topic_name);

    } else {
        // Handle unknown commands
        printf("[ERROR] Unknown command received: %s\n", msg->content);
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
        command[strcspn(command, "\n")] = 0; // Remover newline

        if (strcmp(command, "list_users") == 0) {
            list_users();
        } else if (strncmp(command, "remove ", 7) == 0) {
            remove_user(command + 7);
        } else if (strcmp(command, "list_topics") == 0) {
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
    return NULL;
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

    load_persistent_messages();

    printf("Manager iniciado e aguardando conexões...\n");

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
