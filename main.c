#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
//Global declarations
#define maxProcesses 10
#define numResources 3
#define TOTAL_MEMORY 1400 
#define NUM_PARTITIONS 6
enum State { NEW, READY, RUNNING, WAITING, TERMINATED };
enum Priority { HIGH = 1, MEDIUM = 2, LOW = 3};
// Process Control Block (PCB) structure
struct PCB {
    int processID;
    char name[20];
    char description[100];
    enum Priority priority;
    int burst_time;
    int remaining_time;
    int waiting_time;
    int turnaround_time;
    enum State state;
    int memory_required;
    int memory_partition_id;
    int arrival_time;
};
// Some global variables to manage processes and resources
struct PCB processTable[maxProcesses];
int processCount = 0, nextPID = 1, available[numResources] = {5, 4, 10}, max_need[maxProcesses][numResources], allocation[maxProcesses][numResources], need[maxProcesses][numResources];
void init_process_resources(int process_index);
void log_process_action(char *action, int processID, char *name, char *description);
void log_scheduling(char *algorithm, float avg_wait, float avg_turn);
void init_system_memory();
int allocate_memory_first_fit(int pid, int size_required);
void free_memory_by_pid(int pid);
void show_memory_map();
void free_system_memory();
void release_process_resources(int process_index);
// memory partition structure and management
typedef struct MemoryPartition {
    int block_id;
    int total_size; 
    int free_space; 
    int is_occupied; 
    int assigned_pid;
} MemoryPartition;

MemoryPartition memory_map[NUM_PARTITIONS];
int handle_menu_choice(int choice);
//Function converts enum to string 
const char* stateToString(enum State state) {
    if (state == NEW) return "NEW";
    if (state == READY) return "READY";
    if (state == RUNNING) return "RUNNING";
    if (state == WAITING) return "WAITING";
    if (state == TERMINATED) return "TERMINATED";
    return "UNKNOWN";
}
// Function converts string to enum value
enum State stringToState(const char *stateString) {
    if (strcmp(stateString, "NEW") == 0) return NEW;
    if (strcmp(stateString, "READY") == 0) return READY;
    if (strcmp(stateString, "RUNNING") == 0) return RUNNING;
    if (strcmp(stateString, "WAITING") == 0) return WAITING;
    if (strcmp(stateString, "TERMINATED") == 0) return TERMINATED;
    return NEW;
}
void saveProcessToFile(struct PCB process) {
    FILE *fp = fopen("processes.txt", "a");
    if (fp == NULL) {
        printf("Error: Cannot open processes.txt for writing!\n");
        return;
    }
        fprintf(fp, "%d,%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d\n",
            process.processID, process.name, process.description, (int)process.priority,
            stateToString(process.state), process.burst_time, process.remaining_time,
            process.waiting_time, process.turnaround_time,
            process.memory_required, process.memory_partition_id,
            process.arrival_time);
    fclose(fp);
}
 
