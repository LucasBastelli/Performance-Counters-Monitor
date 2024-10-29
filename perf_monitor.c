#include <linux/perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>

// Structure to store the PERF event
struct perf_event_attr attr[10]; // Increased size for more counters

// Structure to define a performance counter
typedef struct {
    char *name;
    int type;
    int config;
} perf_counter_t;

// Function to configure the PERF event
void config_perfevent(int pid, int idx, perf_counter_t counter) {
    memset(&attr[idx], 0, sizeof(struct perf_event_attr));
    attr[idx].type = counter.type;
    attr[idx].config = counter.config;
    attr[idx].size = sizeof(struct perf_event_attr);
    attr[idx].disabled = 0; 
    attr[idx].exclude_kernel = 0;  
    attr[idx].exclude_hv = 0;      
}

// Wrapper function for perf_event_open syscall
int perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

// Function to open the PERF event
int open_perfevent(int pid, int idx) {
    int fd = perf_event_open(&attr[idx], pid, -1, -1, 0);
    if (fd == -1) {
        perror("perf_event_open");
        exit(EXIT_FAILURE);
    }
    return fd;
}

// Function to read the value of the PERF event
long long read_perfevent(int fd) {
    long long value;
    read(fd, &value, sizeof(long long));
    return value;
}

int main(int argc, char **argv) {
    int pid;
    int num_counters = 4; // Number of counters to monitor
    int fd[10]; // File descriptors for perf events
    long long values[10] = {0};
    long long sum[10] = {0};
    struct timespec start_time, current_time;
    double elapsed_seconds;

    // Check if PID was provided
    if (argc < 2) {
        printf("Usage: %s <PID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Convert PID to integer
    pid = atoi(argv[1]);

    // Define the performance counters
    perf_counter_t counters[] = {
        {"CPU cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES},
        {"L1/L2 TLB miss", PERF_TYPE_RAW, 0x10D},
        {"CPU clock", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK},
        {"CPU migrations", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS},
        // Add more counters here...
    };

    num_counters = sizeof(counters) / sizeof(counters[0]); // Update num_counters

    // Configure PERF events
    for (int i = 0; i < num_counters; i++) {
        config_perfevent(pid, i, counters[i]);
    }

    // Open PERF events
    for (int i = 0; i < num_counters; i++) {
        fd[i] = open_perfevent(pid, i);
    }

    // Enable PERF events
    for (int i = 0; i < num_counters; i++) {
        ioctl(fd[i], PERF_EVENT_IOC_ENABLE, 0);
    }

    // Get start time
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Monitor for 5 seconds, reading values every 0.5 seconds
    do {
        usleep(500000); // Wait 0.5 seconds

        for (int i = 0; i < num_counters; i++) {
            values[i] = read_perfevent(fd[i]);
            sum[i] += values[i];
        }

        // Get current time
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        elapsed_seconds = (current_time.tv_sec - start_time.tv_sec) +
                          (current_time.tv_nsec - start_time.tv_nsec) / 1e9;

    } while (elapsed_seconds < 5.0);

    // Calculate and print the mean values
    for (int i = 0; i < num_counters; i++) {
        printf("Mean value for %s: %lld /s\n", counters[i].name, sum[i] / 10); 
    }

    // Disable and close PERF events
    for (int i = 0; i < num_counters; i++) {
        ioctl(fd[i], PERF_EVENT_IOC_DISABLE, 0);
        close(fd[i]);
    }

    return 0;
}