CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude

SRC = src/main.c src/lexer_v1.c
OUT = lexer

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

run: all
	./$(OUT)

clean:
	rm -f $(OUT)