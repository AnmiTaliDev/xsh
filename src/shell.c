#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <termios.h>
#include <stdarg.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "shell.h"

// Signal handler
void handle_signal(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            printf("\n");
            rl_on_new_line();
            rl_replace_line("", 0);
            rl_redisplay();
            break;
    }
}

// Print error message
void print_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%sError:%s ", COLOR_RED, COLOR_RESET);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// Print success message
void print_success(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stdout, "%sSuccess:%s ", COLOR_GREEN, COLOR_RESET);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    va_end(args);
}

// Parse command into arguments
char **parse_command(char *command, int *argc) {
    if (!command || !argc) return NULL;
    
    char **args = malloc(MAX_ARGS * sizeof(char *));
    if (!args) {
        print_error("malloc: failed to allocate memory");
        return NULL;
    }
    
    *argc = 0;
    char *cmd = strdup(command);
    if (!cmd) {
        free(args);
        return NULL;
    }

    char *token = strtok(cmd, " \t\n\r");
    while (token && *argc < MAX_ARGS - 1) {
        args[*argc] = strdup(token);
        if (!args[*argc]) {
            for (int i = 0; i < *argc; i++) {
                free(args[i]);
            }
            free(args);
            free(cmd);
            return NULL;
        }
        (*argc)++;
        token = strtok(NULL, " \t\n\r");
    }
    args[*argc] = NULL;
    
    free(cmd);
    return args;
}

// Get shortened path (replace home directory with ~)
char *get_short_path(const char *path) {
    static char short_path[MAX_PATH_LENGTH];
    
    if (!path) {
        strncpy(short_path, ".", sizeof(short_path) - 1);
        short_path[sizeof(short_path) - 1] = '\0';
        return short_path;
    }

    const char *home = getenv("HOME");
    if (home && strncmp(path, home, strlen(home)) == 0) {
        snprintf(short_path, sizeof(short_path), "~%s", path + strlen(home));
    } else {
        strncpy(short_path, path, sizeof(short_path) - 1);
        short_path[sizeof(short_path) - 1] = '\0';
    }

    return short_path;
}

// Add command to history
void add_to_history(const char *command) {
    if (command && *command) {
        add_history(command);
    }
}

// Add alias
void add_alias(const char *name, const char *value) {
    if (!name || !value) return;

    if (config.alias_count >= MAX_ALIASES) {
        print_error("Maximum number of aliases reached");
        return;
    }

    // Check if alias already exists
    for (int i = 0; i < config.alias_count; i++) {
        if (strcmp(config.aliases[i].name, name) == 0) {
            free(config.aliases[i].value);
            config.aliases[i].value = strdup(value);
            return;
        }
    }

    // Add new alias
    config.aliases[config.alias_count].name = strdup(name);
    config.aliases[config.alias_count].value = strdup(value);
    config.alias_count++;
}

// Get alias value
char *get_alias(const char *name) {
    if (!name) return NULL;

    for (int i = 0; i < config.alias_count; i++) {
        if (strcmp(config.aliases[i].name, name) == 0) {
            return config.aliases[i].value;
        }
    }
    return NULL;
}

// Remove alias
void remove_alias(const char *name) {
    if (!name) return;

    for (int i = 0; i < config.alias_count; i++) {
        if (strcmp(config.aliases[i].name, name) == 0) {
            free(config.aliases[i].name);
            free(config.aliases[i].value);
            
            // Shift remaining aliases
            for (int j = i; j < config.alias_count - 1; j++) {
                config.aliases[j] = config.aliases[j + 1];
            }
            
            config.alias_count--;
            return;
        }
    }
}

// Update background jobs
void update_jobs(void) {
    for (int i = 0; i < config.job_count; i++) {
        if (!config.jobs[i].running) continue;

        int status;
        pid_t result = waitpid(config.jobs[i].pid, &status, WNOHANG);
        
        if (result == 0) {
            // Process still running
            continue;
        } else if (result == config.jobs[i].pid) {
            // Process finished
            config.jobs[i].running = 0;
            config.jobs[i].status = WEXITSTATUS(status);
            
            // Print job completion status
            printf("[%d] %s %s (%s)\n",
                i + 1,
                config.jobs[i].command,
                "Done",
                config.jobs[i].status == 0 ? "success" : "failed");
        }
    }
}

// Show background jobs
void show_jobs(void) {
    for (int i = 0; i < config.job_count; i++) {
        printf("[%d] %s%s%s  %s  %s\n",
               i + 1,
               config.jobs[i].running ? COLOR_GREEN : COLOR_RED,
               config.jobs[i].running ? "Running" : "Done",
               COLOR_RESET,
               config.jobs[i].running ? "" : (config.jobs[i].status == 0 ? "(success)" : "(failed)"),
               config.jobs[i].command);
    }
}

