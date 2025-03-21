/*
Name: Nikhil Sesha Sai Kondapalli
Net ID: nxk240025
Course: CS 5348.004 - Operating Systems
*/

//Including the necessary headers required for this program

#include <stdio.h>  // For fprintf, fscanf, fopen, fclose, perror functions     
#include <stdlib.h> // For exit, atoi, malloc, free     
#include <unistd.h> // For fork, pid_t  (POSIX OS API)              
#include <sys/ipc.h>    // For IPC_CREAT, IPC_PRIVATE   (Inter-Process Communication)
#include <sys/shm.h>        // For shmget, shmat, shmdt, shmctl functions (Shared Memory)
#include <sys/wait.h>   // For wait function to wait for child processes
#include <errno.h>  // For Error handling

/* Creating Barrier function using shared memory.
Each process has an entry in wall[]. When a process reaches the barrier,
it increments its counter and busy-waits until all processes have reached
at least that count. */
void barrier(int id, int m, volatile int *wall) {       //volatile keyword is used to prevent compiler optimization
    wall[id]++;                         // Increment the counter for this process.
    int my_val = wall[id];              // Read the current value of the counter.
    int j, isReady;                     // Variables to check if all processes have reached the barrier.
    do {            
        isReady = 1;                    // Assume all processes have reached the barrier.
        for (j = 0; j < m; j++) {       // Check if all processes have reached the barrier.
            if (wall[j] < my_val) {    // Check if any process has not reached the barrier.
                isReady = 0;          // If any process has not reached the barrier, set isReady to 0.
                break;
            }
        }
    } while (!isReady);               // Busy-wait until all processes have reached the barrier.
}