void loadProcessesFromFile() {
    FILE *fp = fopen("processes.txt", "r");
    if (fp == NULL) {   
        return ;
    }
    processCount = 0;
    nextPID = 1;
    char line[300];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        struct PCB process;
        char stateString[20];
        int comma_positions[32];
        int comma_count = 0;
        for (size_t i = 0; i < strlen(line); i++) {
            if (line[i] == ',') {
                if (comma_count < 32) {
                    comma_positions[comma_count] = (int)i;
                }
                comma_count++;
            }
        }
        if (comma_count < 11) {
            continue;
        }
        int index_pid = 0, indexName = 1, indexPriority = comma_count - 9, indexState = comma_count - 8, index_burst = comma_count - 7, index_remaining = comma_count - 6, index_waiting = comma_count - 5, index_turnaround = comma_count - 4, index_memory = comma_count - 3, index_partition = comma_count - 2, index_arrival = comma_count - 1, pidLength = comma_positions[index_pid];
        char pidBuffer[16];
        //Copy Pid
        strncpy(pidBuffer, line, pidLength < 15 ? pidLength : 15);
        pidBuffer[pidLength < 15 ? pidLength : 15] = '\0';
        process.processID = atoi(pidBuffer);
        //copy process name
        int name_len = comma_positions[indexName] - comma_positions[index_pid] - 1;
        strncpy(process.name, line + comma_positions[index_pid] + 1, name_len < 19 ? name_len : 19);
        process.name[name_len < 19 ? name_len : 19] = '\0';
        //copy process description
        int descriptionStart = comma_positions[indexName] + 1;
        int descriptionEnd = comma_positions[indexPriority];
        int descriptionLength = descriptionEnd - descriptionStart;
        strncpy(process.description, line + descriptionStart, descriptionLength < 99 ? descriptionLength : 99);
        process.description[descriptionLength < 99 ? descriptionLength : 99] = '\0';
        //copy process priority
        char priorityBuffer[8];
        int priorityLength = comma_positions[indexState] - comma_positions[indexPriority] - 1;
        strncpy(priorityBuffer, line + comma_positions[indexPriority] + 1, priorityLength < 7 ? priorityLength : 7);
        priorityBuffer[priorityLength < 7 ? priorityLength : 7] = '\0';
        process.priority = (enum Priority)atoi(priorityBuffer);
        //copy process state
        int stateLength = comma_positions[index_burst] - comma_positions[indexState] - 1;
        strncpy(stateString, line + comma_positions[indexState] + 1, stateLength < 19 ? stateLength : 19);
        stateString[stateLength < 19 ? stateLength : 19] = '\0';
        process.state = stringToState(stateString);
        //copy remaining fields
        process.burst_time = atoi(line + comma_positions[index_burst] + 1);
        process.remaining_time = atoi(line + comma_positions[index_remaining] + 1);
        process.waiting_time = atoi(line + comma_positions[index_waiting] + 1);
        process.turnaround_time = atoi(line + comma_positions[index_turnaround] + 1);
        process.memory_required = atoi(line + comma_positions[index_memory] + 1);
        process.memory_partition_id = atoi(line + comma_positions[index_partition] + 1);
        process.arrival_time = atoi(line + comma_positions[index_arrival] + 1);

        if (processCount < maxProcesses) {
            processTable[processCount] = process;
            processCount++;
            if (process.processID >= nextPID) {
                nextPID = process.processID + 1;
            }
        }
    }
    fclose(fp);
}

void updateProcessInFile(struct PCB process) {
    FILE *tempfp = fopen("processesTemp.txt", "w");
    FILE *fp = fopen("processes.txt", "r");
    
    if (fp == NULL || tempfp == NULL) {
        printf("Error: Cannot access process file!\n");
        if (tempfp) fclose(tempfp);
        if (fp) fclose(fp);
        return;
    }
    char line[300];
    while (fgets(line, sizeof(line), fp)) {
        int pid;
        if (sscanf(line, "%d,", &pid) == 1) {
            if (pid == process.processID) {
                fprintf(tempfp, "%d,%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d\n",
                    process.processID, process.name, process.description, (int)process.priority,
                    stateToString(process.state), process.burst_time, process.remaining_time,
                    process.waiting_time, process.turnaround_time,
                    process.memory_required, process.memory_partition_id,
                    process.arrival_time);
            } else {
                fprintf(tempfp, "%s", line);
            }
        }
    }
    fclose(fp);
    fclose(tempfp);
    
    remove("processes.txt");
    rename("processesTemp.txt", "processes.txt");
}
void deleteProcessFromFile(int processID) {
    FILE *tempfp = fopen("processesTemp.txt", "w");
    FILE *fp = fopen("processes.txt", "r");
    
    if (fp == NULL) {
        printf("No processes file found.\n");
        if (tempfp) fclose(tempfp);
        return;
    }
    if (tempfp == NULL) {
        printf("Error: Cannot write to temporary file!\n");
        fclose(fp);
        return;
    }
    char line[300];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        int pid;
        if (sscanf(line, "%d,", &pid) == 1) {
            if (pid == processID) {
                found = 1;
            } else {
                fprintf(tempfp, "%s", line);
            }
        }
    }
    fclose(fp);
    fclose(tempfp);
    
    if (found) {
        remove("processes.txt");
        rename("processesTemp.txt", "processes.txt");
        printf("Process %d deleted from storage.\n", processID);
    } else {
        remove("processesTemp.txt");
        printf("Process %d not found in storage.\n", processID);
    }
}
void deleteAllProcessesFromFile() {
    if (remove("processes.txt") == 0) {
        printf("All processes deleted from storage.\n");
    } else {
        printf("No process storage file found to delete.\n");
    }
}

