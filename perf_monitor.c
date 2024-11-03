#include <linux/perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define num_counters 6

struct perf_event_attr attr[10];

// Performance counter structure
typedef struct {
    char *name;
    int type;
    int config;
} perf_counter_t;

void config_perfevent(int pid, int idx, perf_counter_t counter) {
    memset(&attr[idx], 0, sizeof(struct perf_event_attr));
    attr[idx].type = counter.type;
    attr[idx].config = counter.config;
    attr[idx].size = sizeof(struct perf_event_attr);
    attr[idx].disabled = 0; 
    attr[idx].exclude_kernel = 0;  
    attr[idx].exclude_hv = 0;      
}

int perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

int open_perfevent(int pid, int idx) {
    int fd = perf_event_open(&attr[idx], pid, -1, -1, 0);
    if (fd == -1) {
        perror("perf_event_open");
        exit(EXIT_FAILURE);
    }
    return fd;
}

long long read_perfevent(int fd) {
    long long value;
    read(fd, &value, sizeof(long long));
    return value;
}

int is_process_alive(int pid) {
    return kill(pid, 0) == 0 || errno != ESRCH;
}

int main(int argc, char **argv) {
    int pid;
    int fd[num_counters];
    long long values[num_counters] = {0};
    long long sum[num_counters] = {0};
    struct timespec start_time, current_time;
    float elapsed_seconds = 0;
    float time_space = 0.5;
    int monitoring_mode = 0; // 0 for time-based, 1 for monitoring until process end
    float monitor_duration = 0;

    if (argc < 3) {
        printf("Usage: %s <PID> <mode> [seconds]\n", argv[0]);
        printf("Modes: time <seconds> or end\n");
        exit(EXIT_FAILURE);
    }

    pid = atoi(argv[1]);

    if (strcmp(argv[2], "time") == 0) {
        monitoring_mode = 0;
        if (argc < 4) {
            printf("Please specify the number of seconds to monitor.\n");
            exit(EXIT_FAILURE);
        }
        monitor_duration = atof(argv[3]);
    } else if (strcmp(argv[2], "end") == 0) {
        monitoring_mode = 1;
    } else {
        printf("Invalid mode. Use 'time' or 'end'.\n");
        exit(EXIT_FAILURE);
    }

    perf_counter_t counters[] = {
        {"CPU cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES},
        {"L1/L2 TLB miss", PERF_TYPE_RAW, 0x10D},
        {"CPU clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK},
        {"CPU migrations", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS},
        {"node_read_bytes", PERF_TYPE_RAW, 0x10F},
        {"node_write_bytes", PERF_TYPE_RAW, 0x110}
    };

    for (int i = 0; i < num_counters; i++) {
        config_perfevent(pid, i, counters[i]);
    }

    for (int i = 0; i < num_counters; i++) {
        fd[i] = open_perfevent(pid, i);
    }

    for (int i = 0; i < num_counters; i++) {
        ioctl(fd[i], PERF_EVENT_IOC_ENABLE, 0);
    }

    do {
        usleep(time_space * 1000000); //500000 = 0.5s; 1000000 = 1s

        for (int i = 0; i < num_counters; i++) {
            values[i] = read_perfevent(fd[i]);
            sum[i] += values[i];
        }

        elapsed_seconds += time_space;

    } while ((monitoring_mode == 0 && elapsed_seconds < monitor_duration) || //Time mode
             (monitoring_mode == 1 && is_process_alive(pid))); //Till the program ends

    for (int i = 0; i < num_counters; i++) {
        printf("Mean value for %s: %.2f /s\n", counters[i].name, (sum[i] / elapsed_seconds)); 
    }

    for (int i = 0; i < num_counters; i++) {
        ioctl(fd[i], PERF_EVENT_IOC_DISABLE, 0);
        close(fd[i]);
    }

    return 0;
}