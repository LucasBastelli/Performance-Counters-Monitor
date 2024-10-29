PROGRAMS = papi_monitor perf_monitor

all:	$(PROGRAMS)

papi_monitor: papi_monitor.c
	gcc -O3 -o papi_monitor papi_monitor.c -lpapi


perf_monitor: perf_monitor.c
	gcc -O3 -o perf_monitor perf_monitor.c



clean:
	rm -f $(PROGRAMS) *.o
	rm -f $(PROGRAMS)
