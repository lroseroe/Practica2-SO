CC=gcc
CFLAGS=-Wall -Wextra -std=c11 `pkg-config --cflags gtk4`
LIBS=`pkg-config --libs gtk4`

all: gui search index_name index_publisher

gui: src/GUI.c src/common.c
	$(CC) $(CFLAGS) src/GUI.c src/common.c -o gui $(LIBS)

search: src/searcher.c src/search_name.c src/search_publisher.c  src/common.c
	$(CC) $(CFLAGS) src/searcher.c src/search_name.c src/search_publisher.c src/common.c -o searcher

index_name: src/build_name_index.c src/common.c
	$(CC) $(CFLAGS) src/build_name_index.c src/common.c -o index_name 

index_publisher: src/build_publisher_index.c src/common.c
	$(CC) $(CFLAGS) src/build_publisher_index.c src/common.c -o index_publisher

clean:
	rm -f gui searcher index_name