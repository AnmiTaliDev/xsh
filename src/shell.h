#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <termios.h>
#include <stdarg.h>
#include <ctype.h>
#include <locale.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <inttypes.h>

// Constants
#define MAX_COMMAND_LENGTH 4096
#define MAX_ARGS 512
#define MAX_PATH_LENGTH 4096
#define MAX_PROMPT_LENGTH 1024
#define MAX_ALIASES 100
#define MAX_HISTORY 1000
#define HISTORY_FILE ".xsh_history"

// ANSI color codes
#define COLOR_RESET   "\001\033[0m\002"
#define COLOR_BLACK   "\001\033[30m\002"
#define COLOR_RED     "\001\033[31m\002"
#define COLOR_GREEN   "\001\033[32m\002"
#define COLOR_YELLOW  "\001\033[33m\002"
#define COLOR_BLUE    "\001\033[34m\002"
#define COLOR_MAGENTA "\001\033[35m\002"
#define COLOR_CYAN    "\001\033[36m\002"
#define COLOR_WHITE   "\001\033[37m\002"

// Bold colors
#define COLOR_BOLD_BLACK   "\001\033[1;30m\002"
#define COLOR_BOLD_RED     "\001\033[1;31m\002"
#define COLOR_BOLD_GREEN   "\001\033[1;32m\002"
#define COLOR_BOLD_YELLOW  "\001\033[1;33m\002"
#define COLOR_BOLD_BLUE    "\001\033[1;34m\002"
#define COLOR_BOLD_MAGENTA "\001\033[1;35m\002"
#define COLOR_BOLD_CYAN    "\001\033[1;36m\002"
#define COLOR_BOLD_WHITE   "\001\033[1;37m\002"

// Background colors
#define COLOR_BG_BLACK   "\001\033[40m\002"
#define COLOR_BG_RED     "\001\033[41m\002"
#define COLOR_BG_GREEN   "\001\033[42m\002"
#define COLOR_BG_YELLOW  "\001\033[43m\002"
#define COLOR_BG_BLUE    "\001\033[44m\002"
#define COLOR_BG_MAGENTA "\001\033[45m\002"
#define COLOR_BG_CYAN    "\001\033[46m\002"
#define COLOR_BG_WHITE   "\001\033[47m\002"

// Exit codes
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define EXIT_NOT_FOUND 127

// Structures
typedef struct {
    char *name;
    char *value;
} Alias;

typedef struct {
    pid_t pid;
    char *command;
    int status;
    int running;
    time_t start_time;
    char *cwd;
} Job;

typedef struct {
    char history_file[MAX_PATH_LENGTH];
    int history_size;
    Alias aliases[MAX_ALIASES];
    int alias_count;
    Job jobs[MAX_ARGS];
    int job_count;
    int color_prompt;
    int verbose_mode;
    int debug_mode;
} Config;

// IO redirection structure
typedef struct {
    char *input_file;
    char *output_file;
    char *error_file;
    int append_output;
    int append_error;
} Redirection;

// Command structure
typedef struct {
    char **args;
    int argc;
    Redirection redirect;
    int background;
    char *raw_command;
} Command;

// Global variables
extern char current_dir[MAX_PATH_LENGTH];
extern char current_user[256];
extern volatile sig_atomic_t running;
extern Config config;

// Function prototypes

// Initialization and cleanup
void initialize_shell(void);
void cleanup_shell(void);

// Signal handling
void handle_signal(int sig);

// Command parsing and execution
char **parse_command(char *command, int *argc);
Command *parse_command_full(char *command);
int execute_command(char *command);
int execute_builtin(char **args);
int execute_external(char **args);
char *find_command(const char *cmd);
void free_command(Command *cmd);

// Built-in commands
int cmd_cd(char **args);
int cmd_pwd(char **args);
int cmd_exit(char **args);
int cmd_clear(char **args);
int cmd_help(char **args);
int cmd_history(char **args);
int cmd_alias(char **args);
int cmd_unalias(char **args);
int cmd_jobs(char **args);
int cmd_fg(char **args);
int cmd_bg(char **args);
int cmd_kill(char **args);
int cmd_set(char **args);
int cmd_unset(char **args);
int cmd_source(char **args);

// Path handling
char *get_short_path(const char *path);
char *expand_path(const char *path);
char *resolve_path(const char *path);
int is_directory(const char *path);
int is_executable(const char *path);

// History management
void add_to_history(const char *command);
void load_history(void);
void save_history(void);
char *get_history_file(void);
HIST_ENTRY **get_history(void);

// Alias management
void add_alias(const char *name, const char *value);
char *get_alias(const char *name);
void remove_alias(const char *name);
void load_aliases(void);
void save_aliases(void);

// Job control
void update_jobs(void);
void show_jobs(void);
void add_job(pid_t pid, char *command);
Job *get_job(int job_id);
void remove_job(int job_id);
void cleanup_jobs(void);

// IO redirection
int setup_redirections(Redirection *redirect);
void cleanup_redirections(Redirection *redirect);

// Completion
char *command_generator(const char *text, int state);
char **xsh_completion(const char *text, int start, int end);

// File operations
const char *get_file_type(mode_t mode);
void get_permissions(mode_t mode, char *perms);
char *format_size(off_t size);
char *format_time(time_t t);
char *get_file_owner(uid_t uid);
char *get_file_group(gid_t gid);

// Environment variables
char *get_env(const char *name);
int set_env(const char *name, const char *value, int overwrite);
int unset_env(const char *name);

// String utilities
void print_error(const char *format, ...);
void print_success(const char *format, ...);
void print_debug(const char *format, ...);
char *trim_whitespace(char *str);
char *strdup_safe(const char *str);
char **split_string(const char *str, const char *delim, int *count);
void free_array(char **array);

// Prompt generation
char *generate_prompt(void);
char *generate_rprompt(void);

// Configuration
void load_config(void);
void save_config(void);
void set_config_value(const char *key, const char *value);
char *get_config_value(const char *key);

#endif // SHELL_H