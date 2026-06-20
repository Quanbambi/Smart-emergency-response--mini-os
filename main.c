#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
//Global declarations
#define maxProcesses 10
#define numResources 3
#define totalMemory 1400 
#define numberOfPartitions 6
enum State { NEW, READY, RUNNING, WAITING, TERMINATED };
enum Priority { HIGH = 1, MEDIUM = 2, LOW = 3};
// Process Control Block (PCB) structure
struct PCB {
    int processID;
    char name[20];
    char description[100];
    enum Priority priority;
    int burstTime;
    int remainingTime;
    int waitingTime;
    int turnaroundTime;
    enum State state;
    int memoryRequired;
    int memoryPartitionID;
    int arrivalTime;
};
// Some global variables to manage processes and resources
struct PCB processTable[maxProcesses];
int processCount = 0, nextPID = 1, available[numResources] = {5, 4, 10}, maxNeed[maxProcesses][numResources], allocation[maxProcesses][numResources], need[maxProcesses][numResources];
void initProcessResources(int processIndex);
void logProcessAction(char *action, int processID, char *name, char *description);
void logScheduling(char *algorithm, float averageWaitingTime, float averageTurnaround);
void initSystemMemory();
int allocateMemoryFirstFit(int pid, int sizeRequired);
void freeMemoryByPID(int pid);
void showMemoryMap();
void freeSystemMemory();
void releaseProcessResources(int processIndex);
int handleMenuChoice(int choice);

// memory partition structure and management
typedef struct MemoryPartition {
    int blockID;
    int totalSize; 
    int freeSpace; 
    int isOccupied; 
    int assignedPID;
} MemoryPartition;

MemoryPartition memoryMap[numberOfPartitions];


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
            stateToString(process.state), process.burstTime, process.remainingTime,
            process.waitingTime, process.turnaroundTime,
            process.memoryRequired, process.memoryPartitionID,
            process.arrivalTime);
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
        int commaPositions[32];
        int commaCount = 0;
        for (size_t i = 0; i < strlen(line); i++) {
            if (line[i] == ',') {
                if (commaCount < 32) {
                    commaPositions[commaCount] = (int)i;
                }
                commaCount++;
            }
        }
        if (commaCount < 11) {
            continue;
        }
        int indexPID = 0, indexName = 1, indexPriority = commaCount - 9, indexState = commaCount - 8, indexBurst = commaCount - 7, indexRemaining = commaCount - 6, indexWaiting = commaCount - 5, indexTurnaround = commaCount - 4, indexMemory = commaCount - 3, indexPartition = commaCount - 2, indexArrivalTime = commaCount - 1, pidLength = commaPositions[indexPID];
        char pidBuffer[16];
        //Copy Pid
        strncpy(pidBuffer, line, pidLength < 15 ? pidLength : 15);
        pidBuffer[pidLength < 15 ? pidLength : 15] = '\0';
        process.processID = atoi(pidBuffer);
        //copy process name
        int nameLength = commaPositions[indexName] - commaPositions[indexPID] - 1;
        strncpy(process.name, line + commaPositions[indexPID] + 1, nameLength < 19 ? nameLength : 19);
        process.name[nameLength < 19 ? nameLength : 19] = '\0';
        //copy process description
        int descriptionStart = commaPositions[indexName] + 1;
        int descriptionEnd = commaPositions[indexPriority];
        int descriptionLength = descriptionEnd - descriptionStart;
        strncpy(process.description, line + descriptionStart, descriptionLength < 99 ? descriptionLength : 99);
        process.description[descriptionLength < 99 ? descriptionLength : 99] = '\0';
        //copy process priority
        char priorityBuffer[8];
        int priorityLength = commaPositions[indexState] - commaPositions[indexPriority] - 1;
        strncpy(priorityBuffer, line + commaPositions[indexPriority] + 1, priorityLength < 7 ? priorityLength : 7);
        priorityBuffer[priorityLength < 7 ? priorityLength : 7] = '\0';
        process.priority = (enum Priority)atoi(priorityBuffer);
        //copy process state
        int stateLength = commaPositions[indexBurst] - commaPositions[indexState] - 1;
        strncpy(stateString, line + commaPositions[indexState] + 1, stateLength < 19 ? stateLength : 19);
        stateString[stateLength < 19 ? stateLength : 19] = '\0';
        process.state = stringToState(stateString);
        //copy remaining fields
        process.burstTime = atoi(line + commaPositions[indexBurst] + 1);
        process.remainingTime = atoi(line + commaPositions[indexRemaining] + 1);
        process.waitingTime = atoi(line + commaPositions[indexWaiting] + 1);
        process.turnaroundTime = atoi(line + commaPositions[indexTurnaround] + 1);
        process.memoryRequired = atoi(line + commaPositions[indexMemory] + 1);
        process.memoryPartitionID = atoi(line + commaPositions[indexPartition] + 1);
        process.arrivalTime = atoi(line + commaPositions[indexArrivalTime] + 1);

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
                    stateToString(process.state), process.burstTime, process.remainingTime,
                    process.waitingTime, process.turnaroundTime,
                    process.memoryRequired, process.memoryPartitionID,
                    process.arrivalTime);
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
    freeSystemMemory();
}
void persistAllProcesses() {
    FILE *fp = fopen("processes.txt", "w");
    if (fp == NULL) {
        printf("Error: Cannot open processes.txt for writing!\n");
        return;
    }
    for (int i = 0; i < processCount; i++) {
        struct PCB *p = &processTable[i];
        fprintf(fp, "%d,%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d\n",
            p->processID, p->name, p->description, (int)p->priority,
            stateToString(p->state), p->burstTime, p->remainingTime,
            p->waitingTime, p->turnaroundTime,
            p->memoryRequired, p->memoryPartitionID,
            p->arrivalTime);
    }
    fclose(fp);
}
void createProcessRecord(char name[], char description[], enum Priority priority, int burstTime, int memoryRequired) {
    if (processCount >= maxProcesses) {
        printf("Cannot create more processes.\n");
        return;
    }
    struct PCB newProcess;
    newProcess.processID = nextPID++;
    strcpy(newProcess.name, name);
    strcpy(newProcess.description, description);
    newProcess.priority = priority;
    newProcess.burstTime = burstTime;
    newProcess.remainingTime = burstTime;
    newProcess.waitingTime = 0;
    newProcess.arrivalTime = processCount;
    newProcess.turnaroundTime = 0;
    newProcess.state = NEW;
    newProcess.memoryRequired = memoryRequired;
    newProcess.memoryPartitionID = 0;
    processTable[processCount] = newProcess;
    processCount++;
    saveProcessToFile(newProcess);
}
void createProcess() {
    char name[20];
    char description[100];
    int priorityInput;
    int burst;
    int memoryRequired;
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
        if (scanf("%d", &priorityInput) != 1) {
            while (getchar() != '\n');
            priorityInput = 0;
        }
        if (priorityInput < 1 || priorityInput > 3) {
            printf("Invalid priority. Please enter 1, 2, or 3.\n");
        }
    } while (priorityInput < 1 || priorityInput > 3);
    priority = (enum Priority)priorityInput;
    printf("Enter burst time: ");
    scanf("%d", &burst);
    printf("Enter memory required (MB): ");
    scanf("%d", &memoryRequired);
    createProcessRecord(name, description, priority, burst, memoryRequired);
    initProcessResources(processCount - 1);
    
    int assigned = allocateMemoryFirstFit(nextPID - 1, memoryRequired);
    if (assigned > 0) {
        processTable[processCount - 1].memoryPartitionID = assigned;
        updateProcessInFile(processTable[processCount - 1]);
    }

    printf("\nThe emergency process '%s' (PID=%d) has been successfully created and set to NEW state.\n\n", name, nextPID - 1);
    logProcessAction("CREATED", nextPID - 1, name, description);
}
const char* priorityToStr(enum Priority p) {
    if (p == HIGH) return "High";
    if (p == MEDIUM) return "Medium";
    return "Low";
}
void listProcesses() {
    loadProcessesFromFile();
    
    printf("\nProcess table\n");
    printf("PID  Arrival  Name                Priority  State     Burst  Mem(MB) Part  Description\n");
    for (int i = 0; i < processCount; i++) {
        printf("%-4d %-8d %-20s %-8s %-10s %-5d %-7d %-5d %s\n",
               processTable[i].processID,
               processTable[i].arrivalTime,
               processTable[i].name,
               priorityToStr(processTable[i].priority),
               stateToString(processTable[i].state),
               processTable[i].burstTime,
               processTable[i].memoryRequired,
               processTable[i].memoryPartitionID,
               processTable[i].description);
    }
}
void suspendProcess(int processID) {
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
void resumeProcess(int processID) {
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
void releaseProcessResources(int processIndex) {
    for (int j = 0; j < numResources; j++) {
        available[j] += allocation[processIndex][j];
        allocation[processIndex][j] = 0;
        need[processIndex][j] = maxNeed[processIndex][j];
    }

    if (processTable[processIndex].memoryPartitionID > 0) {
        freeMemoryByPID(processTable[processIndex].processID);
        processTable[processIndex].memoryPartitionID = 0;
    }
}
void terminateProcess(int processID) {
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].processID == processID) {
            if (processTable[i].state == TERMINATED) {
                printf("Process %d is already terminated.\n", processID);
                return;
            }
            processTable[i].state = TERMINATED;
            releaseProcessResources(i);
            updateProcessInFile(processTable[i]);
            printf("Process %d terminated and its resources were freed.\n", processID);
            return;
        }
    }
    printf("Process %d not found!\n", processID);
}
void deleteProcess(int processID) {
    deleteProcessFromFile(processID);
    
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].processID == processID) {
            releaseProcessResources(i);
            for (int j = i; j < processCount - 1; j++) {
                processTable[j] = processTable[j + 1];
            }
            processCount--;
            return;
        }
    }
}
void resetToReady() {
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].state != TERMINATED &&
            processTable[i].state != WAITING) {
            processTable[i].state = READY;
            updateProcessInFile(processTable[i]);
        }
    }
}
void scheduleFCFS() {
    printf("\nFCFS scheduling\n");
    int currentTime = 0;
    int totalWaiting = 0;
    int totalTurnaround = 0;
    int completed = 0;

    resetToReady();

    for (int i = 0; i < processCount; i++) {
        if (processTable[i].state == TERMINATED ||
            processTable[i].state == WAITING) {
            continue;
        }

        
        if (currentTime < processTable[i].arrivalTime) {
            currentTime = processTable[i].arrivalTime;
        }

        processTable[i].waitingTime = currentTime - 
                                        processTable[i].arrivalTime;
        processTable[i].state = RUNNING;
        updateProcessInFile(processTable[i]);

        printf("Running: %s (PID=%d) | Arrival=%d | Waiting=%d\n",
               processTable[i].name, processTable[i].processID,
               processTable[i].arrivalTime,
               processTable[i].waitingTime);

        currentTime += processTable[i].burstTime;
        processTable[i].turnaroundTime = currentTime - 
                                           processTable[i].arrivalTime;

        totalWaiting += processTable[i].waitingTime;
        totalTurnaround += processTable[i].turnaroundTime;
        completed++;

        processTable[i].state = TERMINATED;
        releaseProcessResources(i);
        updateProcessInFile(processTable[i]);
    }

    if (completed == 0) {
        printf("No processes to schedule.\n");
        return;
    }

    
    int totalBurst = 0;
    for (int i = 0; i < processCount; i++) 
        totalBurst += processTable[i].burstTime;

    float cpuUtilities = (currentTime > 0) ? 
        ((float)totalBurst / currentTime) * 100.0f : 0;
    float averageWaitingTime = (float)totalWaiting / completed;
    float averageTurnaroundTime = (float)totalTurnaround / completed;

    printf("\nResults:\n");
    printf("Total processes    : %d\n", completed);
    printf("Avg Waiting Time   : %.2f\n", averageWaitingTime);
    printf("Avg Turnaround Time: %.2f\n", averageTurnaroundTime);
    printf("CPU Utilization    : %.2f%%\n", cpuUtilities);
    logScheduling("FCFS", averageWaitingTime, averageTurnaroundTime);
}
void schedulePriority() {
    printf("\nPriority scheduling\n");
    struct PCB temp[maxProcesses];
    int tempCount = 0;
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].state != TERMINATED &&
            processTable[i].state != WAITING) {
            temp[tempCount] = processTable[i];
            tempCount++;
        }
    }
    if (tempCount == 0) {
        printf("No processes to schedule.\n");
        return;
    }
    for (int i = 0; i < tempCount - 1; i++) {
        for (int j = 0; j < tempCount - i - 1; j++) {
            if (temp[j].priority > temp[j+1].priority) {
                struct PCB t = temp[j];
                temp[j] = temp[j+1];
                temp[j+1] = t;
            }
        }
    }
    int currentTime = 0;
    int totalWaiting = 0;
    int totalTurnaround = 0;
    for (int i = 0; i < tempCount; i++) {
        temp[i].waitingTime = currentTime;
        temp[i].turnaroundTime = currentTime + temp[i].burstTime;
        int pid = temp[i].processID;
        int index = -1;
        for (int k = 0; k < processCount; k++) {
            if (processTable[k].processID == pid) {
                index = k;
                break;
            }
        }
        if (index != -1) {
            processTable[index].waitingTime = temp[i].waitingTime;
            processTable[index].turnaroundTime = temp[i].turnaroundTime;
            processTable[index].state = RUNNING;
            updateProcessInFile(processTable[index]);
        }
        printf("Running: %s (PID=%d, Priority=%s) | Waiting=%d | Turnaround=%d\n",
               temp[i].name, temp[i].processID, priorityToStr(temp[i].priority),
               temp[i].waitingTime, temp[i].turnaroundTime);
        printf("  [Simulating %d time unit(s) of execution...]\n", temp[i].burstTime);
        currentTime += temp[i].burstTime;
        totalWaiting += temp[i].waitingTime;
        totalTurnaround += temp[i].turnaroundTime;
        if (index != -1) {
            processTable[index].state = TERMINATED;
            releaseProcessResources(index);
            updateProcessInFile(processTable[index]);
        }
    }
    printf("\nResults:\n");
    float averageWaitingTime = (float)totalWaiting / tempCount;
    float averageTurnaroundTime = (float)totalTurnaround / tempCount;
    int totalBurst = 0;
    for (int i = 0; i < tempCount; i++) totalBurst += temp[i].burstTime;
    float cpuUtilities = (totalBurst > 0) ? ((float)totalBurst / currentTime) * 100.0f : 0;
    printf("Average Waiting Time: %.2f\n", averageWaitingTime);
    printf("Average Turnaround Time: %.2f\n", averageTurnaroundTime);
    printf("CPU Utilization: %.2f%%\n", cpuUtilities);
    logScheduling("PRIORITY", averageWaitingTime, averageTurnaroundTime);
}
void initProcessResources(int processIndex) {
    const char *resourceNames[numResources] = {"ambulances", "fire trucks", "police units"};
    int maxReq[numResources];
    for (int j = 0; j < numResources; j++) {
        while (1) {
            printf("Enter number of %s needed for %s: ",
                   resourceNames[j], processTable[processIndex].name);
            if (scanf("%d", &maxReq[j]) != 1) {
                while (getchar() != '\n');
                printf("Invalid input. Enter a whole number.\n");
                continue;
            }
            if (maxReq[j] < 0) {
                printf("Resource count cannot be negative. Try again.\n");
                continue;
            }
            break;
        }
    }
    for (int j = 0; j < numResources; j++) {
        maxNeed[processIndex][j] = maxReq[j];
        allocation[processIndex][j] = 0;
        need[processIndex][j] = maxReq[j];
    }
}
int isSafeState(int numProcess) {
    int work[numResources];
    int finish[maxProcesses] = {0};
    int safeSequence[maxProcesses];
    int count = 0;
    for (int i = 0; i < numResources; i++) {
        work[i] = available[i];
    }
    while (count < numProcess) {
        int found = 0;
        for (int p = 0; p < numProcess; p++) {
            if (!finish[p]) {
                int canAlloc = 1;
                for (int r = 0; r < numResources; r++) {
                    if (need[p][r] > work[r]) {
                        canAlloc = 0;
                        break;
                    }
                }
                if (canAlloc) {
                    for (int r = 0; r < numResources; r++) {
                        work[r] += allocation[p][r];
                    }
                    safeSequence[count] = p;
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
        printf("P%d ", safeSequence[i]);
    }
    printf("\n");
    return 1;
}
void requestResources(int processID, int req[]) {
    int processIndex = -1;
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].processID == processID) {
            processIndex = i;
            break;
        }
    }
    if (processIndex == -1) {
        printf("Process not found!\n");
        return;
    }
    for (int i = 0; i < numResources; i++) {
        if (req[i] > need[processIndex][i]) {
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
        allocation[processIndex][i] += req[i];
        need[processIndex][i] -= req[i];
    }
    if (isSafeState(processCount)) {
        printf("Resources allocated successfully to PID %d\n", processID);
    } else {
        for (int i = 0; i < numResources; i++) {
            available[i] += req[i];
            allocation[processIndex][i] -= req[i];
            need[processIndex][i] += req[i];
        }
        printf("Allocation denied to prevent deadlock!\n");
    }
}
void showResources() {
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
    showMemoryMap();
}


void initSystemMemory() {
    int sizes[numberOfPartitions] = {150, 150, 250, 250, 300, 300};
    for (int i = 0; i < numberOfPartitions; i++) {
        memoryMap[i].blockID = i + 1;
        memoryMap[i].totalSize = sizes[i];
        memoryMap[i].freeSpace = sizes[i];
        memoryMap[i].isOccupied = 0;
        memoryMap[i].assignedPID = 0;
    }
}

void freeSystemMemory() {
    for (int i = 0; i < numberOfPartitions; i++) {
        memoryMap[i].isOccupied = 0;
        memoryMap[i].assignedPID = 0;
        memoryMap[i].freeSpace = memoryMap[i].totalSize;
    }
}

int allocateMemoryFirstFit(int pid, int sizeRequired) {
    for (int i = 0; i < numberOfPartitions; i++) {
        if (!memoryMap[i].isOccupied && memoryMap[i].totalSize >= sizeRequired) {
            memoryMap[i].isOccupied = 1;
            memoryMap[i].assignedPID = pid;
            memoryMap[i].freeSpace = memoryMap[i].totalSize - sizeRequired;
            printf("Allocated %d MB to PID %d in Partition %d (Internal fragmentation %d MB)\n",
                   sizeRequired, pid, memoryMap[i].blockID, memoryMap[i].freeSpace);
            return memoryMap[i].blockID;
        }
    }
    printf("Memory allocation failed for PID %d - no suitable partition (First-Fit).\n", pid);
    return -1;
}

void freeMemoryByPID(int pid) {
    for (int i = 0; i < numberOfPartitions; i++) {
        if (memoryMap[i].assignedPID == pid) {
            memoryMap[i].isOccupied = 0;
            memoryMap[i].assignedPID = 0;
            memoryMap[i].freeSpace = memoryMap[i].totalSize;
            printf("Freed partition %d from PID %d\n", memoryMap[i].blockID, pid);
            return;
        }
    }
}

void showMemoryMap() {
    printf("\nMemory partitions (MB):\n");
    printf("Part  Size  Free  Occupied PID\n");
    for (int i = 0; i < numberOfPartitions; i++) {
        printf("%4d  %4d  %4d    %s    %d\n",
               memoryMap[i].blockID,
               memoryMap[i].totalSize,
               memoryMap[i].freeSpace,
               memoryMap[i].isOccupied ? "Yes" : "No ",
               memoryMap[i].assignedPID);
    }
}
void getTimestamp(char *buf, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
}
void logEvent(char *event) {
    FILE *fp = fopen("SERCLog.txt", "a");
    if (fp == NULL) {
        printf("Error opening log file!\n");
        return;
    }
    char ts[32];
    getTimestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [LOG] %s\n", ts, event);
    fclose(fp);
}
void logProcessAction(char *action, int processID, char *name, char *description) {
    FILE *fp = fopen("SERCLog.txt", "a");
    if (fp == NULL) {
        printf("Cannot write to log file!\n");
        return;
    }
    char ts[32];
    getTimestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [PROCESS] %s | PID=%d | Name=%s | Description=%s\n",
            ts, action, processID, name, description);
    fclose(fp);
}
void logScheduling(char *algorithm, float averageWaitingTime, float averageTurnaround) {
    FILE *fp = fopen("SERCLog.txt", "a");
    if (fp == NULL) return;
    char ts[32];
    getTimestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [SCHEDULER] %s | AvgWait=%.2f | AvgTurn=%.2f\n",
            ts, algorithm, averageWaitingTime, averageTurnaround);
    fclose(fp);
}
void logDeadlockEvent(char *event, int processID) {
    FILE *fp = fopen("SERCLog.txt", "a");
    if (fp == NULL) return;
    char ts[32];
    getTimestamp(ts, sizeof(ts));
    fprintf(fp, "[%s] [DEADLOCK] %s | PID=%d\n", ts, event, processID);
    fclose(fp);
}
void viewLog() {
    FILE *fp = fopen("SERCLog.txt", "r");
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
void clearLog() {
    FILE *fp = fopen("SERCLog.txt", "w");
    if (fp == NULL) {
        printf("Cannot clear log!\n");
        return;
    }
    fclose(fp);
    printf("Log file cleared.\n");
}
int handleMenuChoice(int choice) {
    int processID;
    
    if (choice == 1) {
        createProcess();
    } else if (choice == 2) {
        listProcesses();
    } else if (choice == 3) {
        int subChoice;
        printf("Choose action for process: 1=Suspend, 2=Resume: ");
        scanf("%d", &subChoice);
        printf("Enter PID: ");
        scanf("%d", &processID);
        char desc[100] = "";
        for (int i = 0; i < processCount; i++) {
            if (processTable[i].processID == processID) {
                strcpy(desc, processTable[i].description);
                break;
            }
        }
        if (subChoice == 1) {
            suspendProcess(processID);
            logProcessAction("SUSPENDED", processID, "", desc);
        } else if (subChoice == 2) {
            resumeProcess(processID);
            logProcessAction("RESUMED", processID, "", desc);
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
        terminateProcess(processID);
        logProcessAction("TERMINATED", processID, "", desc);
    } else if (choice == 5) {
        int algChoice;
        printf("Choose scheduling algorithm: 1=FCFS, 2=Priority: ");
        scanf("%d", &algChoice);
        if (algChoice == 1) {
            scheduleFCFS();
        } else if (algChoice == 2) {
            schedulePriority();
        } else {
            printf("Invalid scheduling option. Use 1 or 2.\n");
        }
    } else if (choice == 6) {
        showResources();
    } else if (choice == 7) {
        int req[3];
        printf("Enter PID: ");
        scanf("%d", &processID);
        printf("Enter request for (Ambulances, Fire Units, Police Units): ");
        scanf("%d %d %d", &req[0], &req[1], &req[2]);
        requestResources(processID, req);
        logDeadlockEvent("REQUEST", processID);
    } else if (choice == 8) {
        viewLog();
    } else if (choice == 9) {
        clearLog();
    } else if (choice == 10) {
        int subChoice;
        printf("Delete single process (1) or delete all processes (2)? ");
        scanf("%d", &subChoice);
        if (subChoice == 1) {
            printf("Enter PID to delete: ");
            scanf("%d", &processID);
            deleteProcess(processID);
        } else if (subChoice == 2) {
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
        logEvent("System shutdown");
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
    logEvent("System started");
    loadProcessesFromFile();
    initSystemMemory();
    
    for (int i = 0; i < processCount; i++) {
        int pid = processTable[i].processID;
        int part = processTable[i].memoryPartitionID;
        if (part > 0 && part <= numberOfPartitions) {
            memoryMap[part - 1].isOccupied = 1;
            memoryMap[part - 1].assignedPID = pid;
            memoryMap[part - 1].freeSpace = memoryMap[part - 1].totalSize - processTable[i].memoryRequired;
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
        if (!handleMenuChoice(choice)) {
            break;
        }
    }
    return 0;
}


