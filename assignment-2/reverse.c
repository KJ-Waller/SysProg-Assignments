#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

static int size = 1024;

void reverse(int fd) {
	char *input = malloc(size);

	int result = read(fd, input, size);

	// Recursive calls until entire file is read
	if (result > 0) {
		reverse(fd);

		// Print this chunk in reverse order
		result--;
		while (result >= 0) {
			printf("%c", input[result]);
			result--;
		}

		free(input);
		return;
	} else if (result == 0) {
		free(input);
		return;
	} else {
		fprintf(stderr, "ERROR: File could not be read\n");
		free(input);
		exit(1);
	}
	
}

int main(int argc, char ** argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: reverse <file_name_to_reverse>\n");
		return 1;
	}
	
	int fd = open(argv[1], O_RDONLY);

	if (fd == -1) {
		fprintf(stderr, "ERROR: File could not be opened\n");
		return 1;
	}

	reverse(fd);
	
	return 0;
}