
















#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {
    "cp","touch","mkdir","ls","pwd","cat",
    "grep","chmod","diff","cd","exit","help","sendmsg"
};

struct message {
    char source[50];
    char target[50];
    char msg[200];
};

void terminate(int sig) {
    (void)sig;
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

void sendmsg(char *user, char *target, char *msg) {
    struct message m;

    strcpy(m.source, user);
    strcpy(m.target, target);
    strcpy(m.msg, msg);

    int fd = open("serverFIFO", O_WRONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    write(fd, &m, sizeof(struct message));
    close(fd);
}

void* messageListener(void *arg) {
    (void)arg;
    struct message m;

    int fd = open(uName, O_RDONLY);
    if (fd < 0) {
        perror("open");
        pthread_exit((void*)0);
    }

    while (1) {
        int n = (int)read(fd, &m, sizeof(struct message));
        if (n > 0) {
            printf("Incoming message from %s: %s\n", m.source, m.msg);
            fflush(stdout);
        }
    }

    close(fd);
    pthread_exit((void*)0);
}

int isAllowed(const char *cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./rsh username\n");
        return 1;
    }

    strcpy(uName, argv[1]);

    signal(SIGINT, terminate);

    pthread_t tid;
    pthread_create(&tid, NULL, messageListener, NULL);

    char line[256];

    while (1) {
        fprintf(stderr, "rsh>");

        if (fgets(line, 256, stdin) == NULL) {
            return 0;
        }

        if (strcmp(line, "\n") == 0) {
            continue;
        }

        line[strlen(line) - 1] = '\0';

        char lineCopy[256];
        strcpy(lineCopy, line);

        char *args[21];
        int argCount = 0;

        char *token = strtok(line, " ");
        while (token != NULL && argCount < 20) {
            args[argCount] = token;
            argCount++;
            token = strtok(NULL, " ");
        }
        args[argCount] = NULL;

        if (argCount == 0) {
            continue;
        }

        if (!isAllowed(args[0])) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(args[0], "exit") == 0) {
            return 0;
        }

        if (strcmp(args[0], "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("%d: %s\n", i + 1, allowed[i]);
            }
            continue;
        }

        if (strcmp(args[0], "cd") == 0) {
            if (argCount > 2) {
                printf("-rsh: cd: too many arguments\n");
                continue;
            }
            if (argCount == 2) {
                chdir(args[1]);
            }
            continue;
        }

        if (strcmp(args[0], "sendmsg") == 0) {
            if (argCount < 3) {
                continue;
            }

            char *target = args[1];

            char *msgStart = strstr(lineCopy, target);
            if (msgStart != NULL) {
                msgStart += strlen(target);
                while (*msgStart == ' ') {
                    msgStart++;
                }
                sendmsg(uName, target, msgStart);
            }

            continue;
        }

        pid_t pid;
        int status;

        if (posix_spawnp(&pid, args[0], NULL, NULL, args, environ) == 0) {
            waitpid(pid, &status, 0);
        }
    }

    return 0;
}
