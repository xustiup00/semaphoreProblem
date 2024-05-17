PROJECT=proj2
CFLAGS=-std=gnu99 -pthread -Wall -Wextra -Werror -pedantic 
CC=gcc
RM=rm -f

$(PROJECT): $(PROJECT).c
	$(CC) $(CFLAGS) -o $(PROJECT) $(PROJECT).c

clean:
	$(RM) *.o $(PROJECT) proj2.out