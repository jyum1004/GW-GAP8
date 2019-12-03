/*
Quiz #2:
With the documentation, write a program which copy two matrices (minimum size 64*64) 
of type unsigned short from L2 Memory to L1 Memory. 
Then produce the results of the two following operations by using the Cluster: 
(Please try to optimize the operations as efficient as possible.)
Two matrices add.
Two matrices multiply.
After all, send the result matrix from the L1 to the L2.
*/

/* PMSIS includes */
#include "pmsis.h"

/* Variables used. */
#define BUFFER_SIZE      ( 64*64 )

static unsigned short L2_in[BUFFER_SIZE*2]; /* two buffers; matrix_A + matrix_B */
static unsigned short L2_result[BUFFER_SIZE*2]; /* to be stored calculation result plus and multiply calculation result respectively */

/* Task executed by cluster cores. */
void cluster_dma_calc(void *arg)
{
	uint32_t i;
    unsigned short *L1_buffer = (unsigned short *) arg;
	unsigned short *L1_result;
    uint32_t coreid = pi_core_id(), start = 0, end = 0;

    /* alloc a buffer for add and multiply calculation results */
	L1_result = (unsigned short *) pmsis_l1_malloc((uint32_t) BUFFER_SIZE*2); 
    if (L1_result == NULL)
    {
        printf("L1_result alloc failed !\n");
        pmsis_exit(-2);
    }
	
    /* Core 0 of cluster initiates DMA transfer from L2 to L1. */
    if (!coreid)
    {
        printf("Core %d requesting DMA transfer from l2_in to l1_buffer.\n", coreid);
        pi_cl_dma_copy_t copy;
        copy.dir = PI_CL_DMA_DIR_EXT2LOC;
        copy.merge = 0;
        copy.size = (uint16_t) (BUFFER_SIZE*2);
        copy.id = 0;
        copy.ext = (uint32_t) L2_in;
        copy.loc = (uint32_t) L1_buffer;

        pi_cl_dma_memcpy(&copy);
        pi_cl_dma_wait(&copy);
        printf("Core %d : Transfer from L2_in to L1_buffer done.\n", coreid);
    }

    start = (coreid * ((uint32_t) BUFFER_SIZE / pi_nb_cluster_cores()));
    end = (start - 1 + ((uint32_t) BUFFER_SIZE / pi_nb_cluster_cores()));

    /* Barrier synchronisation before starting to compute. */
    pi_cl_team_barrier(0);
    /* Each core computes on specific portion of buffer. */
    for (i=start; i<=end; i++)
    {
		/* add two matrices */
        L1_result[i] = (L1_buffer[i] + L1_buffer[(uint32_t)BUFFER_SIZE+i]);
		
		/* multiply two matrices */
        L1_result[(uint32_t)BUFFER_SIZE+i] = (L1_buffer[i] * L1_buffer[(uint32_t)BUFFER_SIZE+i]);
    }

    /* Barrier synchronisation to wait all cores. */
    pi_cl_team_barrier(0);

    /* Core 0 of cluster initiates DMA transfer from L1 to L2. */
    if (!coreid)
    {
        printf("Core %d requesting DMA transfer from L1_buffer to L2_result.\n", coreid);
        pi_cl_dma_copy_t copy;
        copy.dir = PI_CL_DMA_DIR_LOC2EXT;
        copy.merge = 0;
        copy.size = (uint16_t) BUFFER_SIZE*2;
        copy.id = 0;
        copy.ext = (uint32_t)L2_result;
        copy.loc = (uint32_t)L1_result;

        pi_cl_dma_memcpy(&copy);
        pi_cl_dma_wait(&copy);
        printf("Core %d : Transfer done.\n", coreid);
    }
	pmsis_l1_malloc_free(L1_result, (uint32_t) BUFFER_SIZE*2);
}

/* Cluster main entry, executed by core 0. */
void master_entry(void *arg)
{
    /* Task dispatch to cluster cores. */
    pi_cl_team_fork(pi_nb_cluster_cores(), cluster_dma_calc, arg);
}

void cluster_malloc_dma(void)
{
    uint32_t i;
	uint32_t errors = 0;
    struct pi_device cluster_dev;
    struct pi_cluster_conf conf;
	unsigned short *L1_buffer;

    /* L2 Array Init. for matrix A */
    for (i=0; i<(uint32_t) BUFFER_SIZE; i++)
    {
        L2_in[i] = 2;
        L2_result[i] = 0;
    }
	/* L2 Array Init. for matrix B */
    for (i=(uint32_t)BUFFER_SIZE; i<(uint32_t) (BUFFER_SIZE*2); i++)
    {
        L2_in[i] = 3;
        L2_result[i] = 0;
    }

    /* Init cluster configuration structure. */
    pi_cluster_conf_init(&conf);
    conf.id = 0;                /* Set cluster ID. */
    /* Configure & open cluster. */
    pi_open_from_conf(&cluster_dev, &conf);
    if (pi_cluster_open(&cluster_dev))
    {
        printf("Cluster open failed !\n");
        pmsis_exit(-1);
    }

    L1_buffer = (unsigned short *) pmsis_l1_malloc((uint32_t) BUFFER_SIZE*2); /* two results for add and multiply */
    if (L1_buffer == NULL)
    {
        printf("L1_buffer alloc failed !\n");
        pmsis_exit(-2);
    }

    /* Prepare cluster task and send it to cluster. */
    struct pi_cluster_task *task = (struct pi_cluster_task *) pmsis_l2_malloc(sizeof(struct pi_cluster_task));
    if (task == NULL)
    {
        printf("Cluster task alloc failed !\n");
        pmsis_l1_malloc_free(L1_buffer, (uint32_t) BUFFER_SIZE*2);
        pmsis_exit(-3);
    }
    memset(task, 0, sizeof(struct pi_cluster_task));
    task->entry = master_entry;
    task->arg = L1_buffer;

    printf("Sending task.\n");
    #if defined(ASYNC)
    pi_task_t wait_task = {0};
    pi_task_block(&wait_task);
    pi_cluster_send_task_to_cl_async(&cluster_dev, task, &wait_task);
    printf("Wait end of cluster task\n");
    pi_task_wait_on(&wait_task);
    printf("End of cluster task\n");
    #else
    pi_cluster_send_task_to_cl(&cluster_dev, task);
    #endif  /* ASYNC */
    pmsis_l2_malloc_free(task, sizeof(struct pi_cluster_task));
    pmsis_l1_malloc_free(L1_buffer, (uint32_t) BUFFER_SIZE*2);

    pi_cluster_close(&cluster_dev);

    pmsis_exit(errors);
}

/* Program Entry. */
int main(void)
{
    printf("\n\n\t *** Two matrices calculation test efficiently"); 
	printf(" using DMA for L1 and L2 memory ***\n\n");
    return pmsis_kickoff((void *) cluster_malloc_dma);
}
