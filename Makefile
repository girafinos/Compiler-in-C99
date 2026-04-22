CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Icompiler/include

SRC = compiler/src/main.c \
      compiler/src/lexer_v1.c \
      compiler/src/parser.c

OUT = lexer

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

run: all
	./$(OUT)

clean:
	rm -f $(OUT)