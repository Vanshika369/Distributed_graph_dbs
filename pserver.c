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

#define PRIMARY_SERVER_MSG_TYPE 3
#define RESPONSE_TYPE 2

#define MAX_MSG_SIZE 256
#define MSG_KEY 1234
#define SHM_KEY 5678

struct msg_buffer {
    long msg_type;
    char msg_text[MAX_MSG_SIZE];
};

sem_t *fileSemaphore;

void sendMessageToClient(const char* response) {
    key_t key = ftok("/tmp", MSG_KEY);
    int msg_id = msgget(key, 0666);
    if (msg_id == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    struct msg_buffer message;
    message.msg_type = RESPONSE_TYPE;

    snprintf(message.msg_text, MAX_MSG_SIZE, "%s", response);
    if (msgsnd(msg_id, &message, sizeof(message.msg_text), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
}

void *handleWriteRequest(void *arg) {
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

    // Attach shared memory segment
    char *shared_memory = (char *)shmat(shmid, NULL, 0);
    if(shared_memory == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    char *ptr;
    int nodes = strtol(shared_memory, &ptr, 10);

    int adj[31][31];
    for (int i = 1; i <= nodes; ++i) {
        for (int j = 1; j <= nodes; ++j) {
            adj[i][j] = strtol(ptr, &ptr, 10);
        }
    }

    sem_wait(fileSemaphore);

    if(op_no == 1) {
        printf("Thread: Creating a new graph file: %s\n", filename);
        
        char filepath[50];
        snprintf(filepath, sizeof(filepath), "graphs/%s", filename);

        FILE *file = fopen(filepath, "w");
        if(file == NULL) {
            perror("Error creating file");
            exit(EXIT_FAILURE);
        }
        sleep(35);
        fprintf(file, "%d\n", nodes);

        for(int i=1; i<=nodes; i++) {
            for(int j=1; j<=nodes; j++) {
                fprintf(file, "%d ", adj[i][j]);
            }
            fprintf(file,"\n");
        }
        fclose(file);
        printf("File modi succesfully\n");
        sendMessageToClient("File successfully added\n");

    } else if(op_no == 2) {
        printf("Thread: Modifying an existing graph file: %s\n", filename);
        char filepath[50];
        snprintf(filepath, sizeof(filepath), "graphs/%s", filename);

        FILE *file = fopen(filepath, "w");
        if(file == NULL) {
            perror("Error creating file");
            exit(EXIT_FAILURE);
        }
  
        fprintf(file, "%d\n", nodes);

        for(int i=1; i<=nodes; i++) {
            for(int j=1; j<=nodes; j++) {
                fprintf(file, "%d ", adj[i][j]);
            }
            fprintf(file,"\n");
        }
        fclose(file);
        printf("File closed succesfully\n");
        sendMessageToClient("File successfully modified\n");
    } else {
        printf("Thread: Unknown Operation: %d\n", op_no);
    }

    sem_post(fileSemaphore);

    // Detach shared memory segment
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

    sem_unlink("/fileSemaphore");
    fileSemaphore = sem_open("/fileSemaphore", O_CREAT | O_EXCL, 0644, 1);
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

    printf("Primary Server: Connected to Message Queue with key %d\n", key);

    while(1) {
        if(msgrcv(msg_id, &message, sizeof(message.msg_text), PRIMARY_SERVER_MSG_TYPE, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        printf("Primary Server: Received Message: %s\n", message.msg_text);

        // Create a new thread to handle the write request
        pthread_t tid;
        struct msg_buffer *request = malloc(sizeof(struct msg_buffer));
        if(request == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        // Copy the received message to the dynamically allocated structure
        memcpy(request, &message, sizeof(struct msg_buffer));

        if(pthread_create(&tid, NULL, handleWriteRequest, (void *)request)) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        // Detach the thread to allow it to run independently
        pthread_detach(tid);
    }

    sem_close(fileSemaphore);
    sem_unlink("/fileSemaphore");

    printf("Primary Server: Exiting\n");
    return 0;
}