// Add background job
void add_job(pid_t pid, char *command) {
    if (!command) return;

    if (config.job_count >= MAX_ARGS) {
        print_error("Maximum number of background jobs reached");
        return;
    }

    config.jobs[config.job_count].pid = pid;
    config.jobs[config.job_count].command = strdup(command);
    config.jobs[config.job_count].running = 1;
    config.jobs[config.job_count].status = 0;
    config.job_count++;
}

// Get background job by ID
Job *get_job(int job_id) {
    if (job_id <= 0 || job_id > config.job_count) {
        return NULL;
    }
    return &config.jobs[job_id - 1];
}

// Command completion generator
char *command_generator(const char *text, int state) {
    static int list_index, len;
    static char **commands = NULL;
    static char **builtin_commands = NULL;
    static const char *builtin_list[] = {
        "cd", "pwd", "exit", "clear", "help", "history", "alias", "jobs",
        NULL
    };

    // Initialize on first call
    if (!state) {
        if (commands) {
            for (int i = 0; commands[i]; i++) {
                free(commands[i]);
            }
            free(commands);
        }
        if (builtin_commands) {
            free(builtin_commands);
        }

        list_index = 0;
        len = strlen(text);

        // Allocate space for commands
        commands = malloc(MAX_ARGS * sizeof(char *));
        builtin_commands = malloc(MAX_ARGS * sizeof(char *));
        if (!commands || !builtin_commands) {
            print_error("malloc: failed to allocate memory");
            return NULL;
        }

        // Add builtin commands
        int builtin_count = 0;
        for (int i = 0; builtin_list[i]; i++) {
            if (strncmp(builtin_list[i], text, len) == 0) {
                builtin_commands[builtin_count++] = strdup(builtin_list[i]);
            }
        }
        builtin_commands[builtin_count] = NULL;
        
        // Get all commands from PATH
        char *path = strdup(getenv("PATH"));
        char *dir = strtok(path, ":");
        int cmd_count = 0;
        
        while (dir && cmd_count < MAX_ARGS - 1) {
            DIR *d = opendir(dir);
            if (d) {
                struct dirent *entry;
                while ((entry = readdir(d)) && cmd_count < MAX_ARGS - 1) {
                    if (entry->d_type == DT_REG && 
                        strncmp(entry->d_name, text, len) == 0) {
                        commands[cmd_count++] = strdup(entry->d_name);
                    }
                }
                closedir(d);
            }
            dir = strtok(NULL, ":");
        }
        commands[cmd_count] = NULL;
        free(path);
    }

    // Return next match (first builtin, then external)
    char *name = NULL;
    if (builtin_commands[list_index]) {
        name = builtin_commands[list_index];
    } else {
        int external_index = list_index - (sizeof(builtin_list)/sizeof(char*) - 1);
        if (commands[external_index]) {
            name = commands[external_index];
        }
    }

    if (name) {
        list_index++;
        return strdup(name);
    }

    // No more matches
    list_index = 0;
    return NULL;
}

// Setup readline completion
char **xsh_completion(const char *text, int start, int end) {
    (void)start;
    (void)end;

    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, command_generator);
}

// Trim whitespace from string
char *trim_whitespace(char *str) {
    if (!str) return NULL;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    // Trim trailing space
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

// Get file type string
const char *get_file_type(mode_t mode) {
    switch (mode & S_IFMT) {
        case S_IFREG:  return "-";
        case S_IFDIR:  return "d";
        case S_IFLNK:  return "l";
        case S_IFCHR:  return "c";
        case S_IFBLK:  return "b";
        case S_IFSOCK: return "s";
        case S_IFIFO:  return "p";
        default:       return "?";
    }
}

// Get file permissions string
void get_permissions(mode_t mode, char *perms) {
    const char *rwx = "rwx";
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            perms[i * 3 + j] = (mode & (1 << (8 - i * 3 - j))) ? rwx[j] : '-';
        }
    }
    perms[9] = '\0';
}

char *format_size(off_t size) {
    static char buf[32];
    const char *units[] = {"B", "K", "M", "G", "T"};
    int i = 0;
    double formatted = size;

    while (formatted >= 1024 && i < 4) {
        formatted /= 1024;
        i++;
    }

    if (i == 0) {
        snprintf(buf, sizeof(buf), "%" PRIdMAX "%s", (intmax_t)size, units[i]);
    } else {
        snprintf(buf, sizeof(buf), "%.1f%s", formatted, units[i]);
    }

    return buf;
}

// Get formatted time string
char *format_time(time_t t) {
    static char buf[32];
    struct tm *tm = localtime(&t);
    strftime(buf, sizeof(buf), "%b %d %H:%M", tm);
    return buf;
}