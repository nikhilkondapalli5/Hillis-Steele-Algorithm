CC = gcc
CFLAGS = -Wall -Wextra -O2  #Wall and Wextra are for compiler warning messages. O2 ensures optimization of compiled code    
TARGET = assignment

all: $(TARGET)

$(TARGET): assignment.c
	$(CC) $(CFLAGS) -o $(TARGET) assignment.c

clean:
	rm -f $(TARGET)
