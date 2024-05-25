#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    int id;
    int arrival_time;
    int burst_time;
    int waiting_time;
    int turnaround_time;
    int completed;
} Process;

typedef struct {
    Process *processes;
    pthread_mutex_t *mutex;
    int *current_time;
    int n;
} ThreadArgs;

void *hrn_scheduler(void *arg) {
    ThreadArgs *thread_args = (ThreadArgs *)arg;
    Process *processes = thread_args->processes;
    pthread_mutex_t *mutex = thread_args->mutex;
    int *current_time = thread_args->current_time;
    int n = thread_args->n;

    while (1) {
        pthread_mutex_lock(mutex);
        int min_burst = -1;
        int min_index = -1;
        for (int i = 0; i < n; ++i) {
            if (processes[i].completed == 0 && processes[i].arrival_time <= *current_time) {
                if (min_burst == -1 || processes[i].burst_time < min_burst) {
                    min_burst = processes[i].burst_time;
                    min_index = i;
                }
            }
        }
        if (min_index != -1) {
            processes[min_index].completed = 1;
            pthread_mutex_unlock(mutex);
            printf("P%d (%d-%d)\n", processes[min_index].id, *current_time, *current_time + processes[min_index].burst_time);
            processes[min_index].waiting_time = *current_time - processes[min_index].arrival_time;
            *current_time += processes[min_index].burst_time;
            processes[min_index].turnaround_time = processes[min_index].waiting_time + processes[min_index].burst_time;
        } else {
            pthread_mutex_unlock(mutex);
            if (*current_time < processes[n - 1].arrival_time) {
                *current_time = processes[n - 1].arrival_time;
            }
            break;
        }
    }

    pthread_exit(NULL);
}

void calculate_waiting_time(Process processes[], int n) {
    pthread_t threads[n];
    ThreadArgs thread_args;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int current_time = 0;

    thread_args.processes = processes;
    thread_args.mutex = &mutex;
    thread_args.current_time = &current_time;
    thread_args.n = n;

    pthread_create(&threads[0], NULL, hrn_scheduler, (void *)&thread_args);

    pthread_join(threads[0], NULL);
}

void hrn_scheduling(Process processes[], int n) {
    float total_turnaround_time = 0;
    float total_waiting_time = 0;
    for (int i = 0; i < n; i++) {
        total_turnaround_time += processes[i].turnaround_time;
        total_waiting_time += processes[i].waiting_time;
        printf("P%d - 반환시간: %d, 대기시간: %d\n", processes[i].id, processes[i].turnaround_time, processes[i].waiting_time);
    }
    printf("평균 반환시간: %.2f\n", total_turnaround_time / n);
    printf("평균 대기시간: %.2f\n", total_waiting_time / n);
}

int main() {
    Process processes[] = {
        {1, 0, 10, 0, 0, 0},
        {2, 1, 28, 0, 0, 0},
        {3, 2, 6, 0, 0, 0},
        {4, 3, 4, 0, 0, 0},
        {5, 4, 14, 0, 0, 0}
    };

    int n = sizeof(processes) / sizeof(processes[0]);

    printf("HRN Scheduling:\n");
    calculate_waiting_time(processes, n);
    hrn_scheduling(processes, n);

    return 0;
}
