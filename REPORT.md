# SSHELL : Simple Shell
## Summary
This program, `sshell`, aims to construct a basic shell that is capable of executing diverse commands including built-in commands, output redirection, piping, extra features like `sls` and error management. The program adopts a modular approach, dividing the core functionality into separate functions such as parsing, command execution, error checking, output redirection and handling built-in commands like `cd`, `pwd` and `exit`.

## Implementation
The implementation of this program having these features:
1. **Command Execution**: Executes external commands entered by the user
2. **Built-in Commands**: Supports built-in commands such as cd, pwd, exit, and sls
3. **Output Redirection**: Redirects command output to a file using `>` or `>>`
4. **Piping**: Allows chaining multiple commands using `|`
5. **Error Handling**: Provides error messages for common issues like missing
   commands, mislocated output redirection, and unable to open

### 1. Command Execution
The command execution, `execute_command` function, is implemented using the fork function. When a command is
entered, the shell creates a new process using the `fork()`, effectively creating a parent process and a child process. If the fork operation fails, the function prints an error message using `perror()` and exits the program with a failure status. In the child process `(pid == 0)`, it checks if the input and output file descriptors are different from the standard input and output file descriptors. If so, it redirects the standard input or output accordingly using  `dup2()` and closes the corresponding file descriptors. Then, it attempts to execute the command specified in the `command` structure using the `execvp()`. Meanwhile, the parent process waits
for the child to complete using `waitpid()`. Once the child finish, the parent will
print the exit code of the child.

### 2. Built-in Commands
Built-in commands, such as cd, pwd, exit, and sls, are identified within the
main loop. If the entered command matches one of the built-ins, the
corresponding function is called.

The `handle_exit` function terminates the shell program. It prints a farewell message and completion message to stderr to indicate the successful execution of the exit command and then exits the program with a successful termination status using `exit(EXIT_SUCCESS)`.

The `handle_pwd` function utilizes the `getcwd()` to obtain the current directory and stores it in the `cwd` buffer. If successful, it prints the current directory to `stdout`. If not, it prints an error message using `perror()`. It also prints a completion message indicating the execution status.

The `handle_cd` function changes the current directory to the one specified path using the `chdir()`. If the operation fails, it prints an error message to `stderr`. It also prints a completion message indicating the execution status.

### 3. Output Redirection
Output redirection is implemented by identifying the presence of > or >> in the
command line. The output file is then extracted, and the open system call is
used to open the file with the appropriate flags (O_WRONLY, O_CREAT, O_APPEND or
O_TRUNC). The file descriptor is then duplicated to STDOUT_FILENO, and the
command is executed.

### 4. Piping
Piping is implemented by creating multiple child processes, each connected to a
pair of pipes. The input of the first child will come from stdin, and the output
of one command becomes the input for the next. The output of the last command
will be in stdout. The parent process manages these processes and waits for
their completion. File descriptors are duplicated using dup2 to set up the
communication between commands.

### 5. Error Handling
Error handling involves checking for various conditions such as missing
commands, mislocated output redirection, or inability to open output files. The
check_error function is responsible for identifying and reporting errors.
Relevant error messages are printed to stderr. Error code is defined using enum.

### Note
The shell supports a maximum command line length of 512 characters
(CMDLINE_MAX). Maximum arguments per command are set to 16 (ARGS_MAX). Maximum
number of pipes in a pipeline is set to 3 (PIPES_MAX). The parse_command
function is responsible for breaking down the command line into individual
tokens, extracting the program name and its arguments. The function utilizes the
strtok function to tokenize the input string based on space characters. There is
also counters to check if there are too many arguments.


