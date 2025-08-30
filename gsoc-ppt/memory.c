#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main() {
	long long num_elements = 1024 * 1024 * 100; // Allocate 100 million integers
	int *data_array;// Seed the random number generator
	srand(time(NULL));

	// Allocate memory
	data_array = (int *)malloc(num_elements * sizeof(int));

	// Check for allocation failure
	if (data_array == NULL) {
		perror("Failed to allocate memory");
		return 1; // Indicate an error
	}
	printf("Memory allocated successfully. Filling with random values...\n");

	// Fill the allocated memory with random values
	for (long long i = 0; i < num_elements; i++) {
		data_array[i] = rand(); // Assign a random integer
	}

	printf("Memory filled. Displaying a few sample values:\n");

	// Display a few sample values
	for (int i = 0; i < 1000000; i++) {
		printf("data_array[%d] = %d\n", i, data_array[i]);
	}// Free the allocated memory
	free(data_array);
	printf("Memory freed.\n");

	return 0;
}
