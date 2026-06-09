#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* --- Configurations and Constants --- */
#define INITIAL_CAPACITY 10
#define NUM_RESOURCES 3
#define NUM_PARTITIONS 6
#define TOTAL_MEMORY 1400
#define LOG_FILE "serc_log.txt"

/* Enum for different process states */
enum State { NEW, READY, RUNNING, WAITING, TERMINATED };
/* Enum for types of emergencies */
enum EmergencyType { FIRE, AMBULANCE, POLICE };

/* Process Control Block (PCB) structure */
typedef struct {
    int pid;
    char type_str[20];
    enum EmergencyType type;
    int priority;          /* 1 = High (Fire), 2 = Med (Ambulance), 3 = Low (Police) */
    int memory_required;
    int allocated_partition_idx; /* Stores which RAM block this process is using */
    enum State state;
    int burst_time;
    int arrival_time;
    int start_time;
    int completion_time;
    int waiting_time;
    int turnaround_time;
    int resource_reserved; 
    char description[100]; /* Location or description details */
} PCB;

/* Structure to simulate physical RAM slots */
typedef struct {
    int block_id;
    int total_size;
    int free_space;
    int is_occupied;
    int assigned_pid;
} MemoryPartition;

/* Global arrays and counters */
PCB *process_table = NULL;
MemoryPartition memory_map[NUM_PARTITIONS];
int process_count = 0;
int process_capacity = 0;
int system_clock = 0;
int total_burst_time = 0;

/* Banker's Algorithm resources: [0]=Fire Trucks, [1]=Ambulances, [2]=Police Cars */
int available_resources[NUM_RESOURCES] = {1, 2, 2}; 
int **max_need = NULL;
int **allocation = NULL;
int **need = NULL;

/* Helper functions to print names instead of enum numbers */
const char* state_to_string(enum State state) {
    switch (state) {
        case NEW: return "NEW"; case READY: return "READY"; case RUNNING: return "RUNNING";
        case WAITING: return "WAITING"; case TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}
const char* type_to_string(enum EmergencyType type) {
    switch (type) { case FIRE: return "FIRE"; case AMBULANCE: return "AMBULANCE"; case POLICE: return "POLICE"; }
    return "UNKNOWN";
}

/* ========================================================================== */
/* MEMORY MANAGEMENT (FIRST-FIT ALGORITHM)                                   */
/* ========================================================================== */

/* Allocate initial space for process arrays and set up RAM blocks */
void init_system_memory() {
    process_capacity = INITIAL_CAPACITY;
    process_table = (PCB *)malloc(process_capacity * sizeof(PCB));
    max_need = (int **)malloc(process_capacity * sizeof(int *));
    allocation = (int **)malloc(process_capacity * sizeof(int *));
    need = (int **)malloc(process_capacity * sizeof(int *));

    for (int i = 0; i < process_capacity; i++) {
        max_need[i] = (int *)calloc(NUM_RESOURCES, sizeof(int));
        allocation[i] = (int *)calloc(NUM_RESOURCES, sizeof(int));
        need[i] = (int *)calloc(NUM_RESOURCES, sizeof(int));
    }

    /* Set up fixed memory partitions (2x150MB, 2x250MB, 2x300MB) */
    int sizes[NUM_PARTITIONS] = {150, 150, 250, 250, 300, 300};
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        memory_map[i].block_id = i + 1;
        memory_map[i].total_size = sizes[i];
        memory_map[i].free_space = sizes[i];
        memory_map[i].is_occupied = 0;
        memory_map[i].assigned_pid = 0;
    }
}

/* Clean up memory on exit to prevent memory leaks */
void free_system_memory() {
    if (process_table) free(process_table);
    if (max_need && allocation && need) {
        for (int i = 0; i < process_capacity; i++) {
            free(max_need[i]); free(allocation[i]); free(need[i]);
        }
        free(max_need); free(allocation); free(need);
    }
}

/* Double the size of arrays if we run out of process slots */
void expand_system_arrays() {
    int old_capacity = process_capacity;
    process_capacity *= 2;

    process_table = (PCB *)realloc(process_table, process_capacity * sizeof(PCB));
    max_need = (int **)realloc(max_need, process_capacity * sizeof(int *));
    allocation = (int **)realloc(allocation, process_capacity * sizeof(int *));
    need = (int **)realloc(need, process_capacity * sizeof(int *));

    for (int i = old_capacity; i < process_capacity; i++) {
        max_need[i] = (int *)calloc(NUM_RESOURCES, sizeof(int));
        allocation[i] = (int *)calloc(NUM_RESOURCES, sizeof(int));
        need[i] = (int *)calloc(NUM_RESOURCES, sizeof(int));
    }
}

