# SSHELL : Simple Shell
## Summary
This program, `sshell`, aims to construct a basic shell that is capable of
executing diverse commands including built-in commands, output redirection,
piping, extra features like `sls` and error management. The program adopts a
modular approach, dividing the core functionality into separate functions such
as parsing, command execution, error checking, output redirection and handling
built-in commands like `cd`, `pwd` and `exit`.

## Implementation
The implementation of this program having these features:
1. **Command Execution**: Executes external commands entered by the user
2. **Built-in Commands**: Supports built-in commands such as cd, pwd, exit, and
   sls
3. **Output Redirection**: Redirects command output to a file using `>` or `>>`
4. **Piping**: Allows chaining multiple commands using `|`
5. **Error Handling**: Provides error messages for common issues like missing
   commands, mislocated output redirection, and unable to open

### 1. Command Execution
The command execution, `execute_command` function, is implemented using the fork
function. When a command is entered, the shell creates a new process using the
`fork()`, effectively creating a parent process and a child process. If the fork
operation fails, the function prints an error message using `perror()` and exits
the program with a failure status. In the child process `(pid == 0)`, it checks
if the input and output file descriptors are different from the standard input
and output file descriptors. If so, it redirects the standard input or output
accordingly using `dup2()` and closes the corresponding file descriptors. Then,
it attempts to execute the command specified in the `command` structure using
the `execvp()`. Meanwhile, the parent process waits for the child to complete
using `waitpid()`. Once the child finish, the parent will print the exit code of
the child.

### 2. Built-in Commands
Built-in commands, such as cd, pwd, exit, and sls, are identified within the
main loop. If the entered command matches one of the built-ins, the
corresponding function is called.

The `handle_exit` function terminates the shell program. It prints a farewell
message and completion message to stderr to indicate the successful execution of
the exit command and then exits the program with a successful termination status
using `exit(EXIT_SUCCESS)`.

The `handle_pwd` function utilizes the `getcwd()` to obtain the current
directory and stores it in the `cwd` buffer. If successful, it prints the
current directory to `stdout`. If not, it prints an error message using
`perror()`. It also prints a completion message indicating the execution status.

The `handle_cd` function changes the current directory to the one specified path
using the `chdir()`. If the operation fails, it prints an error message to
`stderr`. It also prints a completion message indicating the execution status.

The `sls` function is designed to list the contents of the current directory. It
opens the current directory using `opendir(".")`. If the directory fails to
open, it prints an error message using `perror()`. Otherwise, it proceeds to
read the directory entries using `readdir()` in a loop. For each directory
entry, it retrieves file information using `stat()` excluding the entry name
starting with a dot. If `stat()` fails to retrieve file information, it prints
an error message and closes the directory stream. For each valid entry, the
function prints the entry's name and size. It also prints a completion message
to `stderr` indicating the successful execution and closes the directory stream
using `closedir()`.

### 3. Output Redirection
Output redirection, `output_redirection` function, is implemented by identifying
the presence of `>` or `>>` in the command line. The command line is being
duplicated using `strdup()` and then tokenized using `strtok()` to separate the
command and the output file name based on the `>` delimiter. The function also
detects if the `>>` sequence is present in the command line, indicating append
redirection. After extracting the output file name, the function trims leading
whitespaces from the file name. If the output file and command are valid, the
function attempts to open the output file using the `open()`. Depending on
whether it's append redirection or regular output redirection, appropriate flags
are passed to `open()` to handle file creation. If opening the output file
fails, an error message is printed using `perror()`. If successful, the function
proceeds to parse the command line using the `parse_command()` function and
executes the command using the `execute_command()` function.

### 4. Piping
The `parse_pipe` function is designed to handle command pipelines where commands
are connected by pipe symbols `|`. It begins by counting the number of commands
in the pipeline by iterating through the command line. It then allocates memory
for storing command structures based on the number of commands detected. It then
tokenizes the command line by pipe symbols using `strtok()` and stores each
command in an array. If the last command in the pipeline contains output
redirection symbols `>>` for append and `>` for truncation, it opens the
specified output file accordingly using the `open()`. Each command in the
pipeline get parased and executed using the `parse_command()` function. It then
executes the entire pipeline using the `execute_pipeline()` function, passing in
the array of command structures and the output file descriptor.

The `execute_pipeline` function manages the execution multiple commands
connected by pipe symbols `|`. It begins by counting the number of commands in
the pipeline by iterating through a duplicated copy of the command string using
`strdup()`. It then sets up pipes using the `pipe()`. The function then creates
child processes using `fork()` to execute each command in the pipeline. For each
child process, it redirects the standard input and output using `dup2()` to
connect the input of each command to the output of the previous command via
pipes. It also ensures that the output of the last command is redirected to the
specified output file descriptor if provided. Within each child process, all
pipe file descriptors are closed to prevent unnecessary file descriptor leaks.
The command is executed using `execvp()` to replace the child process with the
desired command. In the parent process, all pipe file descriptors are closed to
prevent potential deadlocks, and it waits for all child processes to complete
using `waitpid()`. It also prints a completion message to `stderr`, indicating
the status of each command along with its exit code.

### 5. Error Handling
Error handling, `check_error`, involves checking errors for various conditions
such as missing commands, mislocated output redirection, or inability to open
output files. Firstly, it checks if the command starts or ends with a pipe
symbol `|` or output redirection symbol `>`. If either condition is met, it
returns an appropriate error code, indicating a missing command or output file.
Next, if a command contains both pipe and output redirection symbols, the
function ensures that the output redirection symbol appears first before the
last pipe symbol. If it doesn't, it indicates a mislocated redirection error. It
also detects if it's unable to open the specified output file. If the command
includes an append redirection `>>`, it attempts to open the file in append mode
('a'). If the command includes a regular output redirection `>`, it attempts to
open the file in write mode ('w'). If the file cannot be opened for writing or
appending, it returns an error indicating the inability to open the file.
Relevant error messages are printed to stderr. Error code is defined using enum.

### Note
The shell supports a maximum command line length of 512 characters
**(CMDLINE_MAX)**. Maximum arguments per command are set to 16 **(ARGS_MAX)**.
Maximum number of pipes in a pipeline is set to 3 **(PIPES_MAX)**. The
`parse_command` function is responsible for breaking down the command line into
individual tokens, extracting the program name and its arguments. The function
utilizes the `strtok()` to tokenize the input string based on space characters.
There is also counters to check if there are too many arguments.


