CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -pthread
LDFLAGS = -lpthread
TARGET  = scraper
SRC     = scraper.c

.PHONY: all clean run install-deps

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

# Install curl CLI if needed (Debian/Ubuntu)
install-deps:
	sudo apt-get install -y curl

clean:
	rm -f $(TARGET)
	rm -rf scraped_pages/
