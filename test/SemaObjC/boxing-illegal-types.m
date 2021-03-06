// RUN: %clang_cc1 -fsyntax-only -verify -Wattributes %s

typedef struct {
    int x, y, z;
} point;

void testStruct() {
    point p = { 0, 0, 0 };
    id boxed = @(p);    // expected-error {{Illegal type 'point' used in a boxed expression}}
}

void testPointers() {
    void *null = 0;
    id boxed_null = @(null);        // expected-error {{Illegal type 'void *' used in a boxed expression}}
    int numbers[] = { 0, 1, 2 };
    id boxed_numbers = @(numbers);  // expected-error {{Illegal type 'int *' used in a boxed expression}}
}
