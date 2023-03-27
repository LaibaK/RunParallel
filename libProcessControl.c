#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>

#include "libParseArgs.h"
#include "libProcessControl.h"

/**
 * parallelDo -n NUM -o OUTPUT_DIR COMMAND_TEMPLATE ::: [ ARGUMENT_LIST ...]
 * build and execute shell command lines in parallel
 */

/**
 * create and return a newly malloced command from commandTemplate and argument
 * the new command replaces each occurrance of {} in commandTemplate with argument
 */
char *createCommand(char *commandTemplate, char *argument){
    int temp_len = strlen(commandTemplate);
    int arg_len = strlen(argument);
    int count = 0;
    int i, j;

    // number of "{}" in cmd template
    for (i = 0; i < temp_len - 1; i++) {
        if (commandTemplate[i] == '{' && commandTemplate[i+1] == '}') {
            count++;
        }
    }

    // Allocate memory for new command + space for the argument
    char *command = malloc(sizeof(char) * (temp_len - (2*count) + arg_len + 1));

    // Replace each "{}" with the argument in the new cmd
    i = 0;
    j = 0;
    while (commandTemplate[i] != '\0') {
        if (commandTemplate[i] == '{' && commandTemplate[i+1] == '}') {
            for (int k = 0; k < arg_len; k++) {
                command[j] = argument[k];
                j++;
            }
            i += 2; // next token
        }
        else {
            command[j] = commandTemplate[i];
            i++;
            j++;
        }
    }
    command[j] = '\0';
	return command; 

}

typedef struct PROCESS_STRUCT {
	int pid;
	int ifExited;
	int exitStatus;
	int status;
	char *command;
} PROCESS_STRUCT;

typedef struct PROCESS_CONTROL {
	int numProcesses;
	int numRunning; 
	int maxNumRunning;
	int numCompleted;
	PROCESS_STRUCT *process;
} PROCESS_CONTROL;

PROCESS_CONTROL processControl;

void printSummary(){
	printf("%d %d %d\n", processControl.numProcesses, processControl.numCompleted, processControl.numRunning);
}
void printSummaryFull(){
	printSummary();
	for(int i=0;i<processControl.numCompleted; i++){
		printf("%d %d %d %s\n", 
				processControl.process[i].pid,
				processControl.process[i].ifExited,
				processControl.process[i].exitStatus,
				processControl.process[i].command);
	}
}
/**
 * find the record for pid and update it based on status
 * status has information encoded in it, you will have to extract it
 */
void updateStatus(int pid, int status){

    for (int i = 0; i < processControl.numProcesses; i++) {
        if (processControl.process[i].pid == pid) {
            processControl.process[i].status = status;
            if (WIFEXITED(status)) {
                processControl.process[i].ifExited = 1;
                processControl.process[i].exitStatus = WEXITSTATUS(status);
            } else {
                processControl.process[i].ifExited = 0;
            }
            break;
        }
    }
}

void handler(int signum){

    if (signum == SIGUSR1) {
        printSummary();
    } else if (signum == SIGUSR2) {
        printSummaryFull();
    }

}
/**
 * This function does the bulk of the work for parallelDo. This is called
 * after understanding the command line arguments. runParallel 
 * uses pparams to generate the commands (createCommand), 
 * forking, redirecting stdout and stderr, waiting for children, ...
 * Instead of passing around variables, we make use of globals pparams and
 * processControl. 
 */
int runParallel(){

	// YOUR CODE GOES HERE
	// THERE IS A LOT TO DO HERE!!
	// TAKE SMALL STEPS, MAKE SURE THINGS WORK AND THEN MOVE FORWARD.

	    
    processControl.numProcesses = pparams.argumentListLen;
    processControl.numRunning = 0;
    processControl.maxNumRunning = pparams.maxNumRunning;
    processControl.numCompleted = 0;
    processControl.process = (PROCESS_STRUCT *) malloc(sizeof(PROCESS_STRUCT) * processControl.numProcesses);

    if (processControl.process == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    // Set up signal handlers
    signal(SIGUSR1, handler);
    signal(SIGUSR2, handler);

    // Fork and execute commands
    for (int i = 0; i < pparams.argumentListLen; i++) {
        char *command = createCommand(pparams.commandTemplate, pparams.argumentList[i]);
        int pid = fork();
        if (pid == -1) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // Child process
            // Redirect stdout and stderr to output directory
            char outputFile[1024];
            sprintf(outputFile, "%s/%d.out", pparams.outputDir, getpid());
            int outFd = open(outputFile, O_CREAT | O_WRONLY, 0644);
            if (outFd == -1) {
                perror("open failed");
                exit(EXIT_FAILURE);
            }
            if (dup2(outFd, STDOUT_FILENO) == -1) {
                perror("dup2 failed");
                exit(EXIT_FAILURE);
            }
            if (dup2(outFd, STDERR_FILENO) == -1) {
                perror("dup2 failed");
                exit(EXIT_FAILURE);
            }
            close(outFd);
            // Execute command
            execl("/bin/sh", "sh", "-c", command, (char *) NULL);
            perror("execl failed");
            exit(EXIT_FAILURE);
        } else { // Parent process
            // Update process control
            processControl.process[i].pid = pid;
            processControl.process[i].ifExited = 0;
            processControl.process[i].status = -1;
            processControl.process[i].command = command;
            processControl.numRunning++;
            // Wait for child process if necessary
            if (processControl.numRunning >= processControl.maxNumRunning) {
                int status;
                pid_t pid = wait(&status);
                if (pid == -1) {
                    perror("wait failed");
                    exit(EXIT_FAILURE);
                }
                processControl.numRunning--;
                processControl.numCompleted++;
                updateStatus(pid, status);
            }
        }
    }

    // Wait for remaining child processes
    while (processControl.numRunning > 0) {
        int status;
        pid_t pid = wait(&status);
        if (pid == -1) {
            perror("wait failed");
            exit(EXIT_FAILURE);
        }
        processControl.numRunning--;
        processControl.numCompleted++;
        updateStatus(pid, status);
    }

    // Print final summary
    printSummaryFull();

    return 0;

	
	//printSummaryFull();
}
