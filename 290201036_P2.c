#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE 100
#define MAX_ARGS 10
#define HISTORY_COUNT 10

// Array to store command history with fixed size HISTORY_COUNT
char *history[HISTORY_COUNT];

// Index to track the current position in the history array
int history_index = 0;


void add_to_history(const char *cmd) {
    /*
        - Adds a command to the history buffer.
        - If the current slot is occupied, free it to avoid memory leaks.
        - Duplicates the input command string and stores it in the history array.
        - Uses circular buffer logic to overwrite oldest entries when full.
    */

    if (history[history_index]) {
        free(history[history_index]);
    } 
    history[history_index] = strdup(cmd);
    history_index = (history_index + 1) % HISTORY_COUNT;
}


void show_history() {
    /*
        - Displays the stored command history.
        - Iterates from the current history_index and prints commands in the order they were entered.
        - Limits output to HISTORY_COUNT entries.
        - Each entry is numbered starting from 1.
    */

    int count = 0;
    int idx = history_index;
    do {
        if (history[idx]) {
            printf("[%d] %s\n", count + 1, history[idx]);
            count++;
        }
        idx = (idx + 1) % HISTORY_COUNT;
    } while (idx != history_index && count < HISTORY_COUNT);
}

void run_builtin_cd(char **args) {
    /*
        - Built-in command handler for 'cd'.
        - Changes the current working directory to the directory specified in args[1].
        - If no argument is provided, changes to the HOME directory.
        - On success, updates the PWD environment variable to the new directory.
        - On failure, prints an error message using perror.
    */
    char *target = args[1];
    if (!target) {
        target = getenv("HOME");  // Default to HOME if no argument
    }

    if (chdir(target) != 0) {
        perror("cd error");  // Print error if chdir fails
    } else {
        // Update PWD environment variable to reflect current directory
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            setenv("PWD", cwd, 1);
        }
    }
}

void run_builtin_pwd() {
    /*
        - Built-in command handler for 'pwd'.
        - Retrieves and prints the current working directory.
        - Uses getcwd system call; on failure, prints an error message.
    */
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd error");
    }
}

void execute_command(char **args, int background) {
    /*
        - Executes a command given by args.
        - If background is 0, waits for the command to finish.
        - If background is 1, runs the command in the background and prints the PID.
        - Uses fork to create a child process and execvp to execute the command.
        - Prints error message if fork or execvp fails.
    */
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");  // Fork failure
    } else if (pid == 0) {
        // Child process: execute the command
        execvp(args[0], args);
        // If execvp returns, an error occurred
        perror("exec error");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        if (!background) {
            // Wait for child to finish if not background
            waitpid(pid, NULL, 0);
        } else {
            // For background process, print process ID and do not wait
            printf("[BG] Process ID: %d\n", pid);
        }
    }
}

int parse_input(char *line, char **args) {
    /*
        - Parses the input line into arguments separated by spaces or tabs.
        - Stores pointers to tokens in the args array.
        - Ensures args is NULL-terminated.
        - Returns the number of arguments parsed.
    */
    int i = 0;
    char *token = strtok(line, " \t\n");

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }

    args[i] = NULL;
    return i;
}

void handle_pipe(char *left, char *right) {
    /*
        - Handles execution of two commands connected by a pipe '|'.
        - Creates a pipe and forks two child processes.
        - The first child redirects its stdout to the pipe write end and executes the left command.
        - The second child redirects its stdin to the pipe read end and executes the right command.
        - Parent closes pipe descriptors and waits for both children to finish.
        - Prints error messages if exec fails.
    */
    int pipefd[2];
    pipe(pipefd);  // Create a pipe: pipefd[0] for reading, pipefd[1] for writing

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // First child: executes left command, output goes to pipe write end
        dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to pipe write end
        close(pipefd[0]);                 // Close unused read end
        close(pipefd[1]);                 // Close original write end after dup2

        char *args[MAX_ARGS];
        parse_input(left, args);
        execvp(args[0], args);
        perror("exec left");  // Exec failed
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Second child: executes right command, input comes from pipe read end
        dup2(pipefd[0], STDIN_FILENO);   // Redirect stdin to pipe read end
        close(pipefd[1]);                 // Close unused write end
        close(pipefd[0]);                 // Close original read end after dup2

        char *args[MAX_ARGS];
        parse_input(right, args);
        execvp(args[0], args);
        perror("exec right");  // Exec failed
        exit(1);
    }

    // Parent closes both ends of the pipe as it's not used here
    close(pipefd[0]);
    close(pipefd[1]);
    // Wait for both child processes to finish
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

void handle_and(char *left, char *right) {
    /*
        - Handles execution of two commands connected by logical AND '&&'.
        - Executes the left command first.
        - If the left command exits successfully (exit status 0), executes the right command.
        - Uses fork and execvp for execution and waitpid to check exit status.
    */
    char *args[MAX_ARGS];
    parse_input(left, args);

    pid_t pid = fork();
    if (pid == 0) {
        // Child executes left command
        execvp(args[0], args);
        perror("exec");  // Exec failed
        exit(1);
    }

    int status;
    waitpid(pid, &status, 0);
    // Check if left command exited normally with status 0
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        // If successful, parse and execute right command
        parse_input(right, args);
        execute_command(args, 0);
    }
}


int main() {
    char line[MAX_LINE];

    // Main shell loop
    while (1) {
        printf("shell322> ");
        if (!fgets(line, sizeof(line), stdin)) break;  // Exit on EOF or error
        if (line[0] == '\n') continue;                  // Ignore empty lines

        add_to_history(line);  // Store command in history

        // Check for logical AND operator '&&' in the input line
        char *and_ptr = strstr(line, "&&");
        if (and_ptr) {
            *and_ptr = '\0';            // Split line into two commands
            handle_and(line, and_ptr + 2);  // Handle commands connected by &&
            continue;
        }

        // Check for pipe '|' in the input line
        char *pipe_ptr = strchr(line, '|');
        if (pipe_ptr) {
            *pipe_ptr = '\0';            // Split line into two commands
            handle_pipe(line, pipe_ptr + 1);  // Handle commands connected by pipe
            continue;
        }

        // Check for background execution symbol '&'
        int background = 0;
        if (strchr(line, '&')) {
            background = 1;                  // Mark command for background execution
            line[strcspn(line, "&")] = '\0'; // Remove '&' from the command string
        }

        char *args[MAX_ARGS];
        parse_input(line, args);
        if (args[0] == NULL) continue;  // Ignore empty commands

        // Handle built-in commands
        if (strcmp(args[0], "cd") == 0) {
            run_builtin_cd(args);  // Change directory
        } else if (strcmp(args[0], "pwd") == 0) {
            run_builtin_pwd();     // Print working directory
        } else if (strcmp(args[0], "exit") == 0) {
            break;                // Exit the shell loop
        } else if (strcmp(args[0], "history") == 0) {
            show_history();       // Display command history
        } else {
            // Execute external command with optional background execution
            execute_command(args, background);
        }
    }

    // Free dynamically allocated memory in history before exiting
    for (int i = 0; i < HISTORY_COUNT; i++) {
        if (history[i]) free(history[i]);
    }

    return 0;
}