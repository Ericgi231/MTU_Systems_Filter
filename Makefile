all: filter
filter: filter.c
	gcc -g -Wall -o filter filter.c
clean:
	rm filter
submission:
	tar czvf prog5.tgz Makefile filter.c
