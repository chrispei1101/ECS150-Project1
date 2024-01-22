#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>


#define CMDLINE_MAX 512
#define ARGS_MAX 32

struct Command {
    char *program;
    char *args[ARGS_MAX];
};

void parse_command(char *cmd, struct Command *command) {
    char *token;
    int arg_count = 0;
    char *cmdline = strdup(cmd);

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

void execute_command(struct Command *command, int output_fd, char* cmd) {
    pid_t pid = fork();
    int status;

    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // Child process
        // Redirect standard output to the specified file
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
            execvp(command->program, command->args);
        }

        else{
            execvp(command->program, command->args);
        }
        // If execvp fails, print an error and exit
        perror("execvp");
        exit(EXIT_FAILURE);
    } 
    
    else { // Parent process
        // Wait for the child process to complete
        waitpid(pid, NULL, 0);
        /* Print completion message to stderr */
        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, WEXITSTATUS(status));
    }
}


void handle_cd(char *path, char *cmd) {
    if (chdir(path) == -1) {
        perror("chdir");
    }
    fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
}

void handle_pwd(char *cmd) {
    char cwd[128];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        fprintf(stdout, "%s\n", cwd);
    } else {
        perror("getcwd");
    }
    fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
}



int main(void) {
    char cmd[CMDLINE_MAX];

    while (1) {
        char *nl;
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

        /* Builtin commands */
        if (!strcmp(cmd, "exit")) { //exit
            fprintf(stderr, "Bye...\n");
            break;
        }

        else if (!strncmp(cmd, "cd ", 3)) { //cd
            char *path = cmd + 3; // Extract path after "cd "
            handle_cd(path, cmd);
        }

        else if (!strcmp(cmd, "pwd")) { //pwd
            handle_pwd(cmd);
        }
        
        else if (strchr(cmd, '>')) {
            // Output or append redirection is detected
            char *command_line = strdup(cmd);
            command_line = strtok(command_line, ">");
            char *output_file = strtok(NULL, ">");
            int is_append = 0;  // Flag to check if it's append redirection

            // Check if ">>" is present for append redirection
            if (strstr(cmd, ">>")) {
                is_append = 1;
            }

            while (*output_file == ' ') {
                output_file++;
            }

            if (output_file != NULL && command_line != NULL) {
                int output_fd;
                if (is_append) {
                    // Append redirection
                    output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                } else {
                    // Output redirection
                    output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                }

                if (output_fd == -1) {
                    perror("open");
                    continue;
                }

                parse_command(command_line, &command);
                execute_command(&command, output_fd, cmd);
                close(output_fd);
            } 
            else {
                fprintf(stderr, "Invalid syntax for output redirection\n");
            }
        }

        else {
            /* Parse the command line */
            parse_command(cmd, &command);
            /* Execute the command */
            execute_command(&command, STDOUT_FILENO, cmd);

        }
    }

    return EXIT_SUCCESS;
}