CC = gcc
CFLAGS = -Wall -O3 -fopenmp -I./include
LDFLAGS = -lcurl -lm -fopenmp

SRC = src/main.c src/twelvedata.c src/strategy.c src/risk.c src/cJSON.c
OBJ = $(SRC:.c=.o)
TARGET = twelvedata_bot

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET) twelvedata_bot.exe