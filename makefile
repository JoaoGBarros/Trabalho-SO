CC = gcc
CFLAGS = -Wall -pthread
TARGET = barbeariaDeHilzer
SRC = barbeariaDeHilzer.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET) $(ARGS)

clean:
	rm -f $(TARGET)