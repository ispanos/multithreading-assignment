#include "o1150174.h"

DayStats stats = {0};

// Safe print
pthread_mutex_t mutex_printf = PTHREAD_MUTEX_INITIALIZER;

// Packager
pthread_mutex_t mutex_package = PTHREAD_MUTEX_INITIALIZER;

// Call Center
int avail_tel = N_TEL;
pthread_mutex_t mutex_tel = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_tel = PTHREAD_COND_INITIALIZER;

// Cook
int avail_cook = N_COOK;
pthread_mutex_t mutex_cook = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_cook = PTHREAD_COND_INITIALIZER;

// Oven
int avail_ovens = N_OVEN;
pthread_mutex_t mutex_ovens = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_ovens = PTHREAD_COND_INITIALIZER;

// Delivery
int avail_deliver = N_DELIVERER;
pthread_mutex_t mutex_deliver = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_deliver = PTHREAD_COND_INITIALIZER;

// Mutexes for daily statistics
pthread_mutex_t mutex_stats_calls = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_stats_tel = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_stats_end = PTHREAD_MUTEX_INITIALIZER;

// Behaves like printf, but uses a mutex to make it thread safe
int sync_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    int print_lock_rc = pthread_mutex_lock(&mutex_printf);
    check_pthread_rc("mutex lock", print_lock_rc);

    vprintf(format, args);

    int print_unlock_rc = pthread_mutex_unlock(&mutex_printf);
    check_pthread_rc("mutex unlock", print_unlock_rc);

    va_end(args);
    return 0;
}

// Checks if memory was allocated - otherwise exits
void check_malloc(void *ptr) {
    if (ptr == NULL) {
        sync_printf("OOM. Exiting...\n\n", NULL);
        exit(5);
    }
}

// Checks if pthread function was executed with no errors - otherwise exits
void check_pthread_rc(char *arr, int rc) {
    if (rc != 0) {
        sync_printf("Error in pthread %s. Return code: %d\n", arr, rc);
        exit(9);
    }
}

// Thread safely decreases the available operators or ovens while they work
// on the current order
void operators_acquire(int *operators, int needed, pthread_mutex_t *mutex,
                       pthread_cond_t *cond) {
    int lock_rc = pthread_mutex_lock(mutex);
    check_pthread_rc("mutex lock", lock_rc);

    // Waits until contdition is met
    while (*operators < needed) {
        int wait_rc = pthread_cond_wait(cond, mutex);
        check_pthread_rc("condition wait", wait_rc);
    }

    // Decreaasing the number of operators means they are working on our thread
    *operators -= needed;

    int unlock_rc = pthread_mutex_unlock(mutex);
    check_pthread_rc("mutex unlock", unlock_rc);
}

// Thread safely increases the available operators or ovens when they are done
// working on the current order
void operators_release(int *operators, int released, pthread_mutex_t *mutex,
                       pthread_cond_t *cond) {
    int lock_rc = pthread_mutex_lock(mutex);
    check_pthread_rc("mutex lock", lock_rc);

    // Increaasing the number of operators means they are done working on our
    // thread
    *operators += released;

    // Broadcast is only needed for ovens, since we need multiple for each
    // thread
    if (mutex == &mutex_ovens) {
        int broadcast_rc = pthread_cond_broadcast(cond);
        check_pthread_rc("condition broadcast", broadcast_rc);
    } else {
        int signal_rc = pthread_cond_signal(cond);
        check_pthread_rc("condition signal", signal_rc);
    }

    int unlock_rc = pthread_mutex_unlock(mutex);
    check_pthread_rc("mutex unlock", unlock_rc);
}

