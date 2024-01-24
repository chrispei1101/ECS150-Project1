#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#define CMDLINE_MAX 512
#define ARGS_MAX 32
#define PIPES_MAX 3

struct Command {
    char *program;
    char *args[ARGS_MAX];
};

struct Pipeline {
    struct Command commands[PIPES_MAX + 1];
};

void sls() {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    // Open the current directory
    dir = opendir(".");
    if (dir == NULL) {
        perror("Error: cannot open directory");
        return;
    }

    // Read and print directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Exclude entries starting with a dot
        if (entry->d_name[0] == '.') {
            continue;
        }

        if (stat(entry->d_name, &file_stat) == -1) {
            perror("Error: cannot get file information");
            closedir(dir);
            return;
        }

        // Print the entry name and size
        printf("%s (%ld bytes)\n", entry->d_name, (long)file_stat.st_size);
    }
    fprintf(stderr, "+ completed 'sls' [%d]\n", 0);

    // Close directory
    closedir(dir);
}

void parse_command(char *cmd, struct Command *command) {
    char *token;
    int arg_count = 0;
    char *cmdline = strdup(cmd);

    // Tokenize the command line
    token = strtok(cmdline, " ");
    command->program = token;
    command->args[0] = token;

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

void execute_command(struct Command command, int input_fd, int output_fd, char* cmd) {
    pid_t pid = fork();
    int status;

    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // Child process
        // Close read end of the pipe if input_fd is not STDIN_FILENO
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }

        // Close write end of the pipe if output_fd is not STDOUT_FILENO
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        execvp(command.program, command.args);

        // If execvp fails, print an error and exit
        perror("execvp");
        exit(EXIT_FAILURE);
    } else { // Parent process
        // Close both ends of the pipe in the parent process
        if (input_fd != STDIN_FILENO) {
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            close(output_fd);
        }

        // Wait for the child process to complete
        waitpid(pid, &status, 0);
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

void output_redirection(char *cmd) {
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
            return;  // Handle the error appropriately
        }

        struct Command command;
        parse_command(command_line, &command);
        execute_command(command, STDIN_FILENO, output_fd, cmd);
        close(output_fd);
    } else {
        fprintf(stderr, "Invalid syntax for output redirection\n");
    }
}

void execute_pipeline(struct Command commands[], char *cmd) {
    int num_commands = 1;
    char* cmd_copy = strdup(cmd);
    while (*cmd_copy != '\0') {
        if (*cmd_copy == '|') {
            num_commands++;
        }
        cmd_copy++;
    }
    int status[num_commands];
    int pipes[num_commands-1][2]; // One less pipe than commands
    pid_t pids[num_commands];
    
    // Create pipes
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Execute the command
    for (int i = 0; i < num_commands; i++) {
        // Create child processes
        if ((pids[i] = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pids[i] == 0) { // Child process
            // Connect input to the previous pipe (if not the first command)
           if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
                close(pipes[i-1][0]);
           }

            // Connect output to the current pipe (if not the last command)
            if (i != num_commands - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
                close(pipes[i][1]);
            }

            // Close all pipes in the child
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Execute the command
            execvp(commands[i].program, commands[i].args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }

    // Close all pipes in the parent
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all child processes to complete
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], &status[i], 0);
    }

    fprintf(stderr, "+ completed '%s' ", cmd);
    for (int i = 0; i < num_commands; i++) {
        int exit_code = WEXITSTATUS(status[i]);
        fprintf(stderr, "[%d]", exit_code);
    }
    fprintf(stderr, "\n");
}

void parse_pipe(char *cmdline) {
    int num_commands = 1;
    char* cmd_copy = strdup(cmdline);
    while (*cmd_copy != '\0') {
        if (*cmd_copy == '|') {
            num_commands++;
        }
        cmd_copy++;
    }
    struct Command *commands = (struct Command*)malloc(num_commands * sizeof(struct Command));
    cmd_copy = strdup(cmdline);
    char *token[num_commands];
    token[0]= strtok(cmd_copy, "|");
    for(int i = 1; i<num_commands; i++) {
        token[i] = strtok(NULL, "|");
    }
    
    for(int i = 0; i<num_commands; i++) {
        parse_command(token[i], &commands[i]);

    }
    execute_pipeline(commands, cmdline);
}

int main(void) {
    char cmd[CMDLINE_MAX];

    while (1) {
        char *nl;
        struct Command command;
        //struct Pipeline pipe;

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
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
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
            output_redirection(cmd);
        }

        else if (strchr(cmd, '|')) {
            // Pipeline is detected
            //execute_pipeline(cmd);
            parse_pipe(cmd);
            
        }

        else if (strcmp(cmd, "sls") == 0)
        {
            sls();
        }

        else {
            /* Parse the command line */
            parse_command(cmd, &command);
            /* Execute the command */
            execute_command(command, STDIN_FILENO, STDOUT_FILENO, cmd);
        }
    }
    return EXIT_SUCCESS;
}
