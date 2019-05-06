#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

extern char ** get_args();
char startingDirectory[100];

void changeDir(char ** args){
	char directory[100];
	if(args[1] == NULL){
		if(chdir(startingDirectory)<0){
			perror("Unable to change path\n");
		}	
	}
	else if(strcmp(args[1], "..") == 0){	
		chdir("..");	
	}
	else{
		char *path = args[1];
		if(chdir(path) < 0){
			perror("Unable to change path\n");
		}
	}
	getcwd(directory, 100);
	printf("%s\n", directory);
	
}

void execute(char ** args){
	if(strcmp(args[0], "cd") == 0){
		changeDir(args);
		return;
	}

	int fileCharInputIndex = -1;
	int fileCharOutputIndex = -1;
	int pipeIndex = -1;
	int ampersand = -1;
	int cdIndex = -1;

	pid_t pid1 = fork();
	if (pid1 < 0){
		perror("Fork Failed");
	}
	if(pid1 == 0){//begin if(pid1 == 0)
		for(int j = 0; args[j] != NULL; j++){
			if(*args[j] == '<'){
				fileCharInputIndex = j;
			}	
			else if(strcmp(args[j], ">&") == 0 || strcmp(args[j], ">>&") == 0){
				printf("Ampersans with output\n");
				fileCharOutputIndex = j;
				ampersand = j;
			}
			else if(*args[j] == '>' || strcmp(args[j], ">>") == 0){
				fileCharOutputIndex = j;
				printf("%s\n", args[j]);
			}	
			else if(strcmp(args[j], "|&") == 0){
				pipeIndex = j;
				ampersand = j;
			}
			else if(*args[j] == '|'){
				pipeIndex = j;
			}
			else if(strcmp(args[j], "cd")){
				cdIndex = j;
			}
			
		}
		if(pipeIndex != -1){
			int pfd[2];
			pipe(pfd);	
			
			int errpfd[2];
			char *argument;
			if(ampersand != -1){
				pipe(errpfd);
				argument = args[ampersand];
				printf("%s\n", argument);
				argument[strlen(argument)-1] = 0;
			}	

			args[pipeIndex] = NULL;
			pid_t pid2 = fork();
			if(pid2 == 0){
				//do second funtion
				//printf("beginning grandchild process\n");
				close(pfd[1]);
				if(ampersand != -1){
					close(errpfd[1]);
				}
				int savestdin = dup(0);
				if(dup2(pfd[0], 0) == -1 && ampersand != -1){
					dup2(errpfd[0], 0);
				}
				if(execvp(args[pipeIndex + 1], args + pipeIndex + 1) < 0){
					perror("Failed to execute in grandchild\n");
				}
				dup2(savestdin, 0);
				close(pfd[0]);
				if(ampersand != -1){
					close(errpfd[0]);
				}
				//printf("Exiting the grandchild process\n");
				exit(0);
			}
			else{
				//do left side function
				int savestdout = dup(1);
				close(pfd[0]);
				dup2(pfd[1], 1); //pipe's write side is now standard output

				int savestderr;
				if(ampersand != -1){
					close(errpfd[0]); //since writing, close the read end
					savestderr = dup(2);
					dup2(errpfd[1], 2); //redirect stderr ro the write end of the error pipe
				}
				//execvp(args[0], args); //execute and write to pipe
				execute(args); //No need to do a wait since execute has a built in wait function
				dup2(savestdout, 1);
				if(ampersand != -1){
					dup2(savestderr, 2);
				}
				close(0);
				close(1);
				close(pfd[1]);
				if(ampersand != -1){
					close(errpfd[1]); //done writing, so close the write end
				}
				//printf("Child process exiting after piped command\n");
				exit(0);
			}	
		}
		else if( fileCharInputIndex != -1){
			if(args[fileCharInputIndex + 1] == NULL){
				perror("No input file\n");
			}
			int savestdin = dup(0);
			int fd = open(args[fileCharInputIndex + 1], O_RDONLY);
			dup2(fd, 0);
			args[fileCharInputIndex] = NULL;
			args[fileCharInputIndex + 1] = NULL;
			execvp(args[0], args);
			dup2(savestdin, 0);
			close(fd);
			//printf("Child process exiting after input redirection\n");
			exit(0);
		}
		else if(fileCharOutputIndex != -1){
			printf("Ampersand = %d\n", ampersand);
			char *argument;
			if(ampersand != -1){
				argument = args[ampersand];
				printf("%s\n", argument);
				argument[strlen(argument)-1] = 0;
			}
			printf("%s\n", argument);
			int fd = open(args[fileCharOutputIndex + 1], O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);
			if(strcmp(args[fileCharOutputIndex], ">") == 0){ 
				fd = open(args[fileCharOutputIndex + 1], O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);
			}
			int savestderr;
			if(ampersand != -1){
				savestderr = dup(2);
				dup2(fd, 2);
			}
			int savestdout = dup(1);
			dup2(fd, 1);
			args[fileCharOutputIndex] = NULL;
			args[fileCharOutputIndex + 1] = NULL;
			execvp(args[0], args);
			dup2(savestdout, 1);
			if(ampersand != -1){
				dup2(savestderr, 2);
			}
			close(fd);
			//printf("Child process exiting after output redirection\n");
			exit(0);	
		}
		else{
			execvp(args[0], args);
			//printf("Child process exiting after normal execution\n");
			exit(0);
		}
	}	//end if(pid1 == 0)	
	else if (pid1 > 0){
		//printf("I am the parent, waiting for the child to end.\n");
		wait(NULL);
		//printf("Parent ending.\n");
	}
}

int main(){
	getcwd(startingDirectory, 100);

	int i;
	char ** args;
	char ** subArr;	
	int semi = 0;
	char semiArr[10];
	
	while (1) {
		printf ("Command ('exit' to quit): ");
		args = get_args();
		for (i = 0; args[i] != NULL; i++) {
			printf ("Argument %d: %s\n", i, args[i]);
			if(*args[i] == ';')
				semi = 1;
		}
		if (args[0] == NULL) {
		    printf ("No arguments on line!\n");
		}
		else if ( !strcmp (args[0], "exit")) {
		    printf ("Exiting...\n");
		    exit(0);
		}
		else if(i > 0){ 
			//if there are semicolons
			if(semi == 1){
				int curIndex = 0;
				int totalSemi = 0;
				for(int i = 0; args[i] != NULL; i++){
					if(*args[i] == ';'){
						semiArr[curIndex] = i;
						curIndex++;
						args[i] = NULL;
						totalSemi++;
					}
				}
				int totalExec = totalSemi;
				for(int h = 0; h < totalExec; h++){
					memcpy(subArr, args + h, semiArr[h]);
					//for(int lastSemiIndex = semiArr[totalSemi-1], n = 0; args[lastSemiIndex + 1] != NULL; lastSemiIndex++, n++){
					//	printf("before reassignment\n");
					//	printf("args last semi is %s\n", args[lastSemiIndex + 1]);
					//	subArr[n] = args[lastSemiIndex + 1];
					//	printf("%s\n", subArr[n]);
					//}
					printf("Before execute \n");
					execute(subArr);
					for(int k = 0; subArr[k] != NULL; k++){
						printf("%s\n", subArr[k]);
						subArr[k] = NULL;
					} 
					totalSemi--;
					if(totalSemi == 0){
						execute(args);
					}
				}
			}
			//if there is a cd	
			else{
				//printf("Start Execute\n");
				execute(args);
			}
		}
	}
}
