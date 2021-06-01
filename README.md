# Multithreading assignment - Operating Systems

## Project description

## Spanos Ioannis - o1150174

&nbsp;&nbsp;&nbsp;&nbsp;This assignment uses a pizzaria to simulate how a real program uses a set amount of resources to "produce" something in a multi-threaded scenario. Each new customer is represented by a new thread. The "operators" have to spend some time working on the customer's order (=thread), depending on the size of the order or a pre-set, pseudo-random amount of time.

## Restaurant to program translation

The restaurant is comprized of 3 types of "operators":

- **Packaging**

    &nbsp;&nbsp;&nbsp;&nbsp;This operator is only one, and works on one order at a time.
    This kind of operators are "acquired" by an order by the use of a single `mutex` variable. The critical section of the `mutex` includes all the work that has to be done while the operator works on an order.

    &nbsp;&nbsp;&nbsp;&nbsp;In this category we also have the `stats` and `printf` "resources". The critical section of the above mutexes includes writing to the terminal or the `stats` variable.

- **Call center, delivery and cook**
    &nbsp;&nbsp;&nbsp;&nbsp;Those jobs are done by multiple workers in parallel, but only one is needed for each order. In those cases, we use a variable that indicates how many of those operators are free. For those operators, we use `operators_acquire` to lock the above mentioned variable and reduce it by `1`. If there aren't any free, we `wait` for a signal. The critical section of this mutex is just the part where we decrease by one the variable that indicates how many of those "operators" are free. In order to free the operator, after it's done working on the order, we increase the aforementioned variable by one (`operators_release`). Then we send a `signal` to "awake" another waiting thread.

- **Ovens**
    &nbsp;&nbsp;&nbsp;&nbsp;Similar to the above operators we use `operators_acquire` and `operators_release` to decrease (or increase) the amount of ovens baking our pizzas from the time that the cook places the pizzas in them, until the packager takes the pizzas out of them. The only difference is that the amount of pizzas in each order defers, thus we need to use a `broadcast` to "awake" all other threads. That way, we make sure that the two orders that the cooks are working on awake, so that the cooks can put them in the ovens, once there are enough free ovens.

<div style="page-break-after: always;"></div>

## Functions' description

`sync_printf` is a thread safe way to write to the terminal. It uses the `<stdarg.h>` library to pass the arguments given to it, to a plain `printf`. The `printf` function is inside the critical section of `mutex_printf` to make sure that only one thread can print at a time.

`check_malloc` is a simple function that terminates the whole program if the `malloc` function didn't find enough space. Currently, memory allocations are done before the pizzaria starts "operating", thus exiting the program so soon doesn't create big problems.

`check_pthread_rc` similarly to `check_malloc` terminates the whole program if any of the `pthread_*` functions return a non-zero code. The limitation of this function is that there isn't any error handling, just error checking. In reality it would be similar to closing down the restaurant because someone stumbled on something. Or maybe it's time to "call the manager" if this happens.
The reason this compromise was made, instead of stopping a single thread is that we (="I") would have to handle each error differently and make sure all acquired operators are freed. It would also be more complicated if the error occurred inside the `operators_release` function.

`operators_acquire` decreases the amount of available operators of a given job, in a thread safe way. It uses `pthread_cond_wait` to wait for a signal if the available operators are less than the number we require.

`operators_release` increase the amount of available operators of a given job, in a thread safe way. Ovens are the only kind of resource that have a variable amount `acquired` or `released` operators, so if the mutex passed to the function is the one used for the ovens, we `broadcast` instead of `signal`ing. The limitation of this approach is scalability. If there were other fridges or cupboards for example, the code would start to look bad. A solution for `N` number of resources would be to create a list of `N` Kitchen_Station structs.

```c
struct Kitchen_Station {
    int avail_operators;
    pthread_mutex_t mutex_operators;
    pthread_cond_t cond_operators;
    unsigned int variable_operators; // used as a boolean
};
```

```c
// ...
if (variable_operators == 1) {
    int broadcast_rc = pthread_cond_broadcast(cond);
    check_pthread_rc("condition broadcast", broadcast_rc);
}
else {
    int signal_rc = pthread_cond_signal(cond);
    check_pthread_rc("condition signal", signal_rc);
}
// ...
```

## `pizza_order`

&nbsp;&nbsp;&nbsp;&nbsp;`pizza_order` uses the above mentioned functions to move through the different stages of pizza production, from the time the customer calls, until the deliverer gets back from the customer's house. Once we get a new call, we increase the `total_calls` variable, which is the only one that we need to log, even if the order fails.

&nbsp;&nbsp;&nbsp;&nbsp;We use `operators_acquire` to connect the customer to a person in the call center, wait for the order to be placed and the payment to go through. If the order fails we use `operators_release` to "hang up the phone" before exiting the thread.

&nbsp;&nbsp;&nbsp;&nbsp;Then, we use acquire a cook to prepare the order and free him only once the pizzas are placed in an oven. Once the packager is free, he empties the ovens and releases them. Then a deliverer gets the order to the customer and comes back, ready to go again.

&nbsp;&nbsp;&nbsp;&nbsp;At the end of the thread we edit the `stats` variables (inside a global struct), using a different mutex than the one for `total_calls`.

## `struct Thread_Info`

&nbsp;&nbsp;&nbsp;&nbsp;The function `rand_r` modifies the `seed` passed by reference. Hence, we need that to be thread safe as well. One approach would be to add a new mutex, but there would be a significant performance penalty. The current implementation passes a the struct `Thread_Info` as an argument to the `pthread_create` function. Thus, each thread modifies it's own seed and there is no need to use a mutex

<div style="page-break-after: always;"></div>

## Memory management

&nbsp;&nbsp;&nbsp;&nbsp;Something worth noting is that allocating the memory for all threads in the beginning, and freeing it at the end, is acceptable, only because the amount of orders a pizzaria can complete throughout a day, isn't big enough to require much memory. A more reasonable approach would be to create a thread pool which would allocate a pre-set number of threads in the beginning and reuse threads after an order is complete. This would require some scheduling, since in our current implementation might leave one of the first orders wait until all other are done. Making it even harder to free the first batch.

## Exit codes

| rc | meaning |
|--- |--- |
| 3 | Wrong number of arguments |
| 4 | Invalid argument |
| 5 | Memory allocation error |
| 6	| Unexpected error when creating new thread |
| 8 | Unexpected error when joining thread |
| 9 | `pthread` error |
| 66 | Unknown error [^1]|

[^1]: \* `stats.total_calls` should be equal to `N_Cust`, which can't be `0`.