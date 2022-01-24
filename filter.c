//Add any includes you require.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>

//enums
enum mode{inm, iom, com};

//global vars
enum mode mode = com;
int maxLines = 20;
int countLines = 0;
pid_t myBaby;
int toBaby, fromBaby, fromBabyErr;
int more = 0, modeChange = 0;
int reading = 0;

void command(char *buff, int size){
	modeChange = 1;
	char num[256];
	switch (buff[1])
	{
	case 'i':
		mode = inm;
		break;
	case 'o':
		mode = iom;
		reading = 1;
		break;
	case 'c':
		mode = com;
		break;
	case 'm':
		for (int i = 0; i < size-3; i++) {
			num[i] = buff[i+3];
		}
		maxLines = atoi(num);
		break;
	case 'k':
		for (int i = 0; i < size-3; i++) {
			num[i] = buff[i+3];
		}
		kill(myBaby, atoi(num));
		break;
	default:
		printf("Invalid command #\n");
		modeChange = 0;
		break;
	}
}

/* main - implementation of filter
 * In this project, you will be developing a filter program that forks
 * a child and then executes a program passed as an argument and intercepts 
 * all output and input from that program. The syntax for the execution of 
 * the filter program is given by:
 * 
 * 	filter <program name> <arguments to program>
 *
 * The following commands must be implemented:
 * 	//           : Pass the character '/' as input to the program.
 * 	/i           : Go to input only mode.
 * 	/o           : Go to input/output mode.
 * 	/c           : Go to command mode.
 * 	/m <integer> : Set the max-text, maximum number of lines to be displayed.
 * 	/k <integer> : Send the child process indicated signal.
 *
 * See the spec for details.
 * 
 * After receiving each command, the program should output a prompt indicating 
 * the current mode and if there is more data to be displayed. 
 * The prompt syntax is :
 *
 * 	<pid> m <more> #
 *
 * where <pid> is the process id of the child process, m is the current mode (
 * i (input only), c (command), o(input/output)), optional <more> is the text "more" 
 * if there is data available to be displayed, and lastly the pound character.
 */
int main(int argc, char *argv[]){
	//check for correct usage
	if (argc < 2) {
		printf("./filter <program> [arguments]\n");
		return 0;
	}

	//create pipes
	int in[2];
	int out[2];
	int err[2];
	pipe(in);
	pipe(out);
	pipe(err);

	//make child
	pid_t pid = fork();
	myBaby = pid;
	if (pid == 0) { 
		//replace childs io with pipes
		dup2(in[0], STDIN_FILENO); //parents in[1] goes to childs in[0]
		dup2(out[1], STDOUT_FILENO); //parents out[0] is taken from childs out[1]
		dup2(err[1], STDERR_FILENO); //parents err[0] is taken from childs err[1]

		close(in[1]);
		close(out[0]);
		close(err[0]);

		//exec program
		execvp(argv[1],&argv[1]);

		//fallthrough
		printf("Failed to open program.\n");
		exit(0);
	} 

	//parent
	close(in[0]);
	close(out[1]);
	close(err[1]);
	toBaby = in[1];
	fromBaby = out[0];
	fromBabyErr = err[0];

	fd_set rfds;
	int res, sel, status;
	char ch;
	struct timeval t;
	t.tv_usec = 50;

	for (;;) {
		//reset fd set
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO ,&rfds);
		FD_SET(fromBaby, &rfds);
		FD_SET(fromBabyErr ,&rfds);

		//select
		sel = select(err[1] + 1, &rfds, NULL, NULL, &t);

		//on child output
		if (reading && FD_ISSET(fromBaby, &rfds)) {
			countLines = 0;
			//while has chars to output
			while (FD_ISSET(fromBaby, &rfds)) {
				//read and print char
				read(fromBaby, &ch, 1);
				printf("%c out\n", ch);
				
				//check if char is new line
				if (ch == '\n') {
					//increase line count
					countLines++;

					//check if can read again without blocking
					FD_SET(STDIN_FILENO ,&rfds);
					FD_SET(fromBaby, &rfds);
					FD_SET(fromBabyErr ,&rfds);
					select(fromBabyErr + 1, &rfds, NULL, NULL, &t);

					//break if more data than can print
					if (countLines >= maxLines) {
						if (FD_ISSET(fromBaby, &rfds)) {
							more = 1;
						}
						break;
					}
				}
			}
			reading = 0;
		}

		//if /o is called with no output from child
		if (sel <= 0 && mode == iom && !FD_ISSET(fromBaby, &rfds)) {
			reading = 0;
		}

		//on standard in
		if (FD_ISSET(STDIN_FILENO, &rfds)) {
			//read standard in
			char buff[256];
			res = read(0, buff, 256);

			//if command, handle
			if (buff[0] == '/' || mode == com) {
				command(buff, res);
			}
			else {
				write(toBaby, buff, res);
			}
		}

		//check if child has died
		if (waitpid(myBaby, &status, WNOHANG) > 0) {
			fprintf(stderr, "The child %d has terminated with code %d\n", myBaby, status);
		}

		//on standard error
		while (!reading && FD_ISSET(fromBabyErr, &rfds) && waitpid(myBaby, &status, WNOHANG) == 0) {
			//stop if child is dead
			printf("err");
			//read and print char
			read(fromBabyErr, &ch, 1);
			fprintf(stderr, "%c", ch);

			//check if can read again without blocking
			FD_SET(STDIN_FILENO ,&rfds);
			FD_SET(fromBaby, &rfds);
			FD_SET(fromBabyErr ,&rfds);
			select(fromBabyErr + 1, &rfds, NULL, NULL, &t);
		}

		//print mode change info
		if (!reading && modeChange) {
			modeChange = 0;
			char m;
			switch (mode)
			{
			case inm: m = 'i'; break;
			case iom: m = 'o'; break;
			case com: m = 'c'; break;
			}
			printf("%d %c", myBaby, m);
			if(more){
				printf(" more");
				more = 0;
			}
			printf(" #\n");
		}
	}
	
	return 0;
}
