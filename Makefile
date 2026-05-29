CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
TARGET = lsandbox

SRC = src/main.c src/sandbox.c src/utils.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

run:
	./$(TARGET) run -- /bin/echo hello