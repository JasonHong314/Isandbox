CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
LDFLAGS = -lseccomp
TARGET = lsandbox

SRC = src/main.c src/sandbox.c src/namespace.c src/overlay.c src/cgroup.c src/seccomp_filter.c src/manage.c src/utils.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)

run:
	./$(TARGET) run -- /bin/echo hello