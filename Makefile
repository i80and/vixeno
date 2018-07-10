CFLAGS=-Wall -Wextra -Wshadow -fomit-frame-pointer -fstrict-aliasing -O2
SOURCE_FILES=gc/gc.c gc/ptr_list.c gc/util.c

.PHONY: clean lint test

test:
	$(CC) gc/test.c $(SOURCE_FILES) $(CFLAGS) -DVIXENO_TEST -o $@
	./test

lint:
	cppcheck --enable=all --inconclusive $(SOURCE_FILES)

clean:
	-rm vixeno