void deleteAllProcesses() {
    processCount = 0;
    nextPID = 1;
    deleteAllProcessesFromFile();
    free_system_memory();
}
void persist_all_processes() {
    FILE *fp = fopen("processes.txt", "w");
    if (fp == NULL) {
        printf("Error: Cannot open processes.txt for writing!\n");
        return;
    }
    for (int i = 0; i < processCount; i++) {
        struct PCB *p = &processTable[i];
        fprintf(fp, "%d,%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d\n",
            p->processID, p->name, p->description, (int)p->priority,
            stateToString(p->state), p->burst_time, p->remaining_time,
            p->waiting_time, p->turnaround_time,
            p->memory_required, p->memory_partition_id,
            p->arrival_time);
    }
    fclose(fp);
}
void create_process_record(char name[], char description[], enum Priority priority, int burst_time, int memory_required) {
    if (processCount >= maxProcesses) {
        printf("Cannot create more processes.\n");
        return;
    }
    struct PCB new_process;
    new_process.processID = nextPID++;
    strcpy(new_process.name, name);
    strcpy(new_process.description, description);
    new_process.priority = priority;
    new_process.burst_time = burst_time;
    new_process.remaining_time = burst_time;
    new_process.waiting_time = 0;
    new_process.arrival_time = processCount;
    new_process.turnaround_time = 0;
    new_process.state = NEW;
    new_process.memory_required = memory_required;
    new_process.memory_partition_id = 0;
    processTable[processCount] = new_process;
    processCount++;
    (new_process);
}
void create_process() {
    char name[20];
    char description[100];
    int priority_input;
    int burst;
    int memory_required;
    enum Priority priority;
    if (processCount >= maxProcesses) {
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
    printf("Enter memory required (MB): ");
    scanf("%d", &memory_required);
    create_process_record(name, description, priority, burst, memory_required);
    init_process_resources(processCount - 1);
    
    int assigned = allocate_memory_first_fit(nextPID - 1, memory_required);
    if (assigned > 0) {
        processTable[processCount - 1].memory_partition_id = assigned;
        updateProcessInFile(processTable[processCount - 1]);
    }

    printf("\nThe emergency process '%s' (PID=%d) has been successfully created and set to NEW state.\n\n", name, nextPID - 1);
    log_process_action("CREATED", nextPID - 1, name, description);
}
const char* priority_to_str(enum Priority p) {
    if (p == HIGH) return "High";
    if (p == MEDIUM) return "Medium";
    return "Low";
}
void list_processes() {
    loadProcessesFromFile();
    
    printf("\nProcess table\n");
    printf("PID  Arrival  Name                Priority  State     Burst  Mem(MB) Part  Description\n");
    for (int i = 0; i < processCount; i++) {
        printf("%-4d %-8d %-20s %-8s %-10s %-5d %-7d %-5d %s\n",
               processTable[i].processID,
               processTable[i].arrival_time,
               processTable[i].name,
               priority_to_str(processTable[i].priority),
               stateToString(processTable[i].state),
               processTable[i].burst_time,
               processTable[i].memory_required,
               processTable[i].memory_partition_id,
               processTable[i].description);
    }
}
void suspend_process(int processID) {
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].processID == processID) {
            if (processTable[i].state == RUNNING || processTable[i].state == READY) {
                processTable[i].state = WAITING;
                updateProcessInFile(processTable[i]);
                printf("Process %d suspended (now WAITING)\n", processID);
            } else {
                printf("Cannot suspend process %d in current state\n", processID);
            }
            return;
        }
    }
    printf("Process %d not found!\n", processID);
}
void resume_process(int processID) {
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].processID == processID) {
            if (processTable[i].state == WAITING) {
                processTable[i].state = READY;
                updateProcessInFile(processTable[i]);
                printf("Process %d resumed (now READY)\n", processID);
            } else {
                printf("Cannot resume process %d in current state\n", processID);
            }
            return;
        }
    }
    printf("Process %d not found!\n", processID);
}
void release_process_resources(int process_index) {
    for (int j = 0; j < numResources; j++) {
        available[j] += allocation[process_index][j];
        allocation[process_index][j] = 0;
        need[process_index][j] = max_need[process_index][j];
    }

    if (processTable[process_index].memory_partition_id > 0) {
        free_memory_by_pid(processTable[process_index].processID);
        processTable[process_index].memory_partition_id = 0;
    }
}
void terminate_process(int processID) {
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].processID == processID) {
            if (processTable[i].state == TERMINATED) {
                printf("Process %d is already terminated.\n", processID);
                return;
            }
            processTable[i].state = TERMINATED;
            release_process_resources(i);
            updateProcessInFile(processTable[i]);
            printf("Process %d terminated and its resources were freed.\n", processID);
            return;
        }
    }
    printf("Process %d not found!\n", processID);
}
void delete_process(int processID) {
    deleteProcessFromFile(processID);
    
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].processID == processID) {
            release_process_resources(i);
            for (int j = i; j < processCount - 1; j++) {
                processTable[j] = processTable[j + 1];
            }
            processCount--;
            return;
        }
    }
}
void reset_to_ready() {
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].state != TERMINATED &&
            processTable[i].state != WAITING) {
            processTable[i].state = READY;
            updateProcessInFile(processTable[i]);
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

    for (int i = 0; i < processCount; i++) {
        if (processTable[i].state == TERMINATED ||
            processTable[i].state == WAITING) {
            continue;
        }

        
        if (current_time < processTable[i].arrival_time) {
            current_time = processTable[i].arrival_time;
        }

        processTable[i].waiting_time = current_time - 
                                        processTable[i].arrival_time;
        processTable[i].state = RUNNING;
        updateProcessInFile(processTable[i]);

        printf("Running: %s (PID=%d) | Arrival=%d | Waiting=%d\n",
               processTable[i].name, processTable[i].processID,
               processTable[i].arrival_time,
               processTable[i].waiting_time);

        current_time += processTable[i].burst_time;
        processTable[i].turnaround_time = current_time - 
                                           processTable[i].arrival_time;

        total_waiting += processTable[i].waiting_time;
        total_turnaround += processTable[i].turnaround_time;
        completed++;

        processTable[i].state = TERMINATED;
        release_process_resources(i);
        updateProcessInFile(processTable[i]);
    }

    if (completed == 0) {
        printf("No processes to schedule.\n");
        return;
    }

    
    int total_burst = 0;
    for (int i = 0; i < processCount; i++) 
        total_burst += processTable[i].burst_time;

    float cpu_util = (current_time > 0) ? 
        ((float)total_burst / current_time) * 100.0f : 0;
    float avg_waiting = (float)total_waiting / completed;
    float avg_turnaround = (float)total_turnaround / completed;

    printf("\nResults:\n");
    printf("Total processes    : %d\n", completed);
    printf("Avg Waiting Time   : %.2f\n", avg_waiting);
    printf("Avg Turnaround Time: %.2f\n", avg_turnaround);
    printf("CPU Utilization    : %.2f%%\n", cpu_util);
    log_scheduling("FCFS", avg_waiting, avg_turnaround);
}
void schedule_priority() {
    printf("\nPriority scheduling\n");
    struct PCB temp[maxProcesses];
    int temp_count = 0;
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].state != TERMINATED &&
            processTable[i].state != WAITING) {
            temp[temp_count] = processTable[i];
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
        int pid = temp[i].processID;
        int index = -1;
        for (int k = 0; k < processCount; k++) {
            if (processTable[k].processID == pid) {
                index = k;
                break;
            }
        }
        if (index != -1) {
            processTable[index].waiting_time = temp[i].waiting_time;
            processTable[index].turnaround_time = temp[i].turnaround_time;
            processTable[index].state = RUNNING;
            updateProcessInFile(processTable[index]);
        }
        printf("Running: %s (PID=%d, Priority=%s) | Waiting=%d | Turnaround=%d\n",
               temp[i].name, temp[i].processID, priority_to_str(temp[i].priority),
               temp[i].waiting_time, temp[i].turnaround_time);
        printf("  [Simulating %d time unit(s) of execution...]\n", temp[i].burst_time);
        current_time += temp[i].burst_time;
        total_waiting += temp[i].waiting_time;
        total_turnaround += temp[i].turnaround_time;
        if (index != -1) {
            processTable[index].state = TERMINATED;
            release_process_resources(index);
            updateProcessInFile(processTable[index]);
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
    const char *resource_names[numResources] = {"ambulances", "fire trucks", "police units"};
    int max_req[numResources];
    for (int j = 0; j < numResources; j++) {
        while (1) {
            printf("Enter number of %s needed for %s: ",
                   resource_names[j], processTable[process_index].name);
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
    for (int j = 0; j < numResources; j++) {
        max_need[process_index][j] = max_req[j];
        allocation[process_index][j] = 0;
        need[process_index][j] = max_req[j];
    }
}
int is_safe_state(int num_proc) {
    int work[numResources];
    int finish[maxProcesses] = {0};
    int safe_sequence[maxProcesses];
    int count = 0;
    for (int i = 0; i < numResources; i++) {
        work[i] = available[i];
    }
    while (count < num_proc) {
        int found = 0;
        for (int p = 0; p < num_proc; p++) {
            if (!finish[p]) {
                int can_alloc = 1;
                for (int r = 0; r < numResources; r++) {
                    if (need[p][r] > work[r]) {
                        can_alloc = 0;
                        break;
                    }
                }
                if (can_alloc) {
                    for (int r = 0; r < numResources; r++) {
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
void request_resources(int processID, int req[]) {
    int process_index = -1;
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].processID == processID) {
            process_index = i;
            break;
        }
    }
    if (process_index == -1) {
        printf("Process not found!\n");
        return;
    }
    for (int i = 0; i < numResources; i++) {
        if (req[i] > need[process_index][i]) {
            printf("Request exceeds maximum claim!\n");
            return;
        }
        if (req[i] > available[i]) {
            printf("Resources not available. Process must wait.\n");
            return;
        }
    }
    for (int i = 0; i < numResources; i++) {
        available[i] -= req[i];
        allocation[process_index][i] += req[i];
        need[process_index][i] -= req[i];
    }
    if (is_safe_state(processCount)) {
        printf("Resources allocated successfully to PID %d\n", processID);
    } else {
        for (int i = 0; i < numResources; i++) {
            available[i] += req[i];
            allocation[process_index][i] -= req[i];
            need[process_index][i] += req[i];
        }
        printf("Allocation denied to prevent deadlock!\n");
    }
}
void show_resources() {
    printf("\nResource allocation\n");
    printf("Available Resources: R0(Ambulances)=%d  R1(Fire Units)=%d  R2(Police Units)=%d\n\n",
           available[0], available[1], available[2]);
    printf("PID  Process          Alloc(R0,R1,R2)  Need(R0,R1,R2)\n");
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].state != TERMINATED) {
            printf("%-4d %-16s (%d,%d,%d)          (%d,%d,%d)\n",
                   processTable[i].processID,
                   processTable[i].name,
                   allocation[i][0], allocation[i][1], allocation[i][2],
                   need[i][0], need[i][1], need[i][2]);
        }
    }
    show_memory_map();
}


void init_system_memory() {
    int sizes[NUM_PARTITIONS] = {150, 150, 250, 250, 300, 300};
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        memory_map[i].block_id = i + 1;
        memory_map[i].total_size = sizes[i];
        memory_map[i].free_space = sizes[i];
        memory_map[i].is_occupied = 0;
        memory_map[i].assigned_pid = 0;
    }
}

void free_system_memory() {
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        memory_map[i].is_occupied = 0;
        memory_map[i].assigned_pid = 0;
        memory_map[i].free_space = memory_map[i].total_size;
    }
}

int allocate_memory_first_fit(int pid, int size_required) {
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        if (!memory_map[i].is_occupied && memory_map[i].total_size >= size_required) {
            memory_map[i].is_occupied = 1;
            memory_map[i].assigned_pid = pid;
            memory_map[i].free_space = memory_map[i].total_size - size_required;
            printf("Allocated %d MB to PID %d in Partition %d (Internal fragmentation %d MB)\n",
                   size_required, pid, memory_map[i].block_id, memory_map[i].free_space);
            return memory_map[i].block_id;
        }
    }
    printf("Memory allocation failed for PID %d - no suitable partition (First-Fit).\n", pid);
    return -1;
}

