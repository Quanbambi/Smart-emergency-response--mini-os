#include <stdio.h>

#define max_processes 10

typedef enum {
  NEW,
  READY,
  WAITING,
  RUNNING,
  TERMINATED,
} processstate;

typedef struct {
  int process_id;
  char process_name[30];
  int priority_level;
  int burst_time;
  int turnaround_time;
  int waiting_time;
  processstate state;
} PCB;

int process_count = 0;
int next_pid = 1;
PCB processes[max_processes];

void create_process(){
  if (process_count >= max_processes){
    printf("Can't create more processes");
  };

  PCB new_process;
  new_process.process_id = next_pid++;

  printf("Enter process name: ");
  scanf("%s", new_process.process_name);
  
  printf("Enter process priority level: ");
  scanf("%d", &new_process.priority_level);

  printf("Enter process burst time: ");
  scanf("%d",  &new_process.burst_time);

  new_process.waiting_time = 0;
  new_process.turnaround_time = 0;
  new_process.state = NEW;

  processes[process_count] = new_process;
  process_count++;

  printf("Process %s created with process ID %d", new_process.process_name, new_process.process_id);
};

int main(){
  printf("WELCOME TO THE SMART EMERGENCY RESPONSE CENTRE\n");
}