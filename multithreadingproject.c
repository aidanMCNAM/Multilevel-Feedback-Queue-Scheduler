#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "prioque.h"

#define MAX_PROCESSES 100
#define MAX_PHASES 20

#define LEVEL_1 1
#define LEVEL_2 2
#define LEVEL_3 3
#define LEVEL_4 4

const int QUANTUM[] = {0, 10, 30, 100, 200};
const int B_LIMIT[] = {0, 1, 2, 2, -1};
const int G_LIMIT[] = {0, -1, 1, 2, 2};

typedef struct {
    int run;
    int io;
    int repeat;
} Phase;

typedef struct {
    int pid;
    int arrival_time;
    Phase phases[MAX_PHASES];
    int num_phases;
    int current_phase;
    int current_repeat;
    int run_remaining;
    int io_remaining;
    int queue_level;
    int b_counter;
    int g_counter;
    int is_waiting;
    int waiting_until;
    int finished;
    int cpu_time;
} Process;

Process processes[MAX_PROCESSES];
int process_count = 0;

Queue q1, q2, q3, q4;
int null_cpu_time = 0;

void init_queues() {
    init_queue(&q1, sizeof(Process), 1, NULL, 1);
    init_queue(&q2, sizeof(Process), 1, NULL, 1);
    init_queue(&q3, sizeof(Process), 1, NULL, 1);
    init_queue(&q4, sizeof(Process), 1, NULL, 1);
}

void parse_input() {
    int time, pid, run, io, repeat;
    while (scanf("%d %d %d %d %d", &time, &pid, &run, &io, &repeat) == 5) {
        int found = 0;
        for (int i = 0; i < process_count; i++) {
            if (processes[i].pid == pid) {
                if (time > processes[i].arrival_time)
                    processes[i].arrival_time = time;
                processes[i].phases[processes[i].num_phases++] = (Phase){run, io, repeat};
                found = 1;
                break;
            }
        }
        if (!found) {
            Process *p = &processes[process_count++];
            p->pid = pid;
            p->arrival_time = time;
            p->num_phases = 0;
            p->current_phase = 0;
            p->current_repeat = 0;
            p->run_remaining = 0;
            p->io_remaining = 0;
            p->queue_level = LEVEL_1;
            p->b_counter = 0;
            p->g_counter = 0;
            p->is_waiting = 0;
            p->waiting_until = -1;
            p->finished = 0;
            p->cpu_time = 0;
            p->phases[p->num_phases++] = (Phase){run, io, repeat};
        }
    }
}

void enqueue_arrivals(int clock) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].arrival_time == clock) {
            printf("PID: %d, ARRIVAL TIME: %d\n", processes[i].pid, clock);
            printf("CREATE: Process %d entered the ready queue at time %d.\n", processes[i].pid, clock);
            add_to_queue(&q1, &processes[i], 0);
        }
    }
}

int all_finished() {
    for (int i = 0; i < process_count; i++) {
        if (!processes[i].finished) return 0;
    }
    return 1;
}

Queue* get_queue_by_level(int level) {
    if (level == LEVEL_1) return &q1;
    if (level == LEVEL_2) return &q2;
    if (level == LEVEL_3) return &q3;
    return &q4;
}

void queue_process(Process *p, int clock) {
    printf("QUEUED: Process %d queued at level %d at time %d.\n", p->pid, p->queue_level, clock);
    add_to_queue(get_queue_by_level(p->queue_level), p, 0);
}

void run_scheduler() {
    int clock = 0;

    while (!all_finished()) {
        enqueue_arrivals(clock);

        for (int i = 0; i < process_count; i++) {
            if (processes[i].is_waiting && processes[i].waiting_until == clock) {
                processes[i].is_waiting = 0;
                queue_process(&processes[i], clock);
            }
        }

        Process current;
        int found = 0;
        Queue *queues[] = {&q1, &q2, &q3, &q4};
        for (int i = 0; i < 4; i++) {
            if (!empty_queue(queues[i])) {
                remove_from_front(queues[i], &current);
                found = 1;
                break;
            }
        }

        if (!found) {
            null_cpu_time++;
            clock++;
            continue;
        }

        int q = current.queue_level;
        int quantum = QUANTUM[q];
        Phase *phase = &current.phases[current.current_phase];
        if (current.run_remaining == 0)
            current.run_remaining = phase->run;

        int ticks = (current.run_remaining < quantum) ? current.run_remaining : quantum;
        printf("RUN: Process %d started execution from level %d at time %d; wants to execute for %d ticks.\n", current.pid, q, clock, current.run_remaining);

        for (int t = 0; t < ticks; t++) {
            clock++;
            current.run_remaining--;
            current.cpu_time++;
            enqueue_arrivals(clock);
        }

        if (current.run_remaining == 0) {
            if (current.current_repeat < phase->repeat) {
                current.io_remaining = phase->io;
                current.waiting_until = clock + phase->io;
                current.is_waiting = 1;
                current.current_repeat++;
                printf("I/O: Process %d blocked for I/O at time %d.\n", current.pid, clock);
                current.b_counter = 0;
            } else {
                current.current_phase++;
                current.current_repeat = 0;
                if (current.current_phase >= current.num_phases) {
                    current.finished = 1;
                    printf("FINISHED: Process %d finished at time %d.\n", current.pid, clock);
                    processes[current.pid % MAX_PROCESSES] = current;
                    continue;
                } else {
                    current.run_remaining = 0;
                }
            }
        } else {
            if (ticks == quantum) {
                current.b_counter++;
                if (B_LIMIT[current.queue_level] != -1 && current.b_counter >= B_LIMIT[current.queue_level]) {
                    if (current.queue_level < LEVEL_4) current.queue_level++;
                    current.b_counter = 0;
                    printf("DEMOTED: Process %d moved to level %d at time %d.\n", current.pid, current.queue_level, clock);
                }
            } else {
                current.g_counter++;
                if (G_LIMIT[current.queue_level] != -1 && current.g_counter >= G_LIMIT[current.queue_level]) {
                    if (current.queue_level > LEVEL_1) current.queue_level--;
                    current.g_counter = 0;
                }
            }
            queue_process(&current, clock);
        }
        processes[current.pid % MAX_PROCESSES] = current;
    }

    printf("Scheduler shutdown at time %d.\n", clock);
    printf("Total CPU usage for all processes scheduled:\n");
    printf("Process <<null>>:\t%d time units.\n", null_cpu_time);
    for (int i = 0; i < process_count; i++) {
        printf("Process %d:\t\t%d time units.\n", processes[i].pid, processes[i].cpu_time);
    }
}

int main() {
    init_queues();
    parse_input();
    run_scheduler();
    return 0;
}