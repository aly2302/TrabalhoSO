#ifndef UTILS_H
#define UTILS_H

#include <semaphore.h>

// Funções para FIFOs
void create_named_pipe(const char *path);

// Funções para semáforos
void init_semaphore(sem_t *semaphore);
void lock_semaphore(sem_t *semaphore);
void unlock_semaphore(sem_t *semaphore);

#endif // UTILS_H