/* Loop through partitions and find the first free block that fits the process */
int allocate_first_fit(int pid, int size_required) {
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        if (!memory_map[i].is_occupied && memory_map[i].total_size >= size_required) {
            memory_map[i].is_occupied = 1;
            memory_map[i].assigned_pid = pid;
            /* Calculate remaining space (Internal Fragmentation) */
            memory_map[i].free_space = memory_map[i].total_size - size_required; 
            return i; /* Return the partition index */
        }
    }
    return -1; /* No available partition fits this process */
}

/* ========================================================================== */
/* PROCESS MANAGEMENT                                                         */
/* ========================================================================== */

/* Create a new process and check if it can enter the system right away */
void create_emergency_process(enum EmergencyType type, const char *desc) {
    if (process_count >= process_capacity) {
        expand_system_arrays();
    }

    /* Initialize the process variables */
    PCB new_proc;
    new_proc.pid = process_count + 1;
    new_proc.type = type;
    strcpy(new_proc.type_str, type_to_string(type));
    strncpy(new_proc.description, desc, 99);
    new_proc.description[99] = '\0';
    
    /* Assign priorities and memory requirements based on emergency type */
    if (type == FIRE) {
        new_proc.priority = 1; new_proc.memory_required = 250;
        max_need[process_count][0] = 1; 
    } else if (type == AMBULANCE) {
        new_proc.priority = 2; new_proc.memory_required = 150;
        max_need[process_count][1] = 1;
    } else {
        new_proc.priority = 3; new_proc.memory_required = 100;
        max_need[process_count][2] = 1;
    }

    /* Random execution time between 2 and 6 seconds */
    new_proc.burst_time = (rand() % 5) + 2;
    new_proc.arrival_time = system_clock;
    new_proc.start_time = 0; new_proc.completion_time = 0;
    new_proc.waiting_time = 0; new_proc.turnaround_time = 0;
    new_proc.resource_reserved = 0;
    new_proc.state = NEW;

    printf("\n[SYSTEM] Asset Description: %s\n", new_proc.description);
    
    /* Try to allocate RAM using First-Fit */
    int partition_idx = allocate_first_fit(new_proc.pid, new_proc.memory_required);
    
    if (partition_idx != -1) {
        new_proc.allocated_partition_idx = partition_idx;
        int r_idx = (type == FIRE) ? 0 : (type == AMBULANCE) ? 1 : 2;
        
        /* Check if physical response vehicles are available */
        if (available_resources[r_idx] >= max_need[process_count][r_idx]) {
            available_resources[r_idx] -= max_need[process_count][r_idx];
            allocation[process_count][r_idx] += max_need[process_count][r_idx];
            new_proc.state = READY;
            new_proc.resource_reserved = 1;
            need[process_count][r_idx] = 0; 
            printf(" -> [FIRST-FIT] Assigned to Partition %d (%d MB Internal Fragmentation)\n", 
                   partition_idx + 1, memory_map[partition_idx].free_space);
        } else {
            /* Got RAM but no vehicle -> put in WAITING state */
            new_proc.state = WAITING;
            need[process_count][r_idx] = max_need[process_count][r_idx];
            printf(" -> [RESOURCE WAIT] Hardware missing. Placed in WAITING loop.\n");
        }
    } else {
        /* No RAM block available -> put in WAITING state */
        new_proc.state = WAITING;
        new_proc.allocated_partition_idx = -1;
        int r_idx = (type == FIRE) ? 0 : (type == AMBULANCE) ? 1 : 2;
        need[process_count][r_idx] = max_need[process_count][r_idx];
        printf(" -> [MEMORY WAIT] No free RAM partitions. Placed in WAITING loop.\n");
    }

    process_table[process_count] = new_proc;
    process_count++;
}

/* ========================================================================== */
/* CPU SCHEDULING MOTHERBOARD ENGINE                                          */
/* ========================================================================== */