void free_memory_by_pid(int pid) {
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        if (memory_map[i].assigned_pid == pid) {
            memory_map[i].is_occupied = 0;
            memory_map[i].assigned_pid = 0;
            memory_map[i].free_space = memory_map[i].total_size;
            printf("Freed partition %d from PID %d\n", memory_map[i].block_id, pid);
            return;
        }
    }
}

void show_memory_map() {
    printf("\nMemory partitions (MB):\n");
    printf("Part  Size  Free  Occupied PID\n");
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        printf("%4d  %4d  %4d    %s    %d\n",
               memory_map[i].block_id,
               memory_map[i].total_size,
               memory_map[i].free_space,
               memory_map[i].is_occupied ? "Yes" : "No ",
               memory_map[i].assigned_pid);
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
void log_process_action(char *action, int processID, char *name, char *description) {
    FILE *fp = fopen("serc_log.txt", "a");
    if (fp == NULL) {
        printf("Cannot write to log file!\n");
        return;
    }
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [PROCESS] %s | PID=%d | Name=%s | Description=%s\n",
            ts, action, processID, name, description);
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
void log_deadlock_event(char *event, int processID) {
    FILE *fp = fopen("serc_log.txt", "a");
    if (fp == NULL) return;
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [DEADLOCK] %s | PID=%d\n", ts, event, processID);
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
    int processID;
    
    if (choice == 1) {
        create_process();
    } else if (choice == 2) {
        list_processes();
    } else if (choice == 3) {
        int sub_choice;
        printf("Choose action for process: 1=Suspend, 2=Resume: ");
        scanf("%d", &sub_choice);
        printf("Enter PID: ");
        scanf("%d", &processID);
        char desc[100] = "";
        for (int i = 0; i < processCount; i++) {
            if (processTable[i].processID == processID) {
                strcpy(desc, processTable[i].description);
                break;
            }
        }
        if (sub_choice == 1) {
            suspend_process(processID);
            log_process_action("SUSPENDED", processID, "", desc);
        } else if (sub_choice == 2) {
            resume_process(processID);
            log_process_action("RESUMED", processID, "", desc);
        } else {
            printf("Invalid action selected.\n");
        }
    } else if (choice == 4) {
        printf("Enter PID to terminate: ");
        scanf("%d", &processID);
        char desc[100] = "";
        for (int i = 0; i < processCount; i++) {
            if (processTable[i].processID == processID) {
                strcpy(desc, processTable[i].description);
                break;
            }
        }
        terminate_process(processID);
        log_process_action("TERMINATED", processID, "", desc);
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
        scanf("%d", &processID);
        printf("Enter request for (Ambulances, Fire Units, Police Units): ");
        scanf("%d %d %d", &req[0], &req[1], &req[2]);
        request_resources(processID, req);
        log_deadlock_event("REQUEST", processID);
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
            scanf("%d", &processID);
            delete_process(processID);
        } else if (sub_choice == 2) {
            char confirm;
            printf("Are you sure you want to delete all processes? (y/n): ");
            scanf(" %c", &confirm);
            if (confirm == 'y' || confirm == 'Y') {
                deleteAllProcesses();
            } else {
                printf("Delete all cancelled.\n");
            }
        } else {
            printf("Invalid delete option.\n");
        }
    } else if (choice == 11) {
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
    loadProcessesFromFile();
    init_system_memory();
    
    for (int i = 0; i < processCount; i++) {
        int pid = processTable[i].processID;
        int part = processTable[i].memory_partition_id;
        if (part > 0 && part <= NUM_PARTITIONS) {
            memory_map[part - 1].is_occupied = 1;
            memory_map[part - 1].assigned_pid = pid;
            memory_map[part - 1].free_space = memory_map[part - 1].total_size - processTable[i].memory_required;
        }
    }
    while (1) {
        printf("\nMain menu\n");
        printf("1.  Create new process\n");
        printf("2.  List all processes\n");
        printf("3.  Suspend/Resume process\n");
        printf("4.  Terminate process\n");
        printf("5.  execute processes\n");
        printf("6.  Show resource allocation\n");
        printf("7.  Request resources\n");
        printf("8.  View system log\n");
        printf("9.  Clear log\n");
        printf("10. Delete process/all processes\n");
        printf("11. exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        if (!handle_menu_choice(choice)) {
            break;
        }
    }
    return 0;
}


