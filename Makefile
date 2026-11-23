CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -pthread
TARGET = vm
SRC = vm.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean