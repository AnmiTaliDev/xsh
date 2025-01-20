#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "shell.h"

// Global variables
char current_dir[MAX_PATH_LENGTH];
char current_user[256];
volatile sig_atomic_t running = 1;
Config config;

// Standard paths for command lookup
const char *standard_paths[] = {
    "/bin",
    "/sbin",
    "/usr/bin",
    "/usr/sbin",
    "/usr/local/bin",
    "~/.local/bin",
    NULL
};

// Initialize shell
void initialize_shell(void) {
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strncpy(current_user, pw->pw_name, sizeof(current_user) - 1);
        current_user[sizeof(current_user) - 1] = '\0';
    } else {
        strcpy(current_user, "unknown");
    }

    if (!getcwd(current_dir, sizeof(current_dir))) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // Initialize readline with custom completion
    rl_initialize();
    rl_attempted_completion_function = xsh_completion;

    // Load history
    char history_path[MAX_PATH_LENGTH];
    snprintf(history_path, sizeof(history_path), "%s/%s", getenv("HOME"), HISTORY_FILE);
    read_history(history_path);

    // Setup signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGQUIT, handle_signal);

    // Initialize config
    config.color_prompt = 1;
    config.history_size = MAX_HISTORY;
    config.alias_count = 0;
    config.job_count = 0;

    setlocale(LC_ALL, "");
}

// Find command in PATH
char *find_command(const char *cmd) {
    if (!cmd) return NULL;
    
    // If cmd contains '/', treat as path
    if (strchr(cmd, '/')) {
        if (access(cmd, X_OK) == 0) {
            return strdup(cmd);
        }
        return NULL;
    }

    // Get PATH environment variable
    char *path_env = getenv("PATH");
    if (!path_env) {
        path_env = "/bin:/usr/bin";  // Default PATH
    }
    char *path = strdup(path_env);
    
    // Add standard paths to search
    size_t total_len = strlen(path);
    for (int i = 0; standard_paths[i]; i++) {
        total_len += strlen(standard_paths[i]) + 1;  // +1 for ':'
    }
    
    char *new_path = malloc(total_len + 1);
    if (!new_path) {
        free(path);
        return NULL;
    }
    
    strcpy(new_path, path);
    for (int i = 0; standard_paths[i]; i++) {
        strcat(new_path, ":");
        
        // Expand ~ to home directory if needed
        if (standard_paths[i][0] == '~') {
            const char *home = getenv("HOME");
            if (home) {
                strcat(new_path, home);
                strcat(new_path, standard_paths[i] + 1);
            } else {
                strcat(new_path, standard_paths[i]);
            }
        } else {
            strcat(new_path, standard_paths[i]);
        }
    }
    
    free(path);
    path = new_path;

    // Search for command in PATH
    char *dir = strtok(path, ":");
    char full_path[MAX_PATH_LENGTH];
    
    while (dir) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
        if (access(full_path, X_OK) == 0) {
            free(path);
            return strdup(full_path);
        }
        dir = strtok(NULL, ":");
    }
    
    free(path);
    return NULL;
}

