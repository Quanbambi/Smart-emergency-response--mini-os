#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#define MAX_PROCESSES 10
#define NUM_RESOURCES 3
enum State { NEW, READY, RUNNING, WAITING, TERMINATED };
enum Priority { HIGH = 1, MEDIUM = 2, LOW = 3 };
struct PCB {
    int process_id;
    char name[20];
    char description[100];
    enum Priority priority;
    int burst_time;
    int remaining_time;
    int waiting_time;
    int turnaround_time;
    enum State state;
};
struct PCB process_table[MAX_PROCESSES];
int process_count = 0;
int next_pid = 1;
int available[NUM_RESOURCES] = {5, 4, 10};
int max_need[MAX_PROCESSES][NUM_RESOURCES];
int allocation[MAX_PROCESSES][NUM_RESOURCES];
int need[MAX_PROCESSES][NUM_RESOURCES];
void init_process_resources(int process_index);
void log_process_action(char *action, int process_id, char *name, char *description);
void log_scheduling(char *algorithm, float avg_wait, float avg_turn);
int handle_menu_choice(int choice);
const char* state_to_str(enum State state) {
    if (state == NEW) return "NEW";
    if (state == READY) return "READY";
    if (state == RUNNING) return "RUNNING";
    if (state == WAITING) return "WAITING";
    if (state == TERMINATED) return "TERMINATED";
    return "UNKNOWN";
}
enum State str_to_state(const char *state_str) {
    if (strcmp(state_str, "NEW") == 0) return NEW;
    if (strcmp(state_str, "READY") == 0) return READY;
    if (strcmp(state_str, "RUNNING") == 0) return RUNNING;
    if (strcmp(state_str, "WAITING") == 0) return WAITING;
    if (strcmp(state_str, "TERMINATED") == 0) return TERMINATED;
    return NEW;
}
void save_process_to_file(struct PCB proc) {
    FILE *fp = fopen("processes.txt", "a");
    if (fp == NULL) {
        printf("Error: Cannot open processes.txt for writing!\n");
        return;
    }
    fprintf(fp, "%d,%s,%s,%d,%s,%d,%d,%d,%d\n",
            proc.process_id, proc.name, proc.description, (int)proc.priority,
            state_to_str(proc.state), proc.burst_time, proc.remaining_time,
            proc.waiting_time, proc.turnaround_time);
    fclose(fp);
}
void load_processes_from_file() {
    FILE *fp = fopen("processes.txt", "r");
    if (fp == NULL) {
        return;
    }
    process_count = 0;
    next_pid = 1;
    char line[300];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        struct PCB proc;
        char state_str[20];
        int comma_positions[32];
        int comma_count = 0;
        for (int i = 0; i < strlen(line); i++) {
            if (line[i] == ',') {
                if (comma_count < 32) {
                    comma_positions[comma_count] = i;
                }
                comma_count++;
            }
        }
        if (comma_count < 8) {
            continue;
        }
        int idx_pid = 0;
        int idx_name = 1;
        int idx_priority = comma_count - 6;
        int idx_state = comma_count - 5;
        int idx_burst = comma_count - 4;
        int idx_remaining = comma_count - 3;
        int idx_waiting = comma_count - 2;
        int idx_turnaround = comma_count - 1;
        char pid_buf[16];
        int pid_len = comma_positions[idx_pid] - 0;
        strncpy(pid_buf, line, pid_len < 15 ? pid_len : 15);
        pid_buf[pid_len < 15 ? pid_len : 15] = '\0';
        proc.process_id = atoi(pid_buf);
        int name_len = comma_positions[idx_name] - comma_positions[idx_pid] - 1;
        strncpy(proc.name, line + comma_positions[idx_pid] + 1, name_len < 19 ? name_len : 19);
        proc.name[name_len < 19 ? name_len : 19] = '\0';
        int desc_start = comma_positions[idx_name] + 1;
        int desc_end = comma_positions[idx_priority];
        int desc_len = desc_end - desc_start;
        strncpy(proc.description, line + desc_start, desc_len < 99 ? desc_len : 99);
        proc.description[desc_len < 99 ? desc_len : 99] = '\0';
        char priority_buf[8];
        int priority_len = comma_positions[idx_state] - comma_positions[idx_priority] - 1;
        strncpy(priority_buf, line + comma_positions[idx_priority] + 1, priority_len < 7 ? priority_len : 7);
        priority_buf[priority_len < 7 ? priority_len : 7] = '\0';
        proc.priority = (enum Priority)atoi(priority_buf);
        int state_len = comma_positions[idx_burst] - comma_positions[idx_state] - 1;
        strncpy(state_str, line + comma_positions[idx_state] + 1, state_len < 19 ? state_len : 19);
        state_str[state_len < 19 ? state_len : 19] = '\0';
        proc.state = str_to_state(state_str);
        proc.burst_time = atoi(line + comma_positions[idx_burst] + 1);
        proc.remaining_time = atoi(line + comma_positions[idx_remaining] + 1);
        proc.waiting_time = atoi(line + comma_positions[idx_waiting] + 1);
        proc.turnaround_time = atoi(line + comma_positions[idx_turnaround] + 1);
        if (process_count < MAX_PROCESSES) {
            process_table[process_count] = proc;
            process_count++;
            if (proc.process_id >= next_pid) {
                next_pid = proc.process_id + 1;
            }
        }
    }
    fclose(fp);
}
void update_process_in_file(struct PCB proc) {
    FILE *temp_fp = fopen("processes_temp.txt", "w");
    FILE *fp = fopen("processes.txt", "r");
    
    if (fp == NULL || temp_fp == NULL) {
        printf("Error: Cannot access process file!\n");
        if (temp_fp) fclose(temp_fp);
        if (fp) fclose(fp);
        return;
    }
    char line[300];
    while (fgets(line, sizeof(line), fp)) {
        int pid;
        if (sscanf(line, "%d,", &pid) == 1) {
            if (pid == proc.process_id) {
                fprintf(temp_fp, "%d,%s,%s,%d,%s,%d,%d,%d,%d\n",
                        proc.process_id, proc.name, proc.description, (int)proc.priority,
                        state_to_str(proc.state), proc.burst_time, proc.remaining_time,
                        proc.waiting_time, proc.turnaround_time);
            } else {
                fprintf(temp_fp, "%s", line);
            }
        }
    }
    fclose(fp);
    fclose(temp_fp);
    
    remove("processes.txt");
    rename("processes_temp.txt", "processes.txt");
}
void delete_process_from_file(int process_id) {
    FILE *temp_fp = fopen("processes_temp.txt", "w");
    FILE *fp = fopen("processes.txt", "r");
    
    if (fp == NULL) {
        printf("No processes file found.\n");
        if (temp_fp) fclose(temp_fp);
        return;
    }
    if (temp_fp == NULL) {
        printf("Error: Cannot write to temporary file!\n");
        fclose(fp);
        return;
    }
    char line[300];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        int pid;
        if (sscanf(line, "%d,", &pid) == 1) {
            if (pid == process_id) {
                found = 1;
            } else {
                fprintf(temp_fp, "%s", line);
            }
        }
    }
    fclose(fp);
    fclose(temp_fp);
    
    if (found) {
        remove("processes.txt");
        rename("processes_temp.txt", "processes.txt");
        printf("Process %d deleted from storage.\n", process_id);
    } else {
        remove("processes_temp.txt");
        printf("Process %d not found in storage.\n", process_id);
    }
}
void delete_all_processes_from_file() {
    if (remove("processes.txt") == 0) {
        printf("All processes deleted from storage.\n");
    } else {
        printf("No process storage file found to delete.\n");
    }
}
void delete_all_processes() {
    process_count = 0;
    next_pid = 1;
    delete_all_processes_from_file();
}
void persist_all_processes() {
    FILE *fp = fopen("processes.txt", "w");
    if (fp == NULL) {
        printf("Error: Cannot open processes.txt for writing!\n");
        return;
    }
    for (int i = 0; i < process_count; i++) {
        struct PCB *p = &process_table[i];
        fprintf(fp, "%d,%s,%s,%d,%s,%d,%d,%d,%d\n",
                p->process_id, p->name, p->description, (int)p->priority,
                state_to_str(p->state), p->burst_time, p->remaining_time,
                p->waiting_time, p->turnaround_time);
    }
    fclose(fp);
}
void create_process_record(char name[], char description[], enum Priority priority, int burst_time) {
    if (process_count >= MAX_PROCESSES) {
        printf("Cannot create more processes.\n");
        return;
    }
    struct PCB new_process;
    new_process.process_id = next_pid++;
    strcpy(new_process.name, name);
    strcpy(new_process.description, description);
    new_process.priority = priority;
    new_process.burst_time = burst_time;
    new_process.remaining_time = burst_time;
    new_process.waiting_time = 0;
    new_process.turnaround_time = 0;
    new_process.state = NEW;
    process_table[process_count] = new_process;
    process_count++;
    save_process_to_file(new_process);
}
void create_process() {
    char name[20];
    char description[100];
    int priority_input;
    int burst;
    enum Priority priority;
    if (process_count >= MAX_PROCESSES) {
        printf("Cannot create more processes.\n");
        return;
    }
    printf("Enter process name: ");
    scanf("%19s", name);
    getchar();
    printf("Enter emergency description: ");
    fgets(description, sizeof(description), stdin);
    size_t len = strlen(description);
    if (len > 0 && description[len-1] == '\n') {
        description[len-1] = '\0';
    }
    do {
        printf("Enter priority (1=High, 2=Medium, 3=Low): ");
        if (scanf("%d", &priority_input) != 1) {
            while (getchar() != '\n');
            priority_input = 0;
        }
        if (priority_input < 1 || priority_input > 3) {
            printf("Invalid priority. Please enter 1, 2, or 3.\n");
        }
    } while (priority_input < 1 || priority_input > 3);
    priority = (enum Priority)priority_input;
    printf("Enter burst time: ");
    scanf("%d", &burst);
    create_process_record(name, description, priority, burst);
    init_process_resources(process_count - 1);
    
    printf("\nThe emergency process '%s' (PID=%d) has been successfully created and set to NEW state.\n\n", name, next_pid - 1);
    log_process_action("CREATED", next_pid - 1, name, description);
}
const char* priority_to_str(enum Priority p) {
    if (p == HIGH) return "High";
    if (p == MEDIUM) return "Medium";
    return "Low";
}
void list_processes() {
    load_processes_from_file();
    
    printf("\nProcess table\n");
    printf("PID  Name                Priority  State     Burst  Description\n");
    for (int i = 0; i < process_count; i++) {
        printf("%-4d %-20s %-8s %-10s %-5d %s\n",
               process_table[i].process_id,
               process_table[i].name,
               priority_to_str(process_table[i].priority),
               state_to_str(process_table[i].state),
               process_table[i].burst_time,
               process_table[i].description);
    }
}
void suspend_process(int process_id) {
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].process_id == process_id) {
            if (process_table[i].state == RUNNING || process_table[i].state == READY) {
                process_table[i].state = WAITING;
                update_process_in_file(process_table[i]);
                printf("Process %d suspended (now WAITING)\n", process_id);
            } else {
                printf("Cannot suspend process %d in current state\n", process_id);
            }
            return;
        }
    }
    printf("Process %d not found!\n", process_id);
}
void terminate_process(int process_id) {
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].process_id == process_id) {
            if (process_table[i].state == TERMINATED) {
                printf("Process %d is already terminated.\n", process_id);
                return;
            }
            process_table[i].state = TERMINATED;
            for (int j = 0; j < NUM_RESOURCES; j++) {
                available[j] += allocation[i][j];
                allocation[i][j] = 0;
                need[i][j] = max_need[i][j];
            }
            update_process_in_file(process_table[i]);
            printf("Process %d terminated and its resources were freed.\n", process_id);
            return;
        }
    }
    printf("Process %d not found!\n", process_id);
}
void delete_process(int process_id) {
    delete_process_from_file(process_id);
    
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].process_id == process_id) {
            for (int j = i; j < process_count - 1; j++) {
                process_table[j] = process_table[j + 1];
            }
            process_count--;
            return;
        }
    }
}
void reset_to_ready() {
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].state != TERMINATED) {
            process_table[i].state = READY;
            update_process_in_file(process_table[i]);
        }
    }
}
void schedule_fcfs() {
    printf("\nFCFS scheduling\n");
    int current_time = 0;
    int total_waiting = 0;
    int total_turnaround = 0;
    int completed = 0;
    reset_to_ready();
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].state == TERMINATED) continue;
        process_table[i].waiting_time = current_time;
        process_table[i].turnaround_time = current_time + process_table[i].burst_time;
        process_table[i].state = RUNNING;
        update_process_in_file(process_table[i]);
        printf("Running: %s (PID=%d) | Waiting=%d | Turnaround=%d\n",
               process_table[i].name, process_table[i].process_id,
               process_table[i].waiting_time, process_table[i].turnaround_time);
        printf("  [Simulating %d time unit(s) of execution...]\n", process_table[i].burst_time);
        current_time += process_table[i].burst_time;
        total_waiting += process_table[i].waiting_time;
        total_turnaround += process_table[i].turnaround_time;
        completed++;
        process_table[i].state = TERMINATED;
        update_process_in_file(process_table[i]);
    }
    if (completed == 0) {
        printf("No processes to schedule.\n");
        return;
    }
    printf("\nResults:\n");
    printf("Total processes: %d\n", completed);
    int total_burst = 0;
    for (int i = 0; i < process_count; i++) total_burst += process_table[i].burst_time;
    float cpu_util = (total_burst > 0) ? ((float)total_burst / current_time) * 100.0f : 0;
    float avg_waiting = (float)total_waiting / completed;
    float avg_turnaround = (float)total_turnaround / completed;
    printf("Average Waiting Time: %.2f\n", avg_waiting);
    printf("Average Turnaround Time: %.2f\n", avg_turnaround);
    printf("CPU Utilization: %.2f%%\n", cpu_util);
    log_scheduling("FCFS", avg_waiting, avg_turnaround);
}
void schedule_priority() {
    printf("\nPriority scheduling\n");
    struct PCB temp[MAX_PROCESSES];
    int temp_count = 0;
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].state != TERMINATED) {
            temp[temp_count] = process_table[i];
            temp_count++;
        }
    }
    if (temp_count == 0) {
        printf("No processes to schedule.\n");
        return;
    }
    for (int i = 0; i < temp_count - 1; i++) {
        for (int j = 0; j < temp_count - i - 1; j++) {
            if (temp[j].priority > temp[j+1].priority) {
                struct PCB t = temp[j];
                temp[j] = temp[j+1];
                temp[j+1] = t;
            }
        }
    }
    int current_time = 0;
    int total_waiting = 0;
    int total_turnaround = 0;
    for (int i = 0; i < temp_count; i++) {
        temp[i].waiting_time = current_time;
        temp[i].turnaround_time = current_time + temp[i].burst_time;
        int pid = temp[i].process_id;
        int idx = -1;
        for (int k = 0; k < process_count; k++) {
            if (process_table[k].process_id == pid) {
                idx = k;
                break;
            }
        }
        if (idx != -1) {
            process_table[idx].waiting_time = temp[i].waiting_time;
            process_table[idx].turnaround_time = temp[i].turnaround_time;
            process_table[idx].state = RUNNING;
            update_process_in_file(process_table[idx]);
        }
        printf("Running: %s (PID=%d, Priority=%s) | Waiting=%d | Turnaround=%d\n",
               temp[i].name, temp[i].process_id, priority_to_str(temp[i].priority),
               temp[i].waiting_time, temp[i].turnaround_time);
        printf("  [Simulating %d time unit(s) of execution...]\n", temp[i].burst_time);
        current_time += temp[i].burst_time;
        total_waiting += temp[i].waiting_time;
        total_turnaround += temp[i].turnaround_time;
        if (idx != -1) {
            process_table[idx].state = TERMINATED;
            update_process_in_file(process_table[idx]);
        }
    }
    printf("\nResults:\n");
    float avg_waiting = (float)total_waiting / temp_count;
    float avg_turnaround = (float)total_turnaround / temp_count;
    int total_burst = 0;
    for (int i = 0; i < temp_count; i++) total_burst += temp[i].burst_time;
    float cpu_util = (total_burst > 0) ? ((float)total_burst / current_time) * 100.0f : 0;
    printf("Average Waiting Time: %.2f\n", avg_waiting);
    printf("Average Turnaround Time: %.2f\n", avg_turnaround);
    printf("CPU Utilization: %.2f%%\n", cpu_util);
    log_scheduling("PRIORITY", avg_waiting, avg_turnaround);
}
void init_process_resources(int process_index) {
    const char *resource_names[NUM_RESOURCES] = {"ambulances", "fire trucks", "police units"};
    int max_req[NUM_RESOURCES];
    for (int j = 0; j < NUM_RESOURCES; j++) {
        while (1) {
            printf("Enter number of %s needed for %s: ",
                   resource_names[j], process_table[process_index].name);
            if (scanf("%d", &max_req[j]) != 1) {
                while (getchar() != '\n');
                printf("Invalid input. Enter a whole number.\n");
                continue;
            }
            if (max_req[j] < 0) {
                printf("Resource count cannot be negative. Try again.\n");
                continue;
            }
            break;
        }
    }
    for (int j = 0; j < NUM_RESOURCES; j++) {
        max_need[process_index][j] = max_req[j];
        allocation[process_index][j] = 0;
        need[process_index][j] = max_req[j];
    }
}
int is_safe_state(int num_proc) {
    int work[NUM_RESOURCES];
    int finish[MAX_PROCESSES] = {0};
    int safe_sequence[MAX_PROCESSES];
    int count = 0;
    for (int i = 0; i < NUM_RESOURCES; i++) {
        work[i] = available[i];
    }
    while (count < num_proc) {
        int found = 0;
        for (int p = 0; p < num_proc; p++) {
            if (!finish[p]) {
                int can_alloc = 1;
                for (int r = 0; r < NUM_RESOURCES; r++) {
                    if (need[p][r] > work[r]) {
                        can_alloc = 0;
                        break;
                    }
                }
                if (can_alloc) {
                    for (int r = 0; r < NUM_RESOURCES; r++) {
                        work[r] += allocation[p][r];
                    }
                    safe_sequence[count] = p;
                    finish[p] = 1;
                    count++;
                    found = 1;
                }
            }
        }
        if (!found) {
            printf("!!! SYSTEM IS IN UNSAFE STATE - Deadlock may occur !!!\n");
            return 0;
        }
    }
    printf("System is SAFE. Safe sequence: ");
    for (int i = 0; i < count; i++) {
        printf("P%d ", safe_sequence[i]);
    }
    printf("\n");
    return 1;
}
void request_resources(int process_id, int req[]) {
    int index = -1;
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].process_id == process_id) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        printf("Process not found!\n");
        return;
    }
    for (int i = 0; i < NUM_RESOURCES; i++) {
        if (req[i] > need[index][i]) {
            printf("Request exceeds maximum claim!\n");
            return;
        }
        if (req[i] > available[i]) {
            printf("Resources not available. Process must wait.\n");
            return;
        }
    }
    for (int i = 0; i < NUM_RESOURCES; i++) {
        available[i] -= req[i];
        allocation[index][i] += req[i];
        need[index][i] -= req[i];
    }
    if (is_safe_state(process_count)) {
        printf("Resources allocated successfully to PID %d\n", process_id);
    } else {
        for (int i = 0; i < NUM_RESOURCES; i++) {
            available[i] += req[i];
            allocation[index][i] -= req[i];
            need[index][i] += req[i];
        }
        printf("Allocation denied to prevent deadlock!\n");
    }
}
void show_resources() {
    printf("\nResource allocation\n");
    printf("Available Resources: R0(Ambulances)=%d  R1(Fire Units)=%d  R2(Police Units)=%d\n\n",
           available[0], available[1], available[2]);
    printf("PID  Process          Alloc(R0,R1,R2)  Need(R0,R1,R2)\n");
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].state != TERMINATED) {
            printf("%-4d %-16s (%d,%d,%d)          (%d,%d,%d)\n",
                   process_table[i].process_id,
                   process_table[i].name,
                   allocation[i][0], allocation[i][1], allocation[i][2],
                   need[i][0], need[i][1], need[i][2]);
        }
    }
}
void get_timestamp(char *buf, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
}
void log_event(char *event) {
    FILE *fp = fopen("serc_log.txt", "a");
    if (fp == NULL) {
        printf("Error opening log file!\n");
        return;
    }
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [LOG] %s\n", ts, event);
    fclose(fp);
}
void log_process_action(char *action, int process_id, char *name, char *description) {
    FILE *fp = fopen("serc_log.txt", "a");
    if (fp == NULL) {
        printf("Cannot write to log file!\n");
        return;
    }
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [PROCESS] %s | PID=%d | Name=%s | Description=%s\n",
            ts, action, process_id, name, description);
    fclose(fp);
}
void log_scheduling(char *algorithm, float avg_wait, float avg_turn) {
    FILE *fp = fopen("serc_log.txt", "a");
    if (fp == NULL) return;
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [SCHEDULER] %s | AvgWait=%.2f | AvgTurn=%.2f\n",
            ts, algorithm, avg_wait, avg_turn);
    fclose(fp);
}
void log_deadlock_event(char *event, int process_id) {
    FILE *fp = fopen("serc_log.txt", "a");
    if (fp == NULL) return;
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [DEADLOCK] %s | PID=%d\n", ts, event, process_id);
    fclose(fp);
}
void view_log() {
    FILE *fp = fopen("serc_log.txt", "r");
    if (fp == NULL) {
        printf("No log file found. Create some events first.\n");
        return;
    }
    printf("\nSystem log\n");
    char line[200];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    fclose(fp);
}
void clear_log() {
    FILE *fp = fopen("serc_log.txt", "w");
    if (fp == NULL) {
        printf("Cannot clear log!\n");
        return;
    }
    fclose(fp);
    printf("Log file cleared.\n");
}
int handle_menu_choice(int choice) {
    int process_id;
    
    if (choice == 1) {
        create_process();
    } else if (choice == 2) {
        list_processes();
    } else if (choice == 3) {
        printf("Enter PID to suspend: ");
        scanf("%d", &process_id);
        char desc[100] = "";
        for (int i = 0; i < process_count; i++) {
            if (process_table[i].process_id == process_id) {
                strcpy(desc, process_table[i].description);
                break;
            }
        }
        suspend_process(process_id);
        log_process_action("SUSPENDED", process_id, "", desc);
    } else if (choice == 4) {
        printf("Enter PID to terminate: ");
        scanf("%d", &process_id);
        char desc[100] = "";
        for (int i = 0; i < process_count; i++) {
            if (process_table[i].process_id == process_id) {
                strcpy(desc, process_table[i].description);
                break;
            }
        }
        terminate_process(process_id);
        log_process_action("TERMINATED", process_id, "", desc);
    } else if (choice == 5) {
        int alg_choice;
        printf("Choose scheduling algorithm: 1=FCFS, 2=Priority: ");
        scanf("%d", &alg_choice);
        if (alg_choice == 1) {
            schedule_fcfs();
        } else if (alg_choice == 2) {
            schedule_priority();
        } else {
            printf("Invalid scheduling option. Use 1 or 2.\n");
        }
    } else if (choice == 6) {
        show_resources();
    } else if (choice == 7) {
        int req[3];
        printf("Enter PID: ");
        scanf("%d", &process_id);
        printf("Enter request for (Ambulances, Fire Units, Police Units): ");
        scanf("%d %d %d", &req[0], &req[1], &req[2]);
        request_resources(process_id, req);
        log_deadlock_event("REQUEST", process_id);
    } else if (choice == 8) {
        view_log();
    } else if (choice == 9) {
        clear_log();
    } else if (choice == 10) {
        int sub_choice;
        printf("Delete single process (1) or delete all processes (2)? ");
        scanf("%d", &sub_choice);
        if (sub_choice == 1) {
            printf("Enter PID to delete: ");
            scanf("%d", &process_id);
            delete_process(process_id);
        } else if (sub_choice == 2) {
            char confirm;
            printf("Are you sure you want to delete all processes? (y/n): ");
            scanf(" %c", &confirm);
            if (confirm == 'y' || confirm == 'Y') {
                delete_all_processes();
            } else {
                printf("Delete all cancelled.\n");
            }
        } else {
            printf("Invalid delete option.\n");
        }
    } else if (choice == 12) {
        log_event("System shutdown");
        printf("Shutting down SERC OS...\n");
        return 0;
    } else {
        printf("Invalid option! Try again.\n");
    }
    
    return 1;
}
int main() {
    int choice;
    printf("SMART EMERGENCY RESPONSE CENTER OS SIMULATOR\n");
    log_event("System started");
    load_processes_from_file();
    while (1) {
        printf("\nMain menu\n");
        printf("1.  Create new process\n");
        printf("2.  List all processes\n");
        printf("3.  Suspend process\n");
        printf("4.  Terminate process\n");
        printf("5.  Execute processes\n");
        printf("6.  Show resource allocation\n");
        printf("7.  Request resources\n");
        printf("8.  View system log\n");
        printf("9.  Clear log\n");
        printf("10. Delete process/all processes\n");
        printf("11. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        if (!handle_menu_choice(choice)) {
            break;
        }
    }
    return 0;
}
