
# C Compiler
CC = gcc
# C Compiler flags
CFLAGS = -std=gnu11 -g -Wall -Wextra -Wpedantic -Wshadow -pthread
#Target executable name 
TARGET = loadbalancer
#QueueTarget
QUEUE = requestQueue
SERVERSOBJ = serverMinHeap

all: $(TARGET)

$(TARGET): $(TARGET).o $(QUEUE).o $(SERVERSOBJ).o
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).o $(QUEUE).o $(SERVERSOBJ).o

$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -c $(TARGET).c

$(QUEUE).o: $(QUEUE).c $(QUEUE).h
	gcc $(CFLAGS) -c $(QUEUE).c

$(SERVERSOBJ).o: $(SERVERSOBJ).c $(SERVERSOBJ).h
	gcc $(CFLAGS) -c $(SERVERSOBJ).c
clean:
	rm *.o $(TARGET) $(TARGET).o $(QUEUE).o $(SERVERSOBJ).o

spotless:
	clean rm $(TARGET).exe