// Built-in commands
int cmd_cd(char **args) {
    if (!args[1]) {
        const char *home = getenv("HOME");
        if (!home) {
            print_error("HOME environment variable not set");
            return EXIT_FAILURE;
        }
        if (chdir(home) != 0) {
            print_error("cd: %s: %s", home, strerror(errno));
            return EXIT_FAILURE;
        }
    } else {
        if (chdir(args[1]) != 0) {
            print_error("cd: %s: %s", args[1], strerror(errno));
            return EXIT_FAILURE;
        }
    }

    if (!getcwd(current_dir, sizeof(current_dir))) {
        print_error("getcwd: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int cmd_pwd(char **args) {
    (void)args;
    printf("%s\n", current_dir);
    return EXIT_SUCCESS;
}

int cmd_exit(char **args) {
    (void)args;
    running = 0;
    return EXIT_SUCCESS;
}

int cmd_clear(char **args) {
    (void)args;
    printf("\033[H\033[J");  // Clear screen using ANSI escape codes
    return EXIT_SUCCESS;
}

int cmd_help(char **args) {
    (void)args;
    printf("\nAvailable built-in commands:\n");
    printf("  cd [dir]     - Change directory\n");
    printf("  pwd          - Print working directory\n");
    printf("  clear        - Clear screen\n");
    printf("  history      - Show command history\n");
    printf("  alias        - Show/set aliases\n");
    printf("  help         - Show this help\n");
    printf("  exit         - Exit shell\n");
    printf("\nExternal commands are searched in:\n");
    for (int i = 0; standard_paths[i]; i++) {
        printf("  %s\n", standard_paths[i]);
    }
    printf("  And any directory in $PATH\n");
    return EXIT_SUCCESS;
}

int cmd_history(char **args) {
    (void)args;
    HIST_ENTRY **hist_list = history_list();
    if (!hist_list) return EXIT_SUCCESS;

    for (int i = 0; hist_list[i]; i++) {
        printf("%5d  %s\n", i + 1, hist_list[i]->line);
    }
    return EXIT_SUCCESS;
}

int cmd_alias(char **args) {
    if (!args[1]) {
        for (int i = 0; i < config.alias_count; i++) {
            printf("alias %s='%s'\n", config.aliases[i].name, config.aliases[i].value);
        }
        return EXIT_SUCCESS;
    }

    char *equals = strchr(args[1], '=');
    if (!equals) {
        print_error("alias: invalid format. Use: alias name=value");
        return EXIT_FAILURE;
    }

    *equals = '\0';
    add_alias(args[1], equals + 1);
    return EXIT_SUCCESS;
}

// Execute built-in command
int execute_builtin(char **args) {
    if (!args[0]) return EXIT_SUCCESS;

    if (strcmp(args[0], "cd") == 0) return cmd_cd(args);
    if (strcmp(args[0], "pwd") == 0) return cmd_pwd(args);
    if (strcmp(args[0], "exit") == 0) return cmd_exit(args);
    if (strcmp(args[0], "clear") == 0) return cmd_clear(args);
    if (strcmp(args[0], "help") == 0) return cmd_help(args);
    if (strcmp(args[0], "history") == 0) return cmd_history(args);
    if (strcmp(args[0], "alias") == 0) return cmd_alias(args);

    return EXIT_NOT_FOUND;
}

// Execute external command
int execute_external(char **args) {
    if (!args || !args[0]) return EXIT_FAILURE;

    // Find command in PATH
    char *cmd_path = find_command(args[0]);
    if (!cmd_path) {
        print_error("%s: command not found", args[0]);
        return EXIT_NOT_FOUND;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execv(cmd_path, args);
        print_error("%s: execution failed: %s", args[0], strerror(errno));
        free(cmd_path);
        exit(EXIT_NOT_FOUND);
    } else if (pid < 0) {
        // Fork failed
        print_error("fork: %s", strerror(errno));
        free(cmd_path);
        return EXIT_FAILURE;
    }

    // Parent process
    free(cmd_path);
    int status;
    do {
        waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
}

// Execute command
int execute_command(char *command) {
    if (!command || !*command) return EXIT_SUCCESS;

    // Add to history
    add_to_history(command);

    // Parse command
    int argc;
    char **args = parse_command(command, &argc);
    if (!args) return EXIT_FAILURE;

    // Check for alias
    if (args[0]) {
        char *alias_value = get_alias(args[0]);
        if (alias_value) {
            // Free old command
            for (int i = 0; args[i]; i++) {
                free(args[i]);
            }
            free(args);
            
            // Parse alias
            args = parse_command(alias_value, &argc);
            if (!args) return EXIT_FAILURE;
        }
    }

    // Execute command
    int status;
    if ((status = execute_builtin(args)) == EXIT_NOT_FOUND) {
        status = execute_external(args);
    }

    // Cleanup
    for (int i = 0; args[i]; i++) {
        free(args[i]);
    }
    free(args);

    return status;
}

// Generate prompt
char *generate_prompt(void) {
    static char prompt[MAX_PROMPT_LENGTH];
    char *short_path = get_short_path(current_dir);

    if (config.color_prompt) {
        snprintf(prompt, sizeof(prompt),
                "%s%s%s %s%s%s %s➜%s ",
                COLOR_CYAN, current_user, COLOR_RESET,
                COLOR_BLUE, short_path, COLOR_RESET,
                COLOR_GREEN, COLOR_RESET);
    } else {
        snprintf(prompt, sizeof(prompt),
                "%s %s ➜ ",
                current_user, short_path);
    }

    return prompt;
}

// Cleanup shell
void cleanup_shell(void) {
    char history_path[MAX_PATH_LENGTH];
    snprintf(history_path, sizeof(history_path), "%s/%s", getenv("HOME"), HISTORY_FILE);
    write_history(history_path);
    rl_clear_history();
    printf("\n%sGoodbye!%s\n", COLOR_GREEN, COLOR_RESET);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    initialize_shell();

    printf("\n%sWelcome to XShell!%s\n", COLOR_GREEN, COLOR_RESET);
    printf("Type 'help' to see available commands\n\n");

    char *command;
    while (running && (command = readline(generate_prompt()))) {
        if (command && *command) {
            char *trimmed = trim_whitespace(command);
            if (*trimmed) {
                execute_command(trimmed);
                update_jobs();
            }
        }
        free(command);
    }

    cleanup_shell();
    return EXIT_SUCCESS;
}