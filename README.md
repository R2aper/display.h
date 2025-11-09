# Display

A single-header C library for simple, yet potentially unsafe, printing of custom structs, similar to Rust's `Display` trait

## Features

-   Type-safe printing of custom structs
-   `printf`-like format string
-   Custom format specifier `{}` for structs
-   Functions for printing to `stdout`, `FILE*`, and character buffers
-   Single header library, just drop it in your project

## Usage

To use this library, you need to define `DISPLAY_IMPLEMENTATION` in one of your C files before including `display.h`.

```c
#define DISPLAY_IMPLEMENTATION
#include "display.h"
```

### Making a struct displayable

For a struct to be displayable, it must:
1.  Have a `display_t` member as its first field. This allows casting the struct pointer to a `display_t` pointer
2.  Implement a display function
3.  Assign the display function to the corresponding function pointer in the `display_t` member
4.  Set the `self` pointer in the `display_t` member to point to the struct instance itself

There are three types of display functions you can implement:
-   `int (*display_fn)(const void *)`: For printing to `stdout`. Used by `print` and `println`
-   `int (*fdisplay_fn)(const void *, FILE *)`: For printing to a `FILE*`. Used by `fprint` and `fprintln`
-   `int (*sndisplay_fn)(const void *, char *, size_t)`: For printing to a string. Used by `snprint` and `snprintln`

You only need to implement the functions for the printing methods you intend to use

### Example

Here is an example of how to make a `point_t` struct displayable and print it.

```c
#define DISPLAY_IMPLEMENTATION
#define DISPLAY_STRIP_PREFIX
#include "display.h"

typedef struct point_t {
  display_t d; // Should be the first field
  int x, y;
} point_t;

// Implement the display function for stdout
int display_point(const void *self) {
  if(!self)
    return -1;

  point_t *p = (point_t *)self;
  return printf("(%d,%d)", p->x, p->y);
}

int main() {
  point_t a = {0};
  a.x = 2;
  a.y = 3;

  // Setup the display_t member
  a.d.display_fn = display_point;
  a.d.self = &a; // WARNING: Be sure that display_t::self points to the object
                 // itself, otherwise passing it to display_fn results in
                 // undefined behavior

  println("Point a = {}", &a); // Pass a pointer to the struct
  // Output: Point a = (2,3)

  return 0;
}
```

### Stripping the prefix

If you find the `display_` prefix verbose, you can define `DISPLAY_STRIP_PREFIX` before including the header. This will create aliases for all functions (e.g., `display_print` becomes `print`)

```c
#define DISPLAY_IMPLEMENTATION
#define DISPLAY_STRIP_PREFIX
#include "display.h"

// ...

println("Point a = {}", &a); // Now you can use println
```

## API

The library provides three sets of printing functions:

### Printing to stdout

-   `int display_print(const char *format, ...)`
-   `int display_println(const char *format, ...)`
-   `int display_vprint(const char *format, va_list args)`
-   `int display_vprintln(const char *format, va_list args)`

### Printing to a FILE stream

-   `int display_fprint(FILE *file, const char *format, ...)`
-   `int display_fprintln(FILE *file, const char *format, ...)`
-   `int display_vfprint(FILE *file, const char *format, va_list args)`
-   `int display_vfprintln(FILE *file, const char *format, va_list args)`

### Printing to a string

-   `int display_snprint(char *buf, size_t size, const char *format, ...)`
-   `int display_snprintln(char *buf, size_t size, const char *format, ...)`
-   `int display_vsnprint(char *buf, size_t size, const char *format, va_list args)`
-   `int display_vsnprintln(char *buf, size_t size, const char *format, va_list args)`

## Format String

The format string is similar to `printf`, but with the addition of the `{}` specifier for custom structs. When a `{}` is encountered, the next argument, which is expected to be a pointer to a displayable struct, is printed using its corresponding display function
