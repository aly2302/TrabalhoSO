#ifndef COMMON_H
#define COMMON_H

#define MAX_USERS 10
#define MAX_TOPICS 20
#define MSG_MAX_LENGTH 300
#define TOPIC_NAME_LENGTH 20

#define MAIN_FIFO "/tmp/manager_fifo" // Caminho do FIFO principal

// Estrutura de Utilizador
typedef struct {
    char username[50];
    int is_active;
} User;

// Estrutura de Tópico
typedef struct {
    char name[TOPIC_NAME_LENGTH];
    int is_locked;
    char subscribers[MAX_USERS][50];
    int sub_count;
    char messages[5][MSG_MAX_LENGTH];
    int message_count;
} Topic;

// Estrutura de Mensagem
typedef struct {
    char username[50];
    char command[20];
    char topic[TOPIC_NAME_LENGTH];
    char message[MSG_MAX_LENGTH];
    int duration; // Tempo de vida (persistência)
} Message;

#endif // COMMON_H
