#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_BLOCK_LINES 32
#define MAX_VARS 64
#define MAX_JOBS 64

typedef struct {
    char name[64];
    char value[256];
} Variable;

Variable local_vars[MAX_VARS];
int local_var_count = 0;

Variable global_vars[MAX_VARS];
int global_var_count = 0;


typedef struct {
    pid_t pid;
    char command[MAX_LINE];
    int stopped;   // 0: running, 1: stopped
} Job;

Job jobs[MAX_JOBS];
int job_count = 0;

// PID of the current foreground process (-1 if none)
pid_t fg_pid = -1;


// Add a job to the job list; returns its 0-based index, or -1 if full.
int add_job(pid_t pid, const char *command, int stopped) {
    if (job_count >= MAX_JOBS) {
        fprintf(stderr, "Error: too many jobs (max %d)\n", MAX_JOBS);
        return -1;
    }
    jobs[job_count].pid     = pid;
    jobs[job_count].stopped = stopped;
    strncpy(jobs[job_count].command, command, MAX_LINE - 1);
    jobs[job_count].command[MAX_LINE - 1] = '\0';
    return job_count++;
}

// Remove a completed/terminated job by PID.
void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            // Shift remaining jobs down
            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }
            job_count--;
            return;
        }
    }
}

// Print a single job entry (command name only).
void print_job_status(int index) {
    const char *status = jobs[index].stopped ? "Stopped" : "Running";
    char cmd_copy[MAX_LINE];
    strncpy(cmd_copy, jobs[index].command, MAX_LINE);
    cmd_copy[MAX_LINE - 1] = '\0';
    char *cmd_name = strtok(cmd_copy, " \t");
    if (!cmd_name) cmd_name = jobs[index].command;
    printf("[%d] %s %d %s\n", index + 1, status, jobs[index].pid, cmd_name);
}

// Reap any background jobs that finished since we last checked.
void reap_background_jobs(void) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        // Find the job
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].pid == pid) {
                if (WIFSTOPPED(status)) {
                    // Already handled by SIGTSTP handler for fg; skip duplicates
                    jobs[i].stopped = 1;
                } else {
                    // Process finished — announce and remove
                    printf("\n[%d] Done    %d %s\n",
                           i + 1, jobs[i].pid, jobs[i].command);
                    remove_job(pid);
                }
                break;
            }
        }
    }
}


// When Ctrl+Z is pressed, forward SIGTSTP to the foreground process.
void handle_sigtstp(int sig) {
    (void)sig;
    if (fg_pid > 0) {
        kill(fg_pid, SIGTSTP);
    }
}

const char *get_var_value(const char *name) {
    for (int i = 0; i < local_var_count; i++)
        if (strcmp(local_vars[i].name, name) == 0)
            return local_vars[i].value;
    for (int i = 0; i < global_var_count; i++)
        if (strcmp(global_vars[i].name, name) == 0)
            return global_vars[i].value;
    const char *env = getenv(name);
    return env ? env : "";
}

void set_local_var(const char *name, const char *value) {
    for (int i = 0; i < local_var_count; i++) {
        if (strcmp(local_vars[i].name, name) == 0) {
            strncpy(local_vars[i].value, value, sizeof(local_vars[i].value));
            return;
        }
    }
    if (local_var_count < MAX_VARS) {
        strncpy(local_vars[local_var_count].name,  name,  sizeof(local_vars[0].name));
        strncpy(local_vars[local_var_count].value, value, sizeof(local_vars[0].value));
        local_var_count++;
    } else {
        fprintf(stderr, "Error: too many local variables (max %d)\n", MAX_VARS);
    }
}

void set_global_var(const char *name, const char *value) {
    if (!value) value = "";
    setenv(name, value, 1);
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_vars[i].name, name) == 0) {
            strncpy(global_vars[i].value, value, sizeof(global_vars[i].value));
            return;
        }
    }
    if (global_var_count < MAX_VARS) {
        strncpy(global_vars[global_var_count].name,  name,  sizeof(global_vars[0].name));
        strncpy(global_vars[global_var_count].value, value, sizeof(global_vars[0].value));
        global_var_count++;
    } else {
        fprintf(stderr, "Error: too many global variables (max %d)\n", MAX_VARS);
    }
}

void expand_variables(char *line) {
    char buffer[MAX_LINE];
    int bi = 0;
    for (int i = 0; line[i] != '\0'; ) {
        if (line[i] == '$') {
            i++;
            char varname[64];
            int vi = 0;
            while (isalnum(line[i]) || line[i] == '_')
                varname[vi++] = line[i++];
            varname[vi] = '\0';
            const char *val = get_var_value(varname);
            for (int j = 0; val[j]; j++)
                buffer[bi++] = val[j];
        } else {
            buffer[bi++] = line[i++];
        }
    }
    buffer[bi] = '\0';
    strncpy(line, buffer, MAX_LINE);
}