/* Shared code that actually runs the READY processes after they have been sorted */
void execute_pipeline() {
    /* Step 1: Scan for WAITING processes and see if we can wake them up now */
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].state == WAITING) {
            int r_idx = (process_table[i].type == FIRE) ? 0 : (process_table[i].type == AMBULANCE) ? 1 : 2;
            
            /* If it was waiting for RAM, try to find a block now */
            if (process_table[i].allocated_partition_idx == -1) {
                process_table[i].allocated_partition_idx = allocate_first_fit(process_table[i].pid, process_table[i].memory_required);
            }
            
            /* If it has RAM and a vehicle is ready, unblock it */
            if (process_table[i].allocated_partition_idx != -1 && available_resources[r_idx] >= need[i][r_idx]) {
                available_resources[r_idx] -= need[i][r_idx];
                allocation[i][r_idx] += need[i][r_idx];
                need[i][r_idx] = 0;
                process_table[i].state = READY;
                process_table[i].resource_reserved = 1;
                printf(" -> [WAKEUP] PID %d secured resources and is now READY.\n", process_table[i].pid);
            }
        }
    }

    /* Step 2: Loop through and execute all processes that are READY */
    int executed_any = 0;
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].state == READY) {
            PCB *p = &process_table[i];
            p->state = RUNNING;
            p->start_time = system_clock;
            p->waiting_time = p->start_time - p->arrival_time;
            if (p->waiting_time < 0) p->waiting_time = 0;

            printf("\n============================================\n");
            printf(" Running Process ID : %d\n", p->pid);
            printf(" Emergency Target   : %s\n", p->type_str);
            printf(" Detail Context     : %s\n", p->description);
            printf(" Burst Duration     : %d seconds\n", p->burst_time);
            printf("============================================\n");

            /* Simulate execution using sleep() */
            sleep(p->burst_time);
            system_clock += p->burst_time;
            total_burst_time += p->burst_time;

            p->completion_time = system_clock;
            p->turnaround_time = p->completion_time - p->arrival_time;
            p->state = TERMINATED;

            /* Release the memory partition back to the system */
            int part_idx = p->allocated_partition_idx;
            if (part_idx != -1) {
                memory_map[part_idx].is_occupied = 0;
                memory_map[part_idx].assigned_pid = 0;
                memory_map[part_idx].free_space = memory_map[part_idx].total_size;
            }

            /* Release vehicles back to the resource pool */
            if (p->resource_reserved) {
                for (int r = 0; r < NUM_RESOURCES; r++) {
                    available_resources[r] += allocation[i][r];
                    allocation[i][r] = 0;
                }
                p->resource_reserved = 0;
            }
            executed_any = 1;
        }
    }
    if (!executed_any) printf("\n[INFO] No tasks in READY state to execute.\n");
}

/* FCFS Scheduler: Sorts processes strictly by arrival time */
void run_fcfs_scheduler() {
    printf("\n[SCHEDULER] Running First-Come, First-Served Scheduler...\n");
    sleep(1);
    
    /* Bubble sort based on arrival time stamps */
    for (int i = 0; i < process_count - 1; i++) {
        for (int j = i + 1; j < process_count; j++) {
            if (process_table[j].arrival_time < process_table[i].arrival_time) {
                /* Swap the PCBs */
                PCB temp = process_table[i]; process_table[i] = process_table[j]; process_table[j] = temp;
                /* Swap the corresponding rows in Banker's matrices */
                int *t_max = max_need[i]; max_need[i] = max_need[j]; max_need[j] = t_max;
                int *t_alloc = allocation[i]; allocation[i] = allocation[j]; allocation[j] = t_alloc;
                int *t_need = need[i]; need[i] = need[j]; need[j] = t_need;
            }
        }
    }
    execute_pipeline();
}

/* Priority Scheduler: Sorts processes by priority level (Fire > Ambulance > Police) */
void run_priority_scheduler() {
    printf("\n[SCHEDULER] Running Priority Level Scheduler...\n");
    sleep(1);
    
    /* Bubble sort by priority, using arrival time to break ties */
    for (int i = 0; i < process_count - 1; i++) {
        for (int j = i + 1; j < process_count; j++) {
            if ((process_table[j].priority < process_table[i].priority) || 
                (process_table[j].priority == process_table[i].priority && 
                 process_table[j].arrival_time < process_table[i].arrival_time)) {
                
                PCB temp = process_table[i]; process_table[i] = process_table[j]; process_table[j] = temp;
                int *t_max = max_need[i]; max_need[i] = max_need[j]; max_need[j] = t_max;
                int *t_alloc = allocation[i]; allocation[i] = allocation[j]; allocation[j] = t_alloc;
                int *t_need = need[i]; need[i] = need[j]; need[j] = t_need;
            }
        }
    }
    execute_pipeline();
}

/* ========================================================================== */
/* DEADLOCK HANDLING (BANKER'S ALGORITHM)                                     */
/* ========================================================================== */

/* Simulates whether allocating resources keeps the system in a safe state */
int calculate_safety_vector() {
    int work[NUM_RESOURCES];
    int *finish = (int *)calloc(process_count, sizeof(int));
    
    for (int i = 0; i < NUM_RESOURCES; i++) work[i] = available_resources[i];
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].state == TERMINATED) finish[i] = 1;
    }

    int evaluation_flag = 1;
    while (evaluation_flag) {
        evaluation_flag = 0;
        for (int p = 0; p < process_count; p++) {
            if (!finish[p]) {
                int resource_match = 1;
                for (int r = 0; r < NUM_RESOURCES; r++) {
                    if (need[p][r] > work[r]) { resource_match = 0; break; }
                }
                if (resource_match) {
                    for (int r = 0; r < NUM_RESOURCES; r++) work[r] += allocation[p][r];
                    finish[p] = 1;
                    evaluation_flag = 1;
                }
            }
        }
    }

    /* If any process couldn't finish, the state is unsafe */
    for (int i = 0; i < process_count; i++) {
        if (!finish[i]) { free(finish); return 0; }
    }
    free(finish);
    return 1; 
}

/* Processes an individual manual resource request from Option 7 */
void manual_request_resources(int process_id, int req[]) {
    int idx = -1;
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].pid == process_id && process_table[i].state != TERMINATED) {
            idx = i; break;
        }
    }

    if (idx == -1) { printf("[ERROR] PID %d not found or already completed.\n", process_id); return; }

    for (int r = 0; r < NUM_RESOURCES; r++) {
        if (req[r] > need[idx][r] || req[r] > available_resources[r]) {
            printf("[DENIED] Request exceeds process need or system limits.\n"); return;
        }
    }

    /* Test allocation (Speculative updates) */
    for (int r = 0; r < NUM_RESOURCES; r++) {
        available_resources[r] -= req[r]; allocation[idx][r] += req[r]; need[idx][r] -= req[r];
    }

    /* Check if the system remains safe */
    if (calculate_safety_vector()) {
        printf("[SUCCESS] State is safe. Resources allocated to PID %d.\n", process_id);
    } else {
        /* Roll back changes if it would cause a deadlock loop */
        for (int r = 0; r < NUM_RESOURCES; r++) {
            available_resources[r] += req[r]; allocation[idx][r] -= req[r]; need[idx][r] += req[r];
        }
        printf("[DENIED] Allocation rolled back to prevent an unsafe state.\n");
    }
}

/* ========================================================================== */
/* FILE MANAGEMENT & SYSTEM VISUALIZATION                                     */
/* ========================================================================== */

