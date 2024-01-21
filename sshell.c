#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define CMDLINE_MAX 512
#define ARGS_MAX 32

struct Command {
    char *program;
    char *args[ARGS_MAX];
};

/*
void output_redirect(char *cmdline) {
    int fd;
    char *meta_character = strchr(cmdline, '>');
    
    // Parse the output file name from the command line
    if (meta_character) {
        char *output_file = meta_character + 1;
        while (*output_file == ' ') {
            output_file++;
        }

        char *token = strtok(cmdline, ">");

        // Redirect stdout to the file
        fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

void output_redirect(char *cmdline) {
    int fd;
    char *meta_character = strchr(cmdline, '>');

    // Parse the output file name from the command line
    if (meta_character) {
        char *output_file = meta_character + 1;
        while (*output_file == ' ') {
            output_file++;
        }

        // Create a pipe to capture the command output
        int pipe_fd[2];
        pipe(pipe_fd);

        pid_t child_pid = fork();

        if (child_pid == 0) {  // Child process
            close(pipe_fd[0]);  
            dup2(pipe_fd[1], STDOUT_FILENO);  // Redirect standard output to the pipe
            close(pipe_fd[1]);  

            system(cmdline);
            exit(EXIT_SUCCESS);

        } else {  // Parent process
            close(pipe_fd[1]); 
            fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            // Read from the pipe and write to the file
            char buffer[4096];
            ssize_t bytesRead;

            while ((bytesRead = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
                write(fd, buffer, bytesRead);
            }

            close(fd);
            close(pipe_fd[0]);
        }
    }
}
*/
void parse_command(char *cmdline, struct Command *command) {
    char *token;
    int arg_count = 0;

    // Tokenize the command line
    token = strtok(cmdline, " ");
    command->program = token;

    // Parse arguments
    while (token != NULL && arg_count < ARGS_MAX - 1) {
        token = strtok(NULL, " ");
        arg_count++;
        command->args[arg_count] = token;
    }

    // Null-terminate the arguments array
    arg_count++;
    command->args[arg_count] = NULL;
}


int main(void) {
    char cmd[CMDLINE_MAX];

    while (1) {
        char *nl;
        int status;
        pid_t pid;
        struct Command command;

        /* Print prompt */
        fprintf(stderr, "sshell$ ");
        fflush(stderr);

        /* Get command line */
        fgets(cmd, CMDLINE_MAX, stdin);

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl)
            *nl = '\0';

        output_redirect(cmd);

        /* Builtin commands */
        if (!strcmp(cmd, "exit")) { //exit
            fprintf(stderr, "Bye...\n");
            break;
        }

        else if (!strncmp(cmd, "cd ", 3)) { //cd
            char *path = cmd + 3; // Extract path after "cd "
            if (chdir(path) == -1) {
                perror("chdir");
            }
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
        }

        else if (!strcmp(cmd, "pwd")) { //pwd
            char cwd[128];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                fprintf(stdout, "%s\n", cwd);
            } 
            else {
                perror("getcwd");
            }
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
        }

        else {
            /* Parse the command line */
            parse_command(cmd, &command);
            /* Fork a child process */
            pid = fork();

            if (pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) { /* Child process */
                /* Execute the command in the child process */
                execvp(command.program, command.args);

                /* If execvp fails, print an error and exit */
                perror("execvp");
                exit(EXIT_FAILURE);
            } else { /* Parent process */
                /* Wait for the child process to complete */
                waitpid(pid, &status, 0);

                /* Print completion message to stderr */
                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(status));
            }
        }
    }

    return EXIT_SUCCESS;
}