// Main function with command-line arguments.
int main(int argc, char *argv[]) {    
    if (argc != 5) {                  // Check if the number of arguments is correct.
        fprintf(stderr, "Error: command-line arguments form for %s: n m input_file output_file\n", argv[0]);    // Print the correct command-line arguments form.
        exit(EXIT_FAILURE);      // Exit the program if the number of arguments is incorrect.
    }

    // Parse and validate command-line arguments as integers.
    int n = atoi(argv[1]);       
    int m = atoi(argv[2]);       
    if(n <= 0 || m <= 0) {      // Check if the integers n and m are positive.
        fprintf(stderr, "Error: n and m must be positive integers.\n");
        exit(EXIT_FAILURE);
    }

    // Open the input file in read mode and check if it exists.
    FILE *fin = fopen(argv[3], "r");    
    if(fin == NULL) {
        perror("Error opening input file. Ensure the file is readable and exists.");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for temporary array to store n integers.        
    int *temp_array = malloc(n * sizeof(int));
    if (temp_array == NULL) {    // Check if memory allocation failed.
        perror("Memory allocation failed");
        fclose(fin);
        exit(EXIT_FAILURE);
    }

    // Read n integers from the input file into the temporary array.
    for (int i = 0; i < n; i++) {   
        if (fscanf(fin, "%d", &temp_array[i]) != 1) {   // Read an integer from the input file.
            fprintf(stderr, "Input file does not have enough integers as given in the argument n.\n");  // Check if the input file contains enough integers.
            free(temp_array);
            fclose(fin);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fin);

    // Calculate the maximum number of iterations required for Hillis and Steele's algorithm. 
    int max_iter = 0;
    while ((1 << max_iter) < n)    // max_iter is the smallest power of 2 greater than or equal to n for log(n) iterations.
        max_iter++;            // Increment the maximum number of iterations.

    // Allocate two shared memory segments of size n integers each for O(n) complexity of the program.
    int shm_size = n * sizeof(int);
    int shm_id1 = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666);  // Create a shared memory segment for the first array.
    if (shm_id1 < 0) {          // Check if shared memory allocation failed.
        perror("shmget failed for shared array 1");
        free(temp_array);
        exit(EXIT_FAILURE);
    }
    int shm_id2 = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666);  // Create a shared memory segment for the second array.
    if (shm_id2 < 0) {
        perror("shmget failed for shared array 2"); 
        free(temp_array);
        shmctl(shm_id1, IPC_RMID, NULL);        // Remove the shared memory segment 1.
        exit(EXIT_FAILURE);
    }
    int *arr1 = (int *) shmat(shm_id1, NULL, 0);    // Attach shared memory segment 1 to the process.
    if (arr1 == (void *) -1) {          // Check if shared memory attachment failed.
        perror("shmat failed for shared array 1");
        free(temp_array);
        shmctl(shm_id1, IPC_RMID, NULL);        // Remove the shared memory segment 1.
        shmctl(shm_id2, IPC_RMID, NULL);        // Remove the shared memory segment 2.
        exit(EXIT_FAILURE);
    }
    int *arr2 = (int *) shmat(shm_id2, NULL, 0);    // Attach shared memory segment 2 to the process.
    if (arr2 == (void *) -1) {        // Check if shared memory attachment failed.
        perror("shmat failed for shared array 2");
        free(temp_array);
        shmdt(arr1);
        shmctl(shm_id1, IPC_RMID, NULL);        // Remove the shared memory segment 1.
        shmctl(shm_id2, IPC_RMID, NULL);        // Remove the shared memory segment 2.
        exit(EXIT_FAILURE);
    }

    // Initialize array1 with input data.
    for (int i = 0; i < n; i++) {
        arr1[i] = temp_array[i];            // Copy the integers from the temporary array to the shared memory segment 1.
    }
    free(temp_array);   // Free the temporary array memory since it is copied into array 1.

    // Allocate new shared memory for the barrier array.
    int shm_barrier = shmget(IPC_PRIVATE, m * sizeof(int), IPC_CREAT | 0666);       // Create a shared memory segment of size m(processes) for the barrier.
    if (shm_barrier < 0) {                  // Check if shared memory allocation failed.
        perror("shmget failed for barrier array");      
        shmdt(arr1);    // Detach shared memory for array 1.
        shmdt(arr2);    // Detach shared memory for array 2.
        shmctl(shm_id1, IPC_RMID, NULL);    // Remove shared memory for array 1.
        shmctl(shm_id2, IPC_RMID, NULL);    // Remove shared memory for array 2.
        exit(EXIT_FAILURE);
    }
    volatile int *wall = (volatile int *) shmat(shm_barrier, NULL, 0);      // Attach shared memory segment for the barrier.
    if (wall == (void *) -1) {        // Check if shared memory attachment failed.
        perror("shmat failed for barrier array");
        shmdt(arr1);        // Detach shared memory for array 1.
        shmdt(arr2);        // Detach shared memory for array 2.
        shmctl(shm_id1, IPC_RMID, NULL);    // Remove shared memory for array 1.
        shmctl(shm_id2, IPC_RMID, NULL);    // Remove shared memory for array 2.
        exit(EXIT_FAILURE);
    }
    // Initialize barrier array to 0 for m processes.
    for (int i = 0; i < m; i++) {
        wall[i] = 0;    
    }

    // Fork m worker processes.
    for (int proc = 0; proc < m; proc++) {  // Loop through the number of processes.
        pid_t pid = fork();   // Fork a new process.
        if (pid < 0) {      // Check if fork failed.
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {  // Child process
            // Local pointers for storing two arrays: initially, curr points to arr1 and next to arr2.
            int *curr = arr1;
            int *next = arr2;

            // Divide work among processes.
            int blocksize = n / m;  // Number of elements to process per process.
            int remainder = n % m;  // Number of processes that will process one extra element.
            int start, end;        // Start and end indices for the current process.
            if (proc < remainder) {   // Check if the process has to process one extra element.
                start = proc * (blocksize + 1);   
                end = start + blocksize; // inclusive (blocksize+1 elements)
            } else {
                start = proc * blocksize + remainder; // Start index for the process.
                end = start + blocksize - 1;      // End index for the process.
            }

            // Perform Hillis and Steele's algorithm iterations.
            for (int p = 1; p <= max_iter; p++) {   // Loop through the maximum number of iterations.
                int shift = 1 << (p - 1);   // Calculate the array index shift value for the current iteration.
                for (int i = start; i <= end && i < n; i++) {   
                    if (i < shift)  
                        next[i] = curr[i];  // If i < shift, copy the current value to the next array.
                    else
                        next[i] = curr[i] + curr[i - shift];    // Else, add the current value with the value at i-shift.
                }
                // Wait until all processes finish the computation for this iteration.
                barrier(proc, m, wall); 

                // Swap pointers so that next becomes the new current array for next iteration.
                int *temp = curr;
                curr = next;
                next = temp;

                // Wait until all processes have swapped pointers before next iteration.
                barrier(proc, m, wall);
            }
            // Detach shared memory arrays and exit child process.
            shmdt((void*) arr1);
            shmdt((void*) arr2);
            shmdt((void*) wall);        
            exit(EXIT_SUCCESS);
        }
    }

    // Parent waits for all worker processes to finish.
    for (int i = 0; i < m; i++) {
        wait(NULL);
    }

    /* Determine the final result:
        Since we started with curr = arr1 and did a pointer swap at the end of each iteration,
        after max_iter iterations the final result is in:
        arr1 if max_iter is even, or arr2 if max_iter is odd. */
    int *final_result = (max_iter % 2 == 0) ? arr1 : arr2;  

    // Open output file to write the final prefix sum.
    FILE *fout = fopen(argv[4], "w");
    if (fout == NULL) {
        perror("Error opening output file");    // Check if the output file could not be opened.
        shmdt((void*) arr1);    // Detach shared memory for arr1, arr2, barrier.
        shmdt((void*) arr2);
        shmdt((void*) wall);
        shmctl(shm_id1, IPC_RMID, NULL);    // Remove shared memory for arr1, arr2, barrier.
        shmctl(shm_id2, IPC_RMID, NULL);
        shmctl(shm_barrier, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; i++) {   // Write the final result to the output file.
        fprintf(fout, "%d ", final_result[i]);
    }
    fprintf(fout, "\n");
    fclose(fout);

    // Clean up: detach and remove shared memory segments.
    shmdt((void*) arr1);    // Detach shared memory for arr1, arr2, barrier.
    shmdt((void*) arr2);
    shmdt((void*) wall);
    shmctl(shm_id1, IPC_RMID, NULL);    // Remove shared memory for arr1, arr2, barrier.
    shmctl(shm_id2, IPC_RMID, NULL);
    shmctl(shm_barrier, IPC_RMID, NULL);

    return 0;   
}