void parse_command(char *line, char **args) {
    expand_variables(line);
    int i = 0;
    char *token = strtok(line, " \t\n");
    while (token && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    if (token)
        fprintf(stderr, "Warning: too many arguments (max %d); some ignored\n",
                MAX_ARGS - 1);
}


// Execute an external command.
// background == 1 → don't wait; add to job list.
// Returns exit status for foreground commands, 0 for background.
int execute_external_command_bg(char **args, int background,
                                 const char *raw_cmd) {
    int input_fd = -1, output_fd = -1;
    char *clean_args[MAX_ARGS];
    int j = 0;

    for (int i = 0; args[i]; i++) {
        if (strcmp(args[i], "<") == 0 && args[i + 1]) {
            input_fd = open(args[i + 1], O_RDONLY);
            if (input_fd < 0) { perror("open for input"); return -1; }
            i++;
        } else if (strcmp(args[i], ">") == 0 && args[i + 1]) {
            output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_fd < 0) { perror("open for output"); return -1; }
            i++;
        } else {
            clean_args[j++] = args[i];
        }
    }
    clean_args[j] = NULL;

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {
        // Child
        if (input_fd  != -1) { dup2(input_fd,  STDIN_FILENO);  close(input_fd);  }
        if (output_fd != -1) { dup2(output_fd, STDOUT_FILENO); close(output_fd); }
        execvp(clean_args[0], clean_args);
        fprintf(stderr, "%s: command not found\n", clean_args[0]);
        exit(1);
    }

    // Parent
    if (background) {
        // Background: add to jobs list and return immediately
        add_job(pid, raw_cmd, 0 /*running*/);
        printf("[background pid %d]\n", pid);
        return 0;
    }

    // Foreground: record fg_pid so SIGTSTP handler can reach it
    fg_pid = pid;
    int status;
    waitpid(pid, &status, WUNTRACED);
    fg_pid = -1;

    if (WIFSTOPPED(status)) {
        // The foreground process was stopped (Ctrl+Z) — move to job list
        add_job(pid, raw_cmd, 1 /*stopped*/);
        printf("\n[Stopped] pid %d\n", pid);
        return 0;
    }

    if (input_fd  != -1) close(input_fd);
    if (output_fd != -1) close(output_fd);

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Legacy wrapper used by handle_if_block (always foreground, no raw_cmd needed)
int execute_external_command(char **args) {
    // Build a reconstructed command string for potential job listing
    char raw[MAX_LINE] = "";
    for (int i = 0; args[i]; i++) {
        if (i) strncat(raw, " ", MAX_LINE - strlen(raw) - 1);
        strncat(raw, args[i], MAX_LINE - strlen(raw) - 1);
    }
    return execute_external_command_bg(args, 0, raw);
}

// Execute built-in or external commands.
// raw_line is the original (pre-tokenised) command string, used for job labels.
void execute_command_raw(char **args, const char *raw_line) {
    (void)raw_line;   // label is used via 'label' buffer below
    if (!args[0]) return;

    // VAR=val syntax
    if (strchr(args[0], '=') && args[0][0] != '=') {
        char *eq = strchr(args[0], '=');
        *eq = '\0';
        set_local_var(args[0], eq + 1);
        return;
    }

    // Built-in: exit
    if (strcmp(args[0], "exit") == 0) exit(0);

    // Built-in: export
    if (strcmp(args[0], "export") == 0) {
        for (int i = 1; args[i]; i++) {
            char *eq = strchr(args[i], '=');
            if (eq) { *eq = '\0'; set_global_var(args[i], eq + 1); }
            else    { set_global_var(args[i], get_var_value(args[i])); }
        }
        return;
    }

    // Built-in: set
    if (strcmp(args[0], "set") == 0) {
        for (int i = 0; i < local_var_count;  i++)
            printf("%s=%s\n",        local_vars[i].name,  local_vars[i].value);
        for (int i = 0; i < global_var_count; i++)
            printf("export %s=%s\n", global_vars[i].name, global_vars[i].value);
        return;
    }
    
    if (strcmp(args[0], "jobs") == 0) {
        reap_background_jobs();   // refresh finished/stopped status first
        if (job_count == 0) {
            printf("No background jobs.\n");
        } else {
            for (int i = 0; i < job_count; i++)
                print_job_status(i);
        }
        return;
    }

    // Built-in: fg
    if (strcmp(args[0], "fg") == 0) {
        int target_idx = -1;

        if (args[1] == NULL) {
            // Case 1: 'fg' -> find the most recently stopped job (scan backwards)
            for (int i = job_count - 1; i >= 0; i--) {
                if (jobs[i].stopped) {
                    target_idx = i;
                    break;
                }
            }
            if (target_idx == -1) {
                fprintf(stderr, "fg: no stopped jobs\n");
                return;
            }
        } else {
            // Case 2: 'fg N' -> find specific job index
            int n = atoi(args[1]);
            if (n <= 0 || n > job_count) {
                fprintf(stderr, "fg: no such job\n");
                return;
            }
            target_idx = n - 1;
            if (!jobs[target_idx].stopped) {
                fprintf(stderr, "fg: job [%d] is not stopped\n", n);
                return;
            }
        }

        // Target job successfully found; save its properties
        pid_t pid = jobs[target_idx].pid;
        printf("Resuming job [%d] %s\n", target_idx + 1, jobs[target_idx].command);

        // Put process into the shell's foreground tracker
        fg_pid = pid;

        // Resume process execution via SIGCONT
        if (kill(pid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
            fg_pid = -1;
            return;
        }

        // Block shell prompt and wait for the process to stop or terminate
        int status;
        waitpid(pid, &status, WUNTRACED);
        fg_pid = -1; // Reset foreground process tracker once done

        if (WIFSTOPPED(status)) {
            // Process caught another Ctrl+Z -> mark as stopped again
            jobs[target_idx].stopped = 1;
            printf("\n[Stopped] pid %d\n", pid);
        } else {
            // Process completed or terminated -> completely clear it from the list
            remove_job(pid);
        }
        return;
    }

    int background = 0;
    // Find the last non-NULL argument
    int last = 0;
    while (args[last + 1]) last++;
    if (strcmp(args[last], "&") == 0) {
        background = 1;
        args[last] = NULL;   // strip the '&' from the argument list
    }

    // Build a clean label (without the trailing '&') for the job list
    char label[MAX_LINE] = "";
    for (int i = 0; args[i]; i++) {
        if (i) strncat(label, " ", MAX_LINE - strlen(label) - 1);
        strncat(label, args[i], MAX_LINE - strlen(label) - 1);
    }

    execute_external_command_bg(args, background, label);
}

// Thin wrapper so old call-sites (handle_if_block) still work
void execute_command(char **args) {
    char raw[MAX_LINE] = "";
    for (int i = 0; args[i]; i++) {
        if (i) strncat(raw, " ", MAX_LINE - strlen(raw) - 1);
        strncat(raw, args[i], MAX_LINE - strlen(raw) - 1);
    }
    execute_command_raw(args, raw);
}

int is_blank_line(const char *line) {
    while (*line) { if (!isspace(*line)) return 0; line++; }
    return 1;
}

void handle_if_block(char *if_line, FILE *input) {
    char cond_line[MAX_LINE];
    strncpy(cond_line, if_line + 3, MAX_LINE);
    cond_line[MAX_LINE - 1] = '\0';

    char line[MAX_LINE];
    if (!fgets(line, sizeof(line), input)) {
        fprintf(stderr, "Syntax error: expected 'then'\n"); return;
    }
    char *trimmed = strtok(line, " \t\n");
    if (!trimmed || strcmp(trimmed, "then") != 0) {
        fprintf(stderr, "Syntax error: expected 'then'\n"); return;
    }

    char block[MAX_BLOCK_LINES][MAX_LINE];
    int count = 0, found_fi = 0;

    while (fgets(line, sizeof(line), input)) {
        if (is_blank_line(line)) continue;
        char tmp[MAX_LINE];
        strncpy(tmp, line, MAX_LINE);
        tmp[MAX_LINE - 1] = '\0';
        char *tok = strtok(tmp, " \t\n");
        if (tok && strcmp(tok, "fi") == 0) { found_fi = 1; break; }
        if (count >= MAX_BLOCK_LINES) {
            fprintf(stderr, "Error: too many lines in if block\n"); return;
        }
        strncpy(block[count], line, MAX_LINE);
        block[count][MAX_LINE - 1] = '\0';
        count++;
    }

    if (!found_fi) { fprintf(stderr, "Syntax error: missing 'fi'\n"); return; }

    char *args[MAX_ARGS];
    parse_command(cond_line, args);
    int cond_result = execute_external_command(args);
    if (cond_result == 0) {
        for (int i = 0; i < count; i++) {
            parse_command(block[i], args);
            execute_command(args);
        }
    }
}


void process_line(char *line, FILE *input) {
    if (is_blank_line(line)) return;

    // Save a copy of the raw line for job labels (parse_command mutates it)
    char raw[MAX_LINE];
    strncpy(raw, line, MAX_LINE);
    raw[MAX_LINE - 1] = '\0';
    // Strip trailing newline from label
    char *nl = strchr(raw, '\n');
    if (nl) *nl = '\0';

    if (strncmp(line, "if ", 3) == 0) {
        handle_if_block(line, input);
    } else {
        char *args[MAX_ARGS];
        parse_command(line, args);
        execute_command_raw(args, raw);
    }

    // Opportunistically reap any finished background jobs
    reap_background_jobs();
}


int main(int argc, char *argv[]) {
    char line[MAX_LINE];

    // Register SIGTSTP handler so Ctrl+Z stops the foreground child,
    // not the shell itself.
    signal(SIGTSTP, handle_sigtstp);

    // Script file mode
    if (argc == 2) {
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { perror("fopen"); return 1; }
        while (fgets(line, sizeof(line), fp))
            process_line(line, fp);
        fclose(fp);
        return 0;
    }

    // Interactive shell mode
    while (1) {
        printf("mini-shell> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        process_line(line, stdin);
    }
    return 0;
}