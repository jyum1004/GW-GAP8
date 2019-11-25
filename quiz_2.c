/*
Quiz #2:
With the documentation, write a program which copy two matrices (minimum size 64*64) of type unsigned short from L2 Memory to L1 Memory. 
Then produce the results of the two following operations by using the Cluster: (Please try to optimize the operations as efficient as possible.)
Two matrices add.
Two matrices multiply.
After all, send the result matrix from the L1 to the L2.
*/

#include <string.h>
/* PMSIS includes */
#include "pmsis.h"

/* Variables used. */
#define BUFFER_SIZE      ( 64*64 )

/* Task executed by cluster cores. */
/* Cluster main entry, executed by core 0. */
void cluster_malloc(void *arg)
{
    uint32_t coreid = pi_core_id(), clusterid = pi_cluster_id();
	unsigned short i;
    unsigned short *ptrL1_a = NULL;
	unsigned short *ptrL1_b = NULL;
	unsigned short *ptrL2_a = NULL;
	unsigned short *ptrL2_b = NULL;

    /* Alloc memory in L2. */
    pi_cl_alloc_req_t alloc_req = {0};
    pi_cl_free_req_t free_req = {0};
	
	pi_cl_L2_malloc((unsigned short) BUFFER_SIZE, &alloc_req);
	ptrL2_a = (unsigned short *) pi_cl_L2_malloc_wait(&alloc_req);
	if (ptrL2_a == NULL)
	{
		printf("Can't alloc L2_Memory for matrix_a\n");
		return;
	}
	/* set all values in the matrix to 0x02 for calculation test */
	memset(ptrL2_a, 0x02, sizeof(ptrL2_a));

	pi_cl_L2_malloc((unsigned short) BUFFER_SIZE, &alloc_req);
	ptrL2_b = (unsigned short *) pi_cl_L2_malloc_wait(&alloc_req);
	if (ptrL2_b == NULL)
	{
		printf("Can't alloc L2_Memory for matrix_b\n");
		pi_cl_L2_free(ptrL2_a, (unsigned short) BUFFER_SIZE, &free_req);
		pi_cl_L2_free_wait(&free_req);
		return;
	}
	/* set all values in the matrix to 0x03 for calculation test */
	memset(ptrL2_b, 0x03, sizeof(ptrL2_b));
	
    /* Alloc memory in L1. */
    /* Cluster core allocates in L1 memory. */
	ptrL1_a = (unsigned short *) pmsis_L1_malloc((unsigned short) BUFFER_SIZE);
	if (ptrL1_a == NULL)
	{
		printf("Can't alloc L1_Memory for matrix_a\n");
		pi_cl_L2_free(ptrL2_b, (unsigned short) BUFFER_SIZE, &free_req);
		pi_cl_L2_free_wait(&free_req);
		pi_cl_L2_free(ptrL2_a, (unsigned short) BUFFER_SIZE, &free_req);
		pi_cl_L2_free_wait(&free_req);
		return;
	}

	ptrL1_b = (unsigned short *) pmsis_L1_malloc((unsigned short) BUFFER_SIZE);
	if (ptrL1_b == NULL)
	{
		printf("Can't alloc L1_Memory for matrix_b\n");
		pi_cl_L2_free(ptrL2_b, (unsigned short) BUFFER_SIZE, &free_req);
		pi_cl_L2_free_wait(&free_req);
		pi_cl_L2_free(ptrL2_a, (unsigned short) BUFFER_SIZE, &free_req);
		pi_cl_L2_free_wait(&free_req);
		pmsis_L1_malloc_free(ptrL1_a, (unsigned short) BUFFER_SIZE);
		return;
	}
	
	/* copy the matrices */
	memcpy(ptrL1_a, ptrL2_a, sizeof(ptrL2_a));
	memcpy(ptrL1_b, ptrL2_b, sizeof(ptrL2_b));
	
	
	/* add the matrices */
	printf("Add two matrices\n");
	for(i=0; i<BUFFER_SIZE; i++) {
		ptrL2_a[i] = ptrL1_a[i] + ptrL1_b[i];
		/* print out for the calculation result */
		printf("%d ", ptrL1_a[i] + ptrL1_b[i]);
	}
	
	/* multiply the matrices */
	printf("\nMultiply two matrices\n");
	for(i=0; i<BUFFER_SIZE; i++) {
		ptrL2_b[i] = ptrL1_a[i] * ptrL1_b[i];
		/* print out for the calculation result */
		printf("%d ", ptrL1_a[i] * ptrL1_b[i]);
	}
	printf("\nDone.\n");
	
	/* free for allocated memory */
	pi_cl_L2_free(ptrL2_b, (unsigned short) BUFFER_SIZE, &free_req);
	pi_cl_L2_free_wait(&free_req);
	pi_cl_L2_free(ptrL2_a, (unsigned short) BUFFER_SIZE, &free_req);
	pi_cl_L2_free_wait(&free_req);

	pmsis_L1_malloc_free(ptrL1_b, (unsigned short) BUFFER_SIZE);
	pmsis_L1_malloc_free(ptrL1_a, (unsigned short) BUFFER_SIZE);
}

void test_cluster_malloc(void)
{
    uint32_t errors = 0;
    struct pi_device cluster_dev;
    struct pi_cluster_conf conf;

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

    /* Prepare cluster task and send it to cluster. */
    struct pi_cluster_task task = {0};
    task.entry = cluster_malloc;
    task.arg = NULL;

    #if defined(ASYNC)
    pi_task_t wait_task = {0};
    pi_task_block(&wait_task);
    pi_cluster_send_task_to_cl_async(&cluster_dev, &task, &wait_task);
    pi_task_wait_on(&wait_task);
    #else
    pi_cluster_send_task_to_cl(&cluster_dev, &task);
    #endif  /* ASYNC */

    /* Close cluster after end of computation. */
    pi_cluster_close(&cluster_dev);

    pmsis_exit(errors);
}

/* Program Entry. */
int main(void)
{
    printf("\n *** Two matrices calculation test using L1 and L2 memory ***\n\n");
    return pmsis_kickoff((void *) test_cluster_malloc);
}
