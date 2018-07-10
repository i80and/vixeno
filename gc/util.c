#include <stdio.h>
#include <stdlib.h>

void __fail(const char* filename, int lineno, const char* condition) {
    fprintf(stderr, "%s(%d): Assertion failed\n\t%s\n", filename, lineno, condition);
    abort();
}
