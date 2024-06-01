#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
	int sleep_sec;
	if (argc < 2){
		fprintf(2, "Usage: sleep seconds\n");
		exit(1);
	}

	sleep_sec = atoi(argv[1]);
	if (sleep_sec > 0){
		sleep(sleep_sec);
	} else {
		fprintf(2, "Invalid interval %s\n", argv[1]);
        exit(1);
	}
	exit(0);
}