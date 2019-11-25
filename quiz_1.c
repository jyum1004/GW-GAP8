/*
Quiz #1:
With the documentation, write a program which would make all the cores says “HelloWorld world”. 
*/


/* PMSIS includes */
#include "pmsis.h"

/* Variables used. */
struct pi_device uart;
char HelloWorld[25];
PI_L1 spinlock_t spinlock;
PI_L1 int32_t tas_addr;

/* Task executed by cluster cores. */
void cluster_helloworld(void *arg)
{
    uint32_t core_id = pi_core_id();

    cl_sync_spinlock_take(&spinlock);
    sprintf(HelloWorld, "Hello World at core %d!\n", core_id);
    printf(HelloWorld);
    cl_sync_spinlock_release(&spinlock);
}

/* Cluster main entry, executed by core 0. */
void cluster_delegate(void *arg)
{
    /* Task dispatch to cluster cores. */
    cl_sync_init_spinlock(&spinlock, &tas_addr);
    pi_cl_team_fork(pi_nb_cluster_cores(), cluster_helloworld, arg);
}

void cluster_run(void)
{
    uint32_t errors = 0;

    struct pi_device cluster_dev = {0};
    struct pi_cluster_conf cl_conf = {0};

    /* Init cluster configuration structure. */
    pi_cluster_conf_init(&cl_conf);
    cl_conf.id = 0;                /* Set cluster ID. */
    /* Configure & open cluster. */
    pi_open_from_conf(&cluster_dev, &cl_conf);
    if (pi_cluster_open(&cluster_dev))
    {
        printf("Cluster open failed !\n");
        pmsis_exit(-1);
    }

    /* Prepare cluster task and send it to cluster. */
    struct pi_cluster_task cl_task = {0};
    cl_task.entry = cluster_delegate;
    cl_task.arg = NULL;

    pi_cluster_send_task_to_cl(&cluster_dev, &cl_task);

    pi_cluster_close(&cluster_dev);

    printf("Done.\n");
    pmsis_exit(errors);
}

/* Program Entry. */
int main(void)
{
    printf("*** HelloWorld print for all the cores. ***\n\n");
    return pmsis_kickoff((void *) cluster_run);
}

