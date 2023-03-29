#define LAB4_EXTEND

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "timer.h"
#include "Lab4_IO.h"

#define EPSILON 0.00001
#define DAMPING_FACTOR 0.85

#define THRESHOLD 0.0001

struct node *nodehead;
int nodecount;
double *r, *last_r, *contribution;
double *my_r, *my_contribution;
double *recvcounts, *displs; // for MPI_Gatherv to specify how many elements to receive from each process
int i, j;
double damp_const;
int iterationcount = 0;
double start, end;
int processes, my_rank;
FILE *ip;

int nodes_per_process;
int my_nodestart, my_nodeend;
double error;

int read_input() {

    // Read the input file
    if ((ip = fopen("data_input_meta","r")) == NULL) {
        printf("Error opening the data_input_meta file.\n");
        return 1;
    }

    // Get node count
    fscanf(ip, "%d\n", &nodecount);
    fclose(ip);

    // Initialize the graph
    if (node_init(&nodehead, 0, nodecount)) return 1;

}

// Setup for a single worker
int init(int argc, char* argv[]) {

    // Init MPI
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &processes);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // Get graph and nodecount info from input file
    if (read_input()) return 1;

    // set our nodecount for each process (as well as ceiling of the nodes per process so we don't miss one)
    nodes_per_process = nodecount / processes + (nodecount % processes != 0);
    // set the start and end i for this process
    my_nodestart = nodes_per_process * my_rank;
    my_nodeend = nodes_per_process * (my_rank + 1) - 1;

    // Initialize r vectors
    r = malloc(nodecount * sizeof(double));
    last_r = malloc(nodecount * sizeof(double));
    // stores local r values for each process
    my_r = malloc(nodes_per_process * sizeof(double));

    // initialize contribution sum for j in D_i
    contribution = malloc(nodecount * sizeof(double));
    // stores local contribution values for each process
    my_contribution = malloc(nodes_per_process * sizeof(double));
    for ( i = 0; i < nodecount; ++i)
        contribution[i] = r[i] / nodehead[i].num_out_links * DAMPING_FACTOR;
    damp_const = (1.0 - DAMPING_FACTOR) / nodecount;

    // Set starting values for the shared and local rank vectors, r_i(0) = 1/N
    for ( i = 0; i < nodecount; ++i) {
        last_r[i] = 0.0;
        r[i] = 1.0 / nodecount;

        // set the rev_counts and displs for MPI_Gatherv
        recv_counts[i] = nodes_per_process;
        displs[i] = nodes_per_process * i;

        // // Update only the local nodes that this process handles
        // if (i < nodes_per_process) {
        //     my_r[i] = r[i];
        // }
    }

    return 0;
}

// One iteration of the page rank calculation
int iteration() {
    // calculate process's chunk of r_i
    for (i = my_nodestart; i < my_nodeend && i < nodecount; ++i) {
        my_r[i - my_nodestart] = damp_const;
        for (j = 0; j < nodehead[i].num_in_links; ++j) {
            my_r[i - my_nodestart] += contribution[nodehead[i].inlinks[j]];
        }
    }

    // update this chunk of r_i contribution for r_(i+1)
    for (i = my_nodestart; i < my_nodeend && i < nodecount; ++i) {
        my_contribution[i - my_nodestart] = my_r[i - my_nodestart] / nodehead[i].num_out_links * DAMPING_FACTOR;
    }

    // gather all chunks of r_i and update full r vector
    MPI_Allgatherv(my_r, nodes_per_process, MPI_DOUBLE, 
                    r, recvcounts, displs, MPI_DOUBLE, 
                    MPI_COMM_WORLD);
}

int main(int argc, char* argv[]) {

    // Setup each process and then start timing
    init(&argc, &argv);
    GET_TIME(start);

    do {
        // prep last_r and r for this iteration
        if (my_rank == 0) {
            ++iterationcount;
            vec_cp(r, last_r, nodecount);
        }

        // perform the iteration chunk
        iteration();
        
        // TODO: Calc error - rel_error(r, last_r, nodecount)
        // TODO: Update last_r - vec_cp(r, last_r, nodecount);

    } while (error >= EPSILON);

    GET_TIME(end);

    // Save output if master process
    if (my_rank == 0) {
        printf("Program converged at %d th iteration - Elapsed time %f - Process %d out of %d\n", iterationcount, end-start, my_rank + 1, processes);
        Lab4_saveoutput(r, nodecount, end-start);
    }

    // Cleanup    
    MPI_Finalize();
    node_destroy(nodehead, nodecount);
    free(contribution);
    free(my_contribution);
    free(r);
    free(last_r);
    free(my_r);
   
    return 0;
}