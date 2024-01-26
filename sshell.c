#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512
#define ARGS_MAX 16
#define PIPES_MAX 3

struct Command {
    char *program;
    char *args[ARGS_MAX+1];
};

struct Pipeline {
    struct Command commands[PIPES_MAX + 1];
};

// Define error codes
enum ErrorCode {
    MISSING_COMMAND = 1,
    NO_OUTPUT_FILE = 2,
    MISLOCATED_REDIRECTION = 3,
    CANNOT_OPEN_FILE = 4
};

int parse_command(char *cmd, struct Command *command) {
    char *token;
    int arg_count = 0;
    char *cmdline = strdup(cmd);

    // Tokenize the command line
    token = strtok(cmdline, " ");
    command->program = token;
    command->args[0] = token;

    // Parse arguments
    while (token != NULL) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            arg_count++;

            if (arg_count >= ARGS_MAX) {
                fprintf(stderr, "Error: too many arguments\n");
                free(cmdline);
                cmd = NULL;
                return 1;
            }

            command->args[arg_count] = token;
        }
    }
    // Null-terminate the arguments array
    arg_count++;
    command->args[arg_count] = NULL;
    return 0;
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
        fprintf(stderr, "Error: command not found\n");
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
        fprintf(stderr, "Error: cannot cd into directory\n");
        fprintf(stderr, "+ completed '%s' [1]\n", cmd);
    }
    else{
    fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
    }
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

void handle_exit(char *cmd) {
    fprintf(stderr, "Bye...\n");
    fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
    exit(EXIT_SUCCESS);
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

void execute_pipeline(struct Command commands[], char *cmd, int output_fd) {
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
            } else  {
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
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
    char *trunc_start;
    char *append_start;
    int output_fd;
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

    append_start = strstr(token[num_commands-1],">>"); //if last command have >>
    if (append_start != NULL){
        size_t index = append_start - token[num_commands-1]; //find the index >>
        append_start++;
        // Skip white spaces
        while (*append_start == ' ') {
            append_start++;
        }
        output_fd = open(append_start, O_WRONLY | O_CREAT | O_APPEND, 0666);
        token[num_commands-1][index] = '\0'; //cut what's after >> inclusive
    }

    trunc_start = strchr(token[num_commands-1],'>'); //if last command have >
    if (trunc_start != NULL){
        size_t index = trunc_start - token[num_commands-1]; //find the index >
        trunc_start++;
        // Skip white spaces
        while (*trunc_start == ' ') {
            trunc_start++;
        }
        output_fd = open(trunc_start, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        token[num_commands-1][index] = '\0'; //cut what's after > inclusive
    }

    for(int i = 0; i<num_commands; i++) {
        parse_command(token[i], &commands[i]);

    }
    execute_pipeline(commands, cmdline, output_fd);
}

void sls() {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    // Open current directory
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

int check_error(char *cmd) {
    size_t len = strlen(cmd);
    while (len > 0 && isspace((unsigned char)cmd[len - 1])) {
        cmd[--len] = '\0';
    }
    while (*cmd == ' ') {
        cmd++;
    }

    /* Error: missing command */
    // Check if the command starts with '|' or '>'
    if (cmd[0] == '>' || cmd[0] == '|') {
        return MISSING_COMMAND;
    } 
    // Check if the command ends with '|'
    if (len > 0 && cmd[len - 1] == '|' ) {
        return MISSING_COMMAND;
    }

    /* Error: no output file */
    // Check if the command ends with '>'
    if (len > 0 && cmd[len - 1] == '>' ) {
        return NO_OUTPUT_FILE;
    }

    /* Error: mislocated output redirection */
    char *redirect_symbol = strstr(cmd, ">");
    char *pipe_symbol = strstr(cmd, "|");
    char *last_pipe = NULL;

    // Find the last pipe symbol before the redirect symbol
    while (pipe_symbol != NULL) {
        last_pipe = pipe_symbol;
        pipe_symbol = strstr(pipe_symbol + 1, "|");
    }
    if (redirect_symbol != NULL && last_pipe != NULL && redirect_symbol < last_pipe) {
        return MISLOCATED_REDIRECTION;
    }

    /* Error: cannot open output file */
    char *output_file = strchr(cmd, '>');
    if (strstr(cmd, ">>")) {
        output_file++;
        if (output_file != NULL) {
             output_file++;

            // Move past any leading spaces
            while (*output_file == ' ') {
                output_file++;
            }
            FILE *output_fp = fopen(output_file, "a");
            if (output_fp == NULL) {
                return CANNOT_OPEN_FILE;
            }
            fclose(output_fp);
        }
    } else if (output_file != NULL) {
        output_file++;
        while (*output_file == ' ') {
            output_file++;
        }
        FILE *output_fp = fopen(output_file, "w");
        if (output_fp == NULL) {
            return CANNOT_OPEN_FILE;
        }
        fclose(output_fp);
    }
    return 0;
}

int main(void) {
    char cmd[CMDLINE_MAX];

    while (1) {
        char *nl;
        struct Command command;

        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

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

        /* Checking for error */
        int error = check_error(cmd);
        if (error == MISSING_COMMAND) {
            fprintf(stderr, "Error: missing command\n");
            continue;
        } else if (error == NO_OUTPUT_FILE) {
            fprintf(stderr, "Error: no output file\n");
            continue;
        } else if (error == MISLOCATED_REDIRECTION) {
            fprintf(stderr, "Error: mislocated output redirection\n");
            continue;
        } else if (error == CANNOT_OPEN_FILE) {
            fprintf(stderr, "Error: cannot open output file\n");
            continue;
        }

        /* Builtin commands */
        if (!strcmp(cmd, "exit")) { //exit
            handle_exit(cmd);
        }

        else if (!strncmp(cmd, "cd ", 3)) { //cd
            char *path = cmd + 3; // Extract path after "cd "
            handle_cd(path, cmd);
        }

        else if (!strcmp(cmd, "pwd")) { //pwd
            handle_pwd(cmd);
        }

        else if (strchr(cmd, '|')) { // pipe
            parse_pipe(cmd);
        }

        else if (strchr(cmd, '>')) {// output redirect
            output_redirection(cmd);
        }

        else if (strcmp(cmd, "sls") == 0) { // sls
            sls();
        }

        else {
            /* Parse the command line */
            int parsing_status = parse_command(cmd, &command);
            /* Execute the command */
            if (!parsing_status){
            execute_command(command, STDIN_FILENO, STDOUT_FILENO, cmd);
            }
        }
    }
    return EXIT_SUCCESS;
}