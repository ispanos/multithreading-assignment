#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// assignment defaults
#define N_TEL 3
#define N_COOK 2
#define N_OVEN 10
#define N_DELIVERER 7
#define T_ORDERLOW 1
#define T_ORDERHIGH 5
#define N_ORDERLOW 1
#define N_ORDERHIGH 5
#define T_PAYMENTLOW 1
#define T_PAYMENTHIGH 2
#define C_PIZZA 10
#define P_FAIL 5
#define T_PREP 1
#define T_BAKE 10
#define T_PACK 2
#define T_DELLOW 5
#define T_DELHIGH 15

// This is called in sync_printf, but also calls sync_printf
// so it has to be added here.
void check_pthread_rc(char *arr, int rc);

struct Thread_Info {
    unsigned int order_id;
    unsigned int seed;
};

struct Daily_Statistics {
    unsigned int revenue;
    unsigned int total_calls;
    unsigned int successful_orders;
    long int call_wait_total;
    long int call_wait_max;
    long int door_time_total;
    long int door_time_max;
    long int cold_time_total;
    long int cold_time_max;
};

#define ThreadInfo struct Thread_Info
#define DayStats struct Daily_Statistics
