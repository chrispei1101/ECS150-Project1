# SSHELL : Simple Shell
## Summary
This program, 'sshell' provides a basic yet functional command-line interface with features commonly found in UNIX shells. It supports basic shell functionalities, including executing external commands, output redirection, piping, and built-in commands like cd, pwd, sls and exit.

## Implementation
The implementation of this program having these features:
1. Command Execution: Executes external commands entered by the user
2. Built-in Commands: Supports built-in commands such as cd, pwd, exit, and sls
3. Output Redirection: Redirects command output to a file using > or >>
4. Piping: Allows chaining multiple commands using |
5. Error Handling: Provides error messages for common issues like missing commands, mislocated output redirection, and unable to open 

### 1. Command Execution
The command execution is implemented using the fork function. When a command is entered, the shell creates a child process using fork(). The child process then executed with command struct using execvp. Meanwhile, the parent process waits for the child to complete using waitpid. Once the child finish, the parent will print exit code of the child.

### 2. Built-in Commands
Built-in commands, such as cd, pwd, exit, and sls, are identified within the main loop. If the entered command matches one of the built-ins, the corresponding function is called.

### 3. Output Redirection
Output redirection is implemented by identifying the presence of > or >> in the command line. The output file is then extracted, and the open system call is used to open the file with the appropriate flags (O_WRONLY, O_CREAT, O_APPEND or O_TRUNC). The file descriptor is then duplicated to STDOUT_FILENO, and the command is executed.

### 4. Piping
Piping is implemented by creating multiple child processes, each connected to a pair of pipes. The input of the first child will come from stdin, and the output of one command becomes the input for the next. The output of the last command will be in stdout. The parent process manages these processes and waits for their completion. File descriptors are duplicated using dup2 to set up the communication between commands. 

### 5. Error Handling
Error handling involves checking for various conditions such as missing commands, mislocated output redirection, or inability to open output files. The check_error function is responsible for identifying and reporting errors. Relevant error messages are printed to stderr. Error code is defined using enum.

### Note
The shell supports a maximum command line length of 512 characters (CMDLINE_MAX).
Maximum arguments per command are set to 16 (ARGS_MAX).
Maximum number of pipes in a pipeline is set to 3 (PIPES_MAX).
The parse_command function is responsible for breaking down the command line into individual tokens, extracting the program name and its arguments. The function utilizes the strtok function to tokenize the input string based on space characters. There is also counters to check if there are too many arguments.