// Represents an order from the moment a customer calls until the deliverer come
// back from the delivery
void *pizza_order(void *thread_info) {
    ThreadInfo *info = (ThreadInfo *)thread_info;

    // Represents the moment a customer starts calling
    struct timespec call_time;
    clock_gettime(CLOCK_REALTIME, &call_time);

    // The only daily statistic that needs to be accounted for even if payment
    // fails
    int stats_call_lock_rc = pthread_mutex_lock(&mutex_stats_calls);
    check_pthread_rc("mutex lock", stats_call_lock_rc);
    ++stats.total_calls;
    int stats_call_unlock_rc = pthread_mutex_unlock(&mutex_stats_calls);
    check_pthread_rc("mutex unlock", stats_call_unlock_rc);

    // Telephone Line Start
    operators_acquire(&avail_tel, 1, &mutex_tel, &cond_tel);

    // Represents the moment the call was answered
    struct timespec answer_time;
    clock_gettime(CLOCK_REALTIME, &answer_time);

    // sleeping for `wait_pay` seconds represents the time it takes to proccess
    // a payment
    unsigned int wait_pay =
        T_PAYMENTLOW +
        (rand_r(&(info->seed)) % (T_PAYMENTHIGH - T_PAYMENTLOW + 1));
    sleep(wait_pay);

    // Orders may fail with a P_FAIL% chance
    if (rand_r(&(info->seed)) % 100 <= P_FAIL) {
        operators_release(&avail_tel, 1, &mutex_tel, &cond_tel);
        sync_printf("The order with id %d failed.\n", info->order_id);
        pthread_exit(&info->order_id);
    }

    sync_printf("The order with id %d was placed successfully.\n",
                info->order_id);

    unsigned int order_size = rand_r(&(info->seed)) % N_ORDERHIGH + N_ORDERLOW;

    operators_release(&avail_tel, 1, &mutex_tel, &cond_tel);

    // Telephone Line End

    // Cook Start
    operators_acquire(&avail_cook, 1, &mutex_cook, &cond_cook);

    sleep(T_PREP * order_size);

    // Oven Start
    // Ovens are required for cook's to start working on the next order
    operators_acquire(&avail_ovens, order_size, &mutex_ovens, &cond_ovens);

    operators_release(&avail_cook, 1, &mutex_cook, &cond_cook);
    // Cook End

    sleep(T_BAKE * order_size);

    // Once the order is done cooking(, we assume there is no residual heat in
    // the oven), and the pizzas start to get cold from now on
    struct timespec cooked_time;
    clock_gettime(CLOCK_REALTIME, &cooked_time);

    // Packaging Start
    // Lock the packager to package the order, before freeing the ovens
    // This is the only operator that is unique and doesn't need a condition
    // signal
    int lock_pack_rc = pthread_mutex_lock(&mutex_package);
    check_pthread_rc("mutex lock", lock_pack_rc);

    sleep(T_PACK * order_size);

    // Frees the ovens when all pizzas are packaged
    operators_release(&avail_ovens, order_size, &mutex_ovens, &cond_ovens);
    // Oven End

    int unlock_pack_rc = pthread_mutex_unlock(&mutex_package);
    check_pthread_rc("mutex unlock", unlock_pack_rc);
    // Packaging End

    struct timespec package_time;
    clock_gettime(CLOCK_REALTIME, &package_time);
    sync_printf("The order with id %d was prepared in %d min %d sec\n",
                info->order_id, (package_time.tv_sec - call_time.tv_sec) / 60,
                (package_time.tv_sec - call_time.tv_sec) % 60);

    // Delivery Start
    operators_acquire(&avail_deliver, 1, &mutex_deliver, &cond_deliver);

    unsigned int wait_delivery =
        T_DELLOW + rand_r(&(info->seed)) % (T_DELHIGH - T_DELLOW + 1);
    sleep(wait_delivery);

    struct timespec delivery_time;
    clock_gettime(CLOCK_REALTIME, &delivery_time);

    sync_printf("The order with id %d was delivered in %ld min %d sec\n",
                info->order_id, (delivery_time.tv_sec - call_time.tv_sec) / 60,
                (delivery_time.tv_sec - call_time.tv_sec) % 60);

    sleep(wait_delivery);
    operators_release(&avail_deliver, 1, &mutex_deliver, &cond_deliver);
    // Delivery End

    // Locks mutex to edit the global variable `stats`, just like we dead for
    // the total calls entry
    int stats_lock_end_rc = pthread_mutex_lock(&mutex_stats_end);
    check_pthread_rc("mutex lock", stats_lock_end_rc);

    stats.revenue += (C_PIZZA * order_size);
    ++stats.successful_orders;

    stats.door_time_total += (delivery_time.tv_sec - call_time.tv_sec);
    stats.call_wait_total += (answer_time.tv_sec - call_time.tv_sec);
    stats.cold_time_total += (delivery_time.tv_sec - cooked_time.tv_sec);

    if (answer_time.tv_sec - call_time.tv_sec > stats.call_wait_max) {
        stats.call_wait_max = answer_time.tv_sec - call_time.tv_sec;
    }

    if (delivery_time.tv_sec - call_time.tv_sec > stats.door_time_max) {
        stats.door_time_max = delivery_time.tv_sec - call_time.tv_sec;
    }

    if (delivery_time.tv_sec - cooked_time.tv_sec > stats.cold_time_max) {
        stats.cold_time_max = delivery_time.tv_sec - cooked_time.tv_sec;
    }

    int stats_unlock_end_rc = pthread_mutex_unlock(&mutex_stats_end);
    check_pthread_rc("mutex unlock", stats_unlock_end_rc);

    pthread_exit(&info->order_id);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Wrong number of arguments. Exiting...\n\n");
        exit(3);
    }

    // The following two checks are suboptimal, but work if either of the
    // arguments passed to the executable don't start with a digit.
    // `100pizzas 1000seed` would technically work for example, even if those
    // arguments contain non-digit chars
    // `pizzas100 seed1000` wouldn't work
    int N_cust = atoi(argv[1]);
    if (N_cust <= 0) {
        printf("Invalid argument.\n");
        exit(4);
    }

    unsigned int seed = atoi(argv[2]);
    if (seed <= 0) {
        printf("Invalid argument.\n");
        exit(4);
    }

    // Allocate memory for treads' info
    ThreadInfo *threads_info = malloc(N_cust * sizeof(ThreadInfo));
    // exits if there's not enough memory
    check_malloc((void *)threads_info);

    // Allocate memory for N number of threads
    // This can be done because the amount of customers is known from the
    // beginning Otherwise we would have to allocate memory when a new customer
    // appears
    pthread_t *threads = malloc(N_cust * sizeof(pthread_t));
    // exits if there's not enough memory
    check_malloc((void *)threads);

    printf(
        "Expecting %d customers\n"
        "********************************************\n\n\n\n\n",
        N_cust);

    // Simulates `N_cust` number of customers calling every `wait` time.
    // Every customer is a new thread
    for (int i = 0; i < N_cust; i++) {
        threads_info[i].order_id = i + 1;
        // thread seed is a placeholder
        threads_info[i].seed = seed + i + 1;

        // create new thread
        int thread_rc =
            pthread_create(&threads[i], NULL, &pizza_order, &threads_info[i]);
        if (thread_rc != 0) {
            sync_printf("Unexpected error when creating new thread.\n", NULL);
            exit(6);
        }

        // simulate waiting for new call
        int wait = rand_r(&seed) % T_ORDERHIGH + T_ORDERLOW;
        sleep(wait);
    }

    // join threads
    for (int i = 0; i < N_cust; i++) {
        int thread_rc = pthread_join(threads[i], NULL);
        if (thread_rc != 0) {
            printf("Unexpected error when joining thread.\n");
            exit(8);
        }
    }

    // Free memory for thread related data
    int mutex_destroy_rc;
    mutex_destroy_rc = pthread_mutex_destroy(&mutex_printf);
    check_pthread_rc("mutex destroy", mutex_destroy_rc);
    mutex_destroy_rc = pthread_mutex_destroy(&mutex_package);
    check_pthread_rc("mutex destroy", mutex_destroy_rc);
    mutex_destroy_rc = pthread_mutex_destroy(&mutex_tel);
    check_pthread_rc("mutex destroy", mutex_destroy_rc);
    mutex_destroy_rc = pthread_mutex_destroy(&mutex_cook);
    check_pthread_rc("mutex destroy", mutex_destroy_rc);
    mutex_destroy_rc = pthread_mutex_destroy(&mutex_ovens);
    check_pthread_rc("mutex destroy", mutex_destroy_rc);
    mutex_destroy_rc = pthread_mutex_destroy(&mutex_deliver);
    check_pthread_rc("mutex destroy", mutex_destroy_rc);
    mutex_destroy_rc = pthread_mutex_destroy(&mutex_stats_calls);
    check_pthread_rc("mutex destroy", mutex_destroy_rc);
    mutex_destroy_rc = pthread_mutex_destroy(&mutex_stats_tel);
    check_pthread_rc("mutex destroy", mutex_destroy_rc);
    mutex_destroy_rc = pthread_mutex_destroy(&mutex_stats_end);
    check_pthread_rc("mutex destroy", mutex_destroy_rc);

    int cond_destroy_rc;
    cond_destroy_rc = pthread_cond_destroy(&cond_tel);
    check_pthread_rc("cond destroy", cond_destroy_rc);
    cond_destroy_rc = pthread_cond_destroy(&cond_cook);
    check_pthread_rc("cond destroy", cond_destroy_rc);
    cond_destroy_rc = pthread_cond_destroy(&cond_ovens);
    check_pthread_rc("cond destroy", cond_destroy_rc);
    cond_destroy_rc = pthread_cond_destroy(&cond_deliver);
    check_pthread_rc("cond destroy", cond_destroy_rc);

    free(threads_info);
    free(threads);

    printf("\n\n\nPizzaria's performance results.\n");
    printf(
        "Total revenue               : %u\n"
        "Number of successful orders : %u\n"
        "Number of failed orders     : %d\n",
        stats.revenue, stats.successful_orders,
        stats.total_calls - stats.successful_orders);

    if (stats.total_calls == 0) {
        printf("Something must have gone terribly wrong!!\n");
        exit(66);
    }

    printf(
        "Mean call waiting time      : %ld sec\n"
        "Max  call waiting time      : %ld sec\n",
        (stats.call_wait_total / stats.total_calls), stats.call_wait_max);

    if (stats.successful_orders == 0) {
        printf("0 successful orders\n");
        return 0;
    }

    printf(
        "Mean order time to delivery : %ld min %ld sec\n"
        "Max  order time to delivery : %ld min %ld sec\n",
        (stats.door_time_total / stats.successful_orders / 60),
        (stats.door_time_total / stats.successful_orders % 60),
        stats.door_time_max / 60,
        stats.door_time_max % 60);

    printf(
        "Mean time that pizzas were getting cold: %ld min %ld sec\n"
        "Max  time that pizzas were getting cold: %ld min %ld sec\n",
        (stats.cold_time_total / stats.successful_orders) / 60,
        (stats.cold_time_total / stats.successful_orders) % 60,
        stats.cold_time_max / 60,
        stats.cold_time_max % 60);

    return 0;
}
