#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep", "chmod", "diff", "cd", "exit", "help", "sendmsg"};

struct message
{
	char source[50];
	char target[50];
	char msg[200];
};

void terminate(int sig)
{
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

void sendmsg(char *user, char *target, char *msg)
{
	struct message message_container;
	strcpy(message_container.source, user);
	strcpy(message_container.target, target);
	strcpy(message_container.msg, msg);

	int fd = open("serverFIFO", O_WRONLY);

	write(fd, &message_container, sizeof(struct message));
	close(fd);
}

void *messageListener(void *arg)
{
    int fd = open(uName, O_RDONLY);
    
    struct message message_container;
    
    while (1) {
        
        if (read(fd, &message_container, sizeof(struct message)) > 0) {
            printf("\nIncoming message from %s: %s\n", message_container.source, message_container.msg);
            fprintf(stderr, "rsh>"); 
            fflush(stdout);
        }
    }
    
    close(fd);
    pthread_exit(NULL);
}

int isAllowed(const char *cmd)
{
	int i;
	for (i = 0; i < N; i++)
	{
		if (strcmp(cmd, allowed[i]) == 0)
		{
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	pid_t pid;
	char **cargv;
	char *path;
	char line[256];
	int status;
	posix_spawnattr_t attr;

	if (argc != 2)
	{
		printf("Usage: ./rsh <username>\n");
		exit(1);
	}
	signal(SIGINT, terminate);

	strcpy(uName, argv[1]);

	// TODO:
	// create the message listener thread
	pthread_t listenerThread;
	pthread_create(&listenerThread, NULL, messageListener, NULL);

	while (1)
	{
		fprintf(stderr, "rsh>");

		if (fgets(line, 256, stdin) == NULL)
			continue;

		if (strcmp(line, "\n") == 0)
			continue;

		line[strlen(line) - 1] = '\0';

		char cmd[256];
		char line2[256];
		char *command[20];
		char *cmdstr = malloc(strlen(line) + 1);
		strcpy(line2, line);
		strcpy(cmd, strtok(line, " "));

		if (!isAllowed(cmd))
		{
			printf("NOT ALLOWED!\n");
			continue;
		}

		if (strcmp(cmd, "sendmsg") == 0)
		{

			int counter = 0;
			command[2] = NULL;
			while (cmdstr != NULL)
			{	
				if (counter > 1){
					if(command[2] == NULL){
						command[2] = malloc(strlen(cmdstr)+1);
						strcpy(command[2], cmdstr);
					}
					else{
						command[2] = realloc(command[2], strlen(cmdstr) + 2 + strlen(command[2]));
						strcat(command[2], " ");
						strcat(command[2], cmdstr);
						
					}
				}

				else{
					command[counter] = malloc(strlen(cmdstr) + 1);
					strcpy(command[counter], cmdstr);
				}

				cmdstr = strtok(NULL, " ");
				counter += 1;
			}

			if (command[1] == NULL)
			{
				printf("sendmsg: you have to specify target user\n");
			}

			else if (command[2] == NULL)
			{
				printf("sendmsg: you have to enter a message\n");
			}

			char* target = strdup(command[1]);
			char* message = strdup(command[2]);

			sendmsg(argv[1], command[1], command[2]);

			continue;
		}

		if (strcmp(cmd, "exit") == 0)
			break;

		if (strcmp(cmd, "cd") == 0)
		{
			char *targetDir = strtok(NULL, " ");
			if (strtok(NULL, " ") != NULL)
			{
				printf("-rsh: cd: too many arguments\n");
			}
			else
			{
				chdir(targetDir);
			}
			continue;
		}

		if (strcmp(cmd, "help") == 0)
		{
			printf("The allowed commands are:\n");
			for (int i = 0; i < N; i++)
			{
				printf("%d: %s\n", i + 1, allowed[i]);
			}
			continue;
		}

		cargv = (char **)malloc(sizeof(char *));
		cargv[0] = (char *)malloc(strlen(cmd) + 1);
		path = (char *)malloc(9 + strlen(cmd) + 1);
		strcpy(path, cmd);
		strcpy(cargv[0], cmd);

		char *attrToken = strtok(line2, " "); /* skip cargv[0] which is completed already */
		attrToken = strtok(NULL, " ");
		int n = 1;
		while (attrToken != NULL)
		{
			n++;
			cargv = (char **)realloc(cargv, sizeof(char *) * n);
			cargv[n - 1] = (char *)malloc(strlen(attrToken) + 1);
			strcpy(cargv[n - 1], attrToken);
			attrToken = strtok(NULL, " ");
		}
		cargv = (char **)realloc(cargv, sizeof(char *) * (n + 1));
		cargv[n] = NULL;

		// Initialize spawn attributes
		posix_spawnattr_init(&attr);

		// Spawn a new process
		if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0)
		{
			perror("spawn failed");
			exit(EXIT_FAILURE);
		}

		// Wait for the spawned process to terminate
		if (waitpid(pid, &status, 0) == -1)
		{
			perror("waitpid failed");
			exit(EXIT_FAILURE);
		}

		// Destroy spawn attributes
		posix_spawnattr_destroy(&attr);
	}
	return 0;
}
