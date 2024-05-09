#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <pthread.h>
#include<fcntl.h>
#include <semaphore.h>

#define SECONDARY_SERVER_2_MSG_TYPE 5

#define MAX_MSG_SIZE 256
#define MSG_KEY 1234
#define SHM_KEY 5678

struct msg_buffer {
    long msg_type;
    char msg_text[MAX_MSG_SIZE];
};

sem_t *fileSemaphore;

void *handleTraversal(void *arg) {
    struct msg_buffer *request = (struct msg_buffer *)arg;

    int seq_no, op_no;
    char filename[10];
    sscanf(request->msg_text, "%d %d %s", &seq_no, &op_no, filename);

    key_t shmkey = ftok("/tmp", SHM_KEY);

    int shmid = shmget(shmkey,0,0);
    if(shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    char *shared_memory = (char *)shmat(shmid, NULL, 0);
    if(shared_memory == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    char *ptr;
    int startNode = strtol(shared_memory, &ptr, 10);

    sem_wait(fileSemaphore);

    if(op_no == 3) {
        printf("Perform DFS, start node: %d\n",startNode);
    } else if(op_no == 4) {
        printf("Perform BFS, start node: %d\n",startNode);
    } else {
        printf("Thread: Unknown Operation: %d\n", op_no);
    }

    sem_post(fileSemaphore);

    if(shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    free(request);
    pthread_exit(NULL);
}

int main() {
    key_t key;
    int msg_id;
    struct msg_buffer message;

    fileSemaphore = sem_open("/fileSemaphore", O_EXCL, 0644, 1);
    if(fileSemaphore == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    key = ftok("/tmp", MSG_KEY);
    if(key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    msg_id = msgget(key, 0666);
    if (msg_id == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    printf("Secondary Server 2: Connected to Message Queue with key %d\n", key);

    while(1) {
        if(msgrcv(msg_id, &message, sizeof(message.msg_text), SECONDARY_SERVER_2_MSG_TYPE, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        printf("Secondary Server 2: Received message: %s\n", message.msg_text);

        pthread_t tid;
        struct msg_buffer *request = malloc(sizeof(struct msg_buffer));
        if(request == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        
        memcpy(request, &message, sizeof(struct msg_buffer));
        if(pthread_create(&tid, NULL, handleTraversal, (void *)request)) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        pthread_detach(tid);
    }

    sem_close(fileSemaphore);
    sem_unlink("/fileSemaphore");

    printf("Secondary Server 2: Exiting\n");
    return 0;
}