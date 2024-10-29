#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <papi.h>
#include <signal.h>

#define NUM_EVENTS 4

int main(int argc, char **argv) {
  int EventSet = PAPI_NULL;
  long long values[NUM_EVENTS];
  int events[NUM_EVENTS] = {PAPI_L1_DCM, PAPI_L2_DCM, PAPI_TOT_CYC, PAPI_TLB_DM};
  char event_names[NUM_EVENTS][PAPI_MAX_STR_LEN];
  int retval;
  pid_t pid;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
    exit(1);
  }

  pid = atoi(argv[1]);

  // Initialize the PAPI library
  retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    fprintf(stderr, "Error initializing PAPI library: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  // Enable multiplexing (allows counting on multiple CPUs if supported)
  retval = PAPI_multiplex_init();
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error initializing PAPI multiplexing: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  // Create an EventSet
  retval = PAPI_create_eventset(&EventSet);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error creating EventSet: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  // Convert EventSet to a multiplexed EventSet
  retval = PAPI_assign_eventset_component(EventSet, 0);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error assigning component to EventSet: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  retval = PAPI_set_multiplex(EventSet);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error enabling multiplexing on EventSet: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  // Add events to the EventSet, checking for availability
  for (int i = 0; i < NUM_EVENTS; i++) {
    retval = PAPI_add_event(EventSet, events[i]);
    if (retval != PAPI_OK) {
      fprintf(stderr, "Error adding event %s: %s\n", PAPI_event_code_to_name(events[i], event_names[i]), PAPI_strerror(retval));
      exit(1);
    }
    PAPI_event_code_to_name(events[i], event_names[i]);
  }

  // Attach the EventSet to the specified process (requires elevated privileges)
  retval = PAPI_attach(EventSet, pid);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error attaching EventSet to process %d: %s\n", pid, PAPI_strerror(retval));
    exit(1);
  }

  // Start counting
  retval = PAPI_start(EventSet);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error starting counters: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  printf("Monitoring performance counters of process %d with multiplexing enabled...\n", pid);

  while (1) {
    sleep(1);

    // Check if the process is still running
    if (kill(pid, 0) != 0) {
      printf("Process %d has exited. Stopping monitoring.\n", pid);
      break;
    }

    // Read the counter values
    retval = PAPI_read(EventSet, values);
    if (retval != PAPI_OK) {
      fprintf(stderr, "Error reading counter values: %s\n", PAPI_strerror(retval));
      exit(1);
    }

    // Print the counter values
    for (int i = 0; i < NUM_EVENTS; i++) {
      printf("%s: %lld\n", event_names[i], values[i]);
    }
    printf("---------------------\n");
  }

  // Stop counting
  retval = PAPI_stop(EventSet, values);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error stopping counters: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  // Cleanup
  retval = PAPI_cleanup_eventset(EventSet);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error removing events from EventSet: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  retval = PAPI_destroy_eventset(&EventSet);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error destroying EventSet: %s\n", PAPI_strerror(retval));
    exit(1);
  }

  PAPI_shutdown();

  return 0;
}
