Program made to monitor an aplication, usage:

    ./papi_monitor <PID>

    ./papi_monitor $(pidof program)


Some counter are not available on PAPI, so we created a perf version, usage keeps the same

    ./perf_monitor <PID>

    ./papi_monitor $(pidof program)