/* Append operational actions out into a text file logs */
void write_log(const char *message) {
    FILE *file = fopen(LOG_FILE, "a");
    if (file == NULL) return;
    time_t now = time(NULL);
    char ts[26];
    struct tm *tm_info = localtime(&now);
    strftime(ts, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(file, "[%s] %s\n", ts, message);
    fclose(file);
}

/* Display full information about current system processes */
void display_process_table() {
    printf("\n==================================================================================================\n");
    printf("PID   TYPE          PRIORITY   MEMORY     STATE        DESCRIPTION\n");
    printf("==================================================================================================\n");
    for (int i = 0; i < process_count; i++) {
        printf("%-5d %-13s %-10d %-10d %-12s %-s\n",
               process_table[i].pid, process_table[i].type_str, process_table[i].priority,
               process_table[i].memory_required, state_to_string(process_table[i].state),
               process_table[i].description);
    }
    printf("==================================================================================================\n");
}

/* Shows vehicle counts and prints the current status of each RAM partition block */
void display_system_status() {
    printf("\n===================== SYSTEM HARDWARE VECTOR MATRIX =====================\n");
    printf(" Fire Trucks on Standby        : %d\n", available_resources[0]);
    printf(" Ambulances on Standby         : %d\n", available_resources[1]);
    printf(" Police Vehicles on Standby    : %d\n", available_resources[2]);
    
    printf("\n======================= PHYSICAL MEMORY FRAME MAP =======================\n");
    printf("BLOCK  TOTAL SIZE   FREE SPACE   STATUS       ASSIGNED PID\n");
    printf("-------------------------------------------------------------------------\n");
    for (int i = 0; i < NUM_PARTITIONS; i++) {
        printf("%-6d %-12d %-12d %-12s %-d\n",
               memory_map[i].block_id, memory_map[i].total_size, memory_map[i].free_space,
               memory_map[i].is_occupied ? "OCCUPIED" : "FREE", memory_map[i].assigned_pid);
    }
    printf("=========================================================================\n");
}

/* Outputs execution time tracking parameters (WT, TAT, and CPU utilization) */
void display_metrics() {
    double utilization = (system_clock > 0) ? ((double)total_burst_time / system_clock) * 100.0 : 0.0;
    printf("\n========== PERFORMANCE ENGINE METRICS ==========\n");
    for (int i = 0; i < process_count; i++) {
        printf(" Task ID Vector PID %-3d | WT=%-3d | TAT=%-3d\n", 
               process_table[i].pid, process_table[i].waiting_time, process_table[i].turnaround_time);
    }
    printf("\n Total System Processing Cycles : %d\n", system_clock);
    printf(" Core CPU Utilization Factor   : %.2f%%\n", utilization);
}

/* ========================================================================== */
/* INTERACTIVE CONSOLE INTERFACE                                              */
/* ========================================================================== */
int main() {
    int choice, sched_choice, pid, req[NUM_RESOURCES];
    char desc_input[100];
    srand((unsigned int)time(NULL));
    init_system_memory();
    write_log("System initialized: Core kernel operational.");

    while (1) {
        printf("\n====================================================\n");
        printf("     SMART EMERGENCY RESPONSE CENTER (SERC OS v4.0) \n");
        printf("====================================================\n");
        printf("  1. Report FIRE Incident         6. View System Hardware & RAM Status\n");
        printf("  2. Report AMBULANCE Incident    7. Manual Request Resource (Banker)\n");
        printf("  3. Report POLICE Incident       8. View Core Performance Metrics\n");
        printf("  4. View Process Table           9. Exit SERC Console\n");
        printf("  5. Run CPU Task Scheduler Submenu\n");
        printf("====================================================\n");
        printf("Enter selection: ");

        /* Handle non-numeric typing bugs */
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("[INVALID] Non-numeric input skipped.\n");
            continue;
        }

        /* Flush input buffer line breaks */
        int c;
        while ((c = getchar()) != '\n' && c != EOF);

        switch (choice) {
            case 1:
                printf("Enter Fire Incident Details/Location: ");
                fgets(desc_input, sizeof(desc_input), stdin); desc_input[strcspn(desc_input, "\n")] = 0; 
                create_emergency_process(FIRE, desc_input); write_log("Fire Incident Logged"); 
                break;
            case 2:
                printf("Enter Ambulance Incident Details/Location: ");
                fgets(desc_input, sizeof(desc_input), stdin); desc_input[strcspn(desc_input, "\n")] = 0; 
                create_emergency_process(AMBULANCE, desc_input); write_log("Ambulance Incident Logged"); 
                break;
            case 3:
                printf("Enter Police Incident Details/Location: ");
                fgets(desc_input, sizeof(desc_input), stdin); desc_input[strcspn(desc_input, "\n")] = 0; 
                create_emergency_process(POLICE, desc_input); write_log("Police Incident Logged"); 
                break;
            case 4: display_process_table(); break;
            case 5:
                printf("\n--- Select CPU Scheduling Strategy ---\n");
                printf(" 1. First-Come, First-Served (FCFS)\n");
                printf(" 2. Emergency Precedence Priority Scheduling\n");
                printf("Selection: ");
                scanf("%d", &sched_choice);
                if (sched_choice == 1) run_fcfs_scheduler();
                else if (sched_choice == 2) run_priority_scheduler();
                else printf("Invalid option chosen.\n");
                break;
            case 6: display_system_status(); break;
            case 7:
                printf("Enter Target PID: "); scanf("%d", &pid);
                printf("Enter vector request [Trucks Ambulances Police]: ");
                scanf("%d %d %d", &req[0], &req[1], &req[2]);
                manual_request_resources(pid, req);
                break;
            case 8: display_metrics(); break;
            case 9:
                write_log("System Shutdown initiated.");
                printf("\nExiting and flushing allocations cleanly...\n");
                free_system_memory();
                return 0;
            default: printf("Invalid choice. Try again.\n");
        }
    }
    return 0;
}