#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define TIME_QUANTUM 1 //RR 규정시간량
#define TIME_QUANTUM_Q1 1 //큐1 규정시간량
#define TIME_QUANTUM_Q2 2 //큐2 규정시간량
#define TIME_QUANTUM_Q3 4 //큐3 규정시간량

typedef struct Process {
    int id;
    int burst_time;
    int remaining_time;
    int waiting_time;
    int turnaround_time;
    struct Process* next;
} Process;

typedef struct Queue {
    Process* head;
    Process* tail;
    pthread_mutex_t lock;
} Queue;

void enqueue(Queue* queue, Process* process) {
    pthread_mutex_lock(&queue->lock);
    if (queue->tail == NULL) {
        queue->head = queue->tail = process;
    } else {
        queue->tail->next = process;
        queue->tail = process;
    }
    process->next = NULL;
    pthread_mutex_unlock(&queue->lock);
}

Process* dequeue(Queue* queue) {
    pthread_mutex_lock(&queue->lock);
    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }
    Process* process = queue->head;
    queue->head = queue->head->next;
    if (queue->head == NULL) queue->tail = NULL;
    pthread_mutex_unlock(&queue->lock);
    return process;
}

int isEmpty(Queue* queue) {
    pthread_mutex_lock(&queue->lock);
    int empty = (queue->head == NULL);
    pthread_mutex_unlock(&queue->lock);
    return empty;
}

void print_gantt_chart(char* queue_name, int process_id, int start_time, int end_time) {
    static char* last_queue = NULL;
    if (last_queue != NULL && strcmp(last_queue, queue_name) != 0) {
        printf("\n");
        printf("%s: ", queue_name);
    }
    printf("P%d (%d-%d) ", process_id, start_time, end_time);
    last_queue = queue_name;
}

typedef struct SchedulerArgs {
    Queue* q1;
    Queue* q2;
    Queue* q3;
    int* time;
    pthread_mutex_t* time_lock;
} SchedulerArgs;

void* mlfq_scheduler(void* args) {
    SchedulerArgs* sched_args = (SchedulerArgs*)args;
    Queue* q1 = sched_args->q1;
    Queue* q2 = sched_args->q2;
    Queue* q3 = sched_args->q3;
    int* time = sched_args->time;
    pthread_mutex_t* time_lock = sched_args->time_lock;

    while (!isEmpty(q1) || !isEmpty(q2) || !isEmpty(q3)) {
        Process* current = NULL;
        int quantum = 0;
        char* queue_name = NULL;

        if (!isEmpty(q1)) {
            current = dequeue(q1);
            quantum = TIME_QUANTUM_Q1;
            queue_name = "Q1";
        } else if (!isEmpty(q2)) {
            current = dequeue(q2);
            quantum = TIME_QUANTUM_Q2;
            queue_name = "Q2";
        } else if (!isEmpty(q3)) {
            current = dequeue(q3);
            quantum = TIME_QUANTUM_Q3;
            queue_name = "Q3";
            if (current->remaining_time <= TIME_QUANTUM_Q1) {
                enqueue(q1, current);
                continue;
            } else if (isEmpty(q1) && isEmpty(q2) && isEmpty(q3)) { //전체큐에 1개의 프로세스만 남아있을때
                enqueue(q1, current); //큐1로 이동
                continue;
            }
        }

        if (current) {
            pthread_mutex_lock(time_lock);
            if (isEmpty(q1) && isEmpty(q2) && isEmpty(q3)) {
                quantum = current->remaining_time; 
            }
            int execute_time = (current->remaining_time < quantum) ? current->remaining_time : quantum;
            current->remaining_time -= execute_time;
            print_gantt_chart(queue_name, current->id, *time, *time + execute_time);
            *time += execute_time;
            pthread_mutex_unlock(time_lock);

            if (current->remaining_time > 0) {
                if (strcmp(queue_name, "Q1") == 0) {
                    enqueue(q2, current);
                } else if (strcmp(queue_name, "Q2") == 0) {
                    enqueue(q3, current);
                } else {
                    enqueue(q3, current);
                }
            } else {
                current->turnaround_time = *time;
                current->waiting_time = *time - current->burst_time;
            }
        }
    }
    return NULL;
}

void mlfq_scheduling(Process processes[], int n) {
    printf("Q1: ");
    Queue q1 = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
    Queue q2 = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
    Queue q3 = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};

    for (int i = 0; i < n; i++) {
        processes[i].remaining_time = processes[i].burst_time;
        enqueue(&q1, &processes[i]);
    }

    int time = 0;
    pthread_mutex_t time_lock = PTHREAD_MUTEX_INITIALIZER;
    SchedulerArgs sched_args = {&q1, &q2, &q3, &time, &time_lock};

    pthread_t scheduler_thread;
    pthread_create(&scheduler_thread, NULL, mlfq_scheduler, &sched_args);

    pthread_join(scheduler_thread, NULL);

    printf("\n");

    float total_turnaround_time = 0, total_waiting_time = 0;
    for (int i = 0; i < n; i++) {
        total_turnaround_time += processes[i].turnaround_time;
        total_waiting_time += processes[i].waiting_time;
        printf("P%d - 반환시간: %d, 대기시간: %d\n", processes[i].id, processes[i].turnaround_time, processes[i].waiting_time);
    }
    printf("평균 반환시간: %.2f\n", total_turnaround_time / n);
    printf("평균 대기시간: %.2f\n", total_waiting_time / n);
}

typedef struct RoundRobinArgs {
    Process* processes;
    int n;
    int* time;
    pthread_mutex_t* time_lock;
} RoundRobinArgs;

void* round_robin_scheduler(void* args) {
    RoundRobinArgs* rr_args = (RoundRobinArgs*)args;
    Process* processes = rr_args->processes;
    int n = rr_args->n;
    int* time = rr_args->time;
    pthread_mutex_t* time_lock = rr_args->time_lock;
    Queue queue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};

    for (int i = 0; i < n; i++) {
        enqueue(&queue, &processes[i]);
    }

    while (!isEmpty(&queue)) {
        Process* current = dequeue(&queue);
        int execute_time = (current->remaining_time < TIME_QUANTUM) ? current->remaining_time : TIME_QUANTUM;
        current->remaining_time -= execute_time;

        pthread_mutex_lock(time_lock);
        print_gantt_chart("RR", current->id, *time, *time + execute_time);
        *time += execute_time;
        pthread_mutex_unlock(time_lock);

        if (current->remaining_time > 0) {
            enqueue(&queue, current);
        } else {
            current->turnaround_time = *time;
            current->waiting_time = *time - current->burst_time;
        }
    }

    return NULL;
}

void round_robin_scheduling(Process processes[], int n) {
    int time = 0;
    pthread_mutex_t time_lock = PTHREAD_MUTEX_INITIALIZER;
    RoundRobinArgs rr_args = {processes, n, &time, &time_lock};

    pthread_t scheduler_thread;
    pthread_create(&scheduler_thread, NULL, round_robin_scheduler, &rr_args);

    pthread_join(scheduler_thread, NULL);

    printf("\n");

    float total_turnaround_time = 0, total_waiting_time = 0;
    for (int i = 0; i < n; i++) {
        total_turnaround_time += processes[i].turnaround_time;
        total_waiting_time += processes[i].waiting_time;
        printf("P%d - 반환시간: %d, 대기시간: %d\n", processes[i].id, processes[i].turnaround_time, processes[i].waiting_time);
    }
    printf("평균 반환시간: %.2f\n", total_turnaround_time / n);
    printf("평균 대기시간: %.2f\n", total_waiting_time / n);
}

void reset_processes(Process processes[], int n) {
    for (int i = 0; i < n; i++) {
        processes[i].remaining_time = processes[i].burst_time;
        processes[i].waiting_time = 0;
        processes[i].turnaround_time = 0;
    }
}

int main() {
    Process processes[] = {
        {1, 30, 0, 0, 0, NULL},
        {2, 20, 0, 0, 0, NULL},
        {3, 10, 0, 0, 0, NULL}
    };

    int n = sizeof(processes) / sizeof(processes[0]);

    printf("MLFQ Scheduling:\n");
    mlfq_scheduling(processes, n);

    reset_processes(processes, n);

    printf("\nRound Robin Scheduling: ");
    round_robin_scheduling(processes, n);

    return 0;
}
