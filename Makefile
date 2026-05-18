CC=gcc
CFLAGS=-Wall -Wextra -std=c11 `pkg-config --cflags gtk4`
LIBS=`pkg-config --libs gtk4`

all: gui search index_builder

gui: src/GUI.c src/common.c
	$(CC) $(CFLAGS) src/GUI.c src/common.c -o gui $(LIBS)

search: src/searcher.c src/search_name.c src/search_publisher.c  src/common.c
	$(CC) $(CFLAGS) src/searcher.c src/search_name.c src/search_publisher.c src/common.c -o searcher

index_builder: src/build_index.c src/common.c
	$(CC) $(CFLAGS) src/build_index.c src/common.c -o index_builder 

clean:
	rm -f gui searcher index_builder