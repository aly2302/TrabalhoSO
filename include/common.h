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

// Estrutura de Mensagem
typedef struct {
    char username[50];
    char content[MSG_MAX_LENGTH];
    int duration; // Tempo de vida (persistência)
} Message;

// Estrutura de Tópico
typedef struct {
    char name[TOPIC_NAME_LENGTH];
    int is_locked;
    char subscribers[MAX_USERS][50];
    int sub_count;
    Message messages[5];
    int message_count;
} Topic;

#endif // COMMON_H
