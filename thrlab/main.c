#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "help.h"


/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in below.
 *
 * === User information ===
 * User 1: 
 * SSN:
 * User 2:
 * SSN:
 * === End User Information ===
 ********************************************************/

static void *barber_work(void *arg);

struct chairs
{
    struct customer **customer; /* Array of customers */
    int max;
    /* TODO: Add more variables related to threads */
    /* Hint: Think of the consumer producer thread problem */
    sem_t mutex;
    sem_t chair;
    sem_t barber;
};

struct barber
{
    int room;
    struct simulator *simulator;
};

struct simulator
{
    struct chairs chairs;
    
    pthread_t *barberThread;
    struct barber **barber;
};

typedef struct{
    int *buf; /* Buffer array */
    int n; /* Maximum number of slots */
    int front; /* buf[(front+1)%n] is first item */
    int rear; /* buf[rear%n] is last item */
    sem_t mutex; /* Protects accesses to buf*/
    sem_t slots; /* Counts available slots */
    sem_t items; /* Counts available items */
} sbuf_t;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

void Sem_init(sem_t *sem, int pshared, unsigned int value);
void P(sem_t *sem);
void V(sem_t *sem);
void unix_error(char *msg);


/**
 * Initialize data structures and create waiting barber threads.
 */
static void setup(struct simulator *simulator)
{
    struct chairs *chairs = &simulator->chairs;
    /* Setup semaphores*/
    chairs->max = thrlab_get_num_chairs();
    
    sem_init(&chairs->mutex, 0 , 1);
    sem_init(&chairs->chair, 0 , 1);
    sem_init(&chairs->barber, 0 , 0);

    /* Create chairs*/
    chairs->customer = malloc(sizeof(struct customer *) * thrlab_get_num_chairs());
    
    /* Create barber thread data */
    simulator->barberThread = malloc(sizeof(pthread_t) * thrlab_get_num_barbers());
    simulator->barber = malloc(sizeof(struct barber*) * thrlab_get_num_barbers());

    /* Start barber threads */
    struct barber *barber;
    for (unsigned int i = 0; i < thrlab_get_num_barbers(); i++) {
    barber = calloc(sizeof(struct barber), 1);
    barber->room = i;
    barber->simulator = simulator;
    simulator->barber[i] = barber;
    pthread_create(&simulator->barberThread[i], 0, barber_work, barber);
    pthread_detach(simulator->barberThread[i]);
    }
}

/**
 * Free all used resources and end the barber threads.
 */
static void cleanup(struct simulator *simulator)
{
    /* Free chairs */
    free(simulator->chairs.customer);

    /* Free barber thread data */
    free(simulator->barber);
    free(simulator->barberThread);
}

/**
 * Called in a new thread each time a customer has arrived.
 */
static void customer_arrived(struct customer *customer, void *arg)
{
    struct simulator *simulator = arg;
    struct chairs *chairs = &simulator->chairs;

    sem_init(&customer->mutex, 0, 0);

    /* TODO: Accept if there is an available chair */
    sem_wait(&chairs->chair);   
    sem_wait(&chairs->mutex);   //lock mutex to protect seat changes
    thrlab_accept_customer(customer);
    chairs->customer[0] = customer;
    sem_post(&chairs->mutex);
    sem_post(&chairs->barber);  //increase number for barber

    sem_wait(&customer->mutex);

    /* TODO: Reject if there are no available chairs */
    thrlab_reject_customer(customer);
}

static void *barber_work(void *arg)
{
    struct barber *barber = arg;
    struct chairs *chairs = &barber->simulator->chairs;
    struct customer *customer = 0; /* TODO: Fetch a customer from a chair */

    /* Main barber loop */
    while (true) {
    /* TODO: Here you must add you semaphores and locking logic */
    sem_wait(&chairs->barber);

    sem_wait(&chairs->mutex);
    customer = chairs->customer[0]; /* TODO: You must choose the customer */
    thrlab_prepare_customer(customer, barber->room);
    sem_post(&chairs->mutex);
    sem_post(&chairs->chair);

    thrlab_sleep(5 * (customer->hair_length - customer->hair_goal));
    thrlab_dismiss_customer(customer, barber->room);

    sem_post(&customer->mutex);
    }
    return NULL;
}

/* Create an empty, bounded, shared FIFO buffer with nslots */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = calloc(n, sizeof(int));
    sp->n= n; /* Buffer holds max of nitems */
    sp->front = sp->rear = 0; /* Empty buffer ifffront == rear */
    Sem_init(&sp->mutex, 0, 1); /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n); /* Initially, bufhas nempty slots */
    Sem_init(&sp->items, 0, 0); /* Initially, bufhas zero items */
}
/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp)
{
    free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots); /* Wait for available slot */
    P(&sp->mutex); /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item; /* Insert the item */
    V(&sp->mutex); /* Unlock the buffer */
    V(&sp->items); /* Announce available item */
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items); /* Wait for available item */
    P(&sp->mutex); /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)]; /* Remove the item */
    V(&sp->mutex); /* Unlock the buffer */
    V(&sp->slots); /* Announce available slot */
    return item;
}

void Sem_init(sem_t *sem, int pshared, unsigned int value) 
{
    if (sem_init(sem, pshared, value) < 0)
    unix_error("Sem_init error");
}

void P(sem_t *sem) 
{
    if (sem_wait(sem) < 0)
    unix_error("P error");
}

void V(sem_t *sem) 
{
    if (sem_post(sem) < 0)
    unix_error("V error");
}

void unix_error(char *msg) /* unix-style error */
{
    //fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

int main (int argc, char **argv)
{
    struct simulator simulator;

    thrlab_setup(&argc, &argv);
    setup(&simulator);

    thrlab_wait_for_customers(customer_arrived, &simulator);

    thrlab_cleanup();
    cleanup(&simulator);

    return EXIT_SUCCESS;
}
