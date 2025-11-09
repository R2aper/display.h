/* display.h

A single-header library for simple/dangerous displaying of custom structs

# Example:
```c
#define DISPLAY_IMPLEMENTATION
#define DISPLAY_STRIP_PREFIX
#include "display.h"

typedef struct point_t {
  display_t d; // Should be the first field (this allows casting the struct
               // pointer to a display_t pointer)
  int x, y;

} point_t;

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

  a.d.display_fn = display_point;
  a.d.self = &a; // WARNING: Be sure that display_t::self points to the object
                 // itself, otherwise passing it to display_fn results in
                 // undefined behavior

  println("Point = {}", &a); // Pass a pointer to the struct
  // Output: Point = (2,3)

  return 0;
}
```

*/
#ifndef DISPLAY_H
#define DISPLAY_H

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// @brief The base struct for containing pointers to display functions.
/// @note  For a struct to be displayable, it must:
/// - Have a display_t member as its first field
/// - Have a valid pointer to itself in the display_t::self field
/// - Implement at least one of display_fn, fdisplay_fn, or sndisplay_fn
typedef struct display_t {
  int (*display_fn)(const void *);
  int (*fdisplay_fn)(const void *, FILE *);
  int (*sndisplay_fn)(const void *, char *, size_t);
  void *self;

} display_t;

//------------------------Print to stdout------------------------\\

/// @brief Prints formatted text to stdout
/// @return The number of elements printed or -1 on failure
int display_vprint(const char *__restrict format, va_list args);

/// @brief Prints formatted text to stdout, followed by a newline
/// @return The number of elements printed or -1 on failure
int display_vprintln(const char *__restrict format, va_list args);

/// @brief Prints formatted text to stdout
/// @return The number of elements printed or -1 on failure
int display_print(const char *__restrict format, ...);

/// @brief Prints formatted text to stdout, followed by a newline
/// @return The number of elements printed or -1 on failure
int display_println(const char *__restrict format, ...);

//------------------------Print to stdout------------------------\\

//-------------------------Print to FILE-------------------------\\

/// @brief Writes formatted text to the specified file stream
/// @return The number of elements printed or -1 on failure
int display_vfprint(FILE *file, const char *__restrict format, va_list args);

/// @brief Writes formatted text to the specified file stream, followed by a
/// newline
/// @return The number of elements printed or -1 on failure
int display_vfprintln(FILE *file, const char *__restrict format, va_list args);

/// @brief Writes formatted text to the specified file stream
/// @return The number of elements printed or -1 on failure
int display_fprint(FILE *file, const char *__restrict format, ...);

/// @brief Writes formatted text to the specified file stream, followed by a
/// newline
/// @return The number of elements printed or -1 on failure
int display_fprintln(FILE *file, const char *__restrict format, ...);

//-------------------------Print to FILE-------------------------\\


#endif // DISPLAY_H

#ifdef DISPLAY_IMPLEMENTATION

typedef enum var_type {
  // Floating point
  TYPE_FLOAT,       // float
  TYPE_DOUBLE,      // double
  TYPE_LONG_DOUBLE, // long double

  // Pointers
  TYPE_POINTER, // void*
  TYPE_STRING,  // char*
  // Reference
  TYPE_POINTER_SIGNED_INT8, // signed char*
  TYPE_POINTER_SHORT,       // short*
  TYPE_POINTER_INT,         // int*
  TYPE_POINTER_LONG,        // long*
  TYPE_POINTER_LONG_LONG,   // long long*
  TYPE_POINTER_INTMAX_T,    // intmax_t*
  TYPE_POINTER_SSIZE_T,     // ssize_t*
  TYPE_POINTER_PTRDIFF_T,   // ptrdiff_t*

  // Signed integers
  TYPE_SIGNED_INT8, // signed char
  TYPE_SHORT,       // short
  TYPE_INT,         // int
  TYPE_LONG,        // long
  TYPE_LONG_LONG,   // long long
  TYPE_INTMAX_T,    // intmax_t
  TYPE_SSIZE_T,     // ssize_t
  TYPE_PTRDIFF_T,   // ptrdiff_t

  // Unsigned integers
  TYPE_UINT8,      // unsigned char
  TYPE_USHORT,     // unsigned short
  TYPE_UINT,       // unsigned int
  TYPE_ULONG,      // unsigned long
  TYPE_ULONG_LONG, // unsigned long long
  TYPE_UINTMAX_T,  // uintmax_t
  TYPE_SIZE_T,     // size_t

  TYPE_NONE, // none

} var_type;

typedef struct format_spec_t {
  char *substr; // The substring of the format specifier (e.g., "%d", "%.2f")
  var_type type;

} format_spec_t;

typedef struct format_specs_array_t {
  struct format_spec_t *data;
  size_t count;

} format_specs_array_t;

static void format_specs_array_t_free(format_specs_array_t *specs) {
  for (size_t i = 0; i < specs->count; i++)
    free(specs->data[i].substr);

  free(specs->data);
}

static format_specs_array_t find_format_specifiers(const char *format) {
  int count = 0;
  format_specs_array_t specs = {NULL, 0};
  const char *p = format;

  while (*p) {
    if (*p == '%') {
      // Skip %%
      if (*(p + 1) == '%') {
        p += 2;
        continue;
      }

      // Start substr
      const char *start = p;
      p++; // Skip '%'

      // Flags: - + ' ' # 0
      while (strchr("-+ #0", *p)) {
        p++;
      }

      // Width: numbers or *
      if (*p == '*') {
        p++;
      } else {
        while (isdigit(*p)) {
          p++;
        }
      }

      // Precision
      if (*p == '.') {
        p++;
        if (*p == '*') {
          p++;
        } else {
          while (isdigit(*p)) {
            p++;
          }
        }
      }

      // Length: hh, h, l, ll, j, z, t, L
      char length[3] = {0};
      if (strchr("hljztL", *p)) {
        length[0] = *p;
        p++;
        if ((*p == 'h' || *p == 'l') &&
            (length[0] == 'h' || length[0] == 'l')) {
          length[1] = *p;
          p++;
        }
      }

      // Specifiers: d i o u x X e E f F g G a A c s p n %
      char specifier = *p;
      if (strchr("diouxXeEfFgGaAcspn%", specifier)) {
        p++;
      } else {
        // Invalid specifier
        continue;
      }

      size_t len = p - start;
      char *sub = (char *)malloc(len + 1);
      if (!sub) {
        // Buy more ram wtf
        continue;
      }

      strncpy(sub, start, len);
      sub[len] = '\0';

      // Get type
      enum var_type type = -1;
      if (specifier == '%') {
        type = TYPE_NONE;
      } else if (specifier == 'p') {
        type = TYPE_POINTER;
      } else if (specifier == 'c') {
        type = TYPE_INT;
      } else if (specifier == 's') {
        type = TYPE_STRING;
      } else if (specifier == 'n') {
        // Reference
        if (strcmp(length, "hh") == 0)
          type = TYPE_POINTER_SIGNED_INT8;
        else if (strcmp(length, "h") == 0)
          type = TYPE_POINTER_SHORT;
        else if (strcmp(length, "") == 0)
          type = TYPE_POINTER_INT;
        else if (strcmp(length, "l") == 0)
          type = TYPE_POINTER_LONG;
        else if (strcmp(length, "ll") == 0)
          type = TYPE_POINTER_LONG_LONG;
        else if (strcmp(length, "j") == 0)
          type = TYPE_POINTER_INTMAX_T;
        else if (strcmp(length, "z") == 0)
          type = TYPE_POINTER_SSIZE_T;
        else if (strcmp(length, "t") == 0)
          type = TYPE_POINTER_PTRDIFF_T;
        else
          type = TYPE_POINTER_INT;
      } else if (strchr("di", specifier)) {
        // Signed integer
        if (strcmp(length, "hh") == 0)
          type = TYPE_SIGNED_INT8;
        else if (strcmp(length, "h") == 0)
          type = TYPE_SHORT;
        else if (strcmp(length, "") == 0)
          type = TYPE_INT;
        else if (strcmp(length, "l") == 0)
          type = TYPE_LONG;
        else if (strcmp(length, "ll") == 0)
          type = TYPE_LONG_LONG;
        else if (strcmp(length, "j") == 0)
          type = TYPE_INTMAX_T;
        else if (strcmp(length, "z") == 0)
          type = TYPE_SSIZE_T;
        else if (strcmp(length, "t") == 0)
          type = TYPE_PTRDIFF_T;
        else
          type = TYPE_INT;
      } else if (strchr("ouxX", specifier)) {
        // Unsigned integer
        if (strcmp(length, "hh") == 0)
          type = TYPE_UINT8;
        else if (strcmp(length, "h") == 0)
          type = TYPE_USHORT;
        else if (strcmp(length, "") == 0)
          type = TYPE_UINT;
        else if (strcmp(length, "l") == 0)
          type = TYPE_ULONG;
        else if (strcmp(length, "ll") == 0)
          type = TYPE_ULONG_LONG;
        else if (strcmp(length, "j") == 0)
          type = TYPE_UINTMAX_T;
        else if (strcmp(length, "z") == 0)
          type = TYPE_SIZE_T;
        else if (strcmp(length, "t") == 0)
          type = TYPE_PTRDIFF_T;
        else
          type = TYPE_UINT;
      } else if (strchr("eEfFgGaA", specifier)) {
        // Floating point
        if (strcmp(length, "L") == 0)
          type = TYPE_LONG_DOUBLE;
        else
          type = TYPE_DOUBLE;
      }

      if (type != -1) {
        count++;
        specs.data = (struct format_spec_t *)realloc(
            specs.data, count * sizeof(struct format_spec_t));
        if (!specs.data) {
          free(sub);
          continue;
        }
        specs.data[count - 1].substr = sub;
        specs.data[count - 1].type = type;
        specs.count = count;
      } else {
        free(sub);
      }
    } else {
      p++;
    }
  }

  return specs;
}

int display_vprint(const char *__restrict format, va_list args) {
  if (!format)
    return -1;

  int spec_count = 0, struct_count = 0;
  format_specs_array_t specs = find_format_specifiers(format);
  const char *p = format;
  int spec_idx = 0;

  while (*p) {
    if (*p == '%' && *(p + 1) != '%') {
      if (spec_idx < specs.count) {
        switch (specs.data[spec_idx].type) {
        // Signed integers
        case TYPE_INT:
          printf(specs.data[spec_idx].substr, va_arg(args, int));
          break;
        case TYPE_SIGNED_INT8:
          printf(specs.data[spec_idx].substr, va_arg(args, int));
          break;
        case TYPE_SHORT:
          printf(specs.data[spec_idx].substr, va_arg(args, int));
          break;
        case TYPE_LONG:
          printf(specs.data[spec_idx].substr, va_arg(args, long));
          break;
        case TYPE_LONG_LONG:
          printf(specs.data[spec_idx].substr, va_arg(args, long long));
          break;
        case TYPE_INTMAX_T:
          printf(specs.data[spec_idx].substr, va_arg(args, intmax_t));
          break;
        case TYPE_SSIZE_T:
          printf(specs.data[spec_idx].substr, va_arg(args, ssize_t));
          break;
        case TYPE_PTRDIFF_T:
          printf(specs.data[spec_idx].substr, va_arg(args, ptrdiff_t));
          break;

        // Unsigned integers
        case TYPE_UINT:
          printf(specs.data[spec_idx].substr, va_arg(args, unsigned int));
          break;
        case TYPE_UINT8:
          printf(specs.data[spec_idx].substr, va_arg(args, unsigned int));
          break;
        case TYPE_USHORT:
          printf(specs.data[spec_idx].substr, va_arg(args, unsigned int));
          break;
        case TYPE_ULONG:
          printf(specs.data[spec_idx].substr, va_arg(args, unsigned long));
          break;
        case TYPE_ULONG_LONG:
          printf(specs.data[spec_idx].substr, va_arg(args, unsigned long long));
          break;
        case TYPE_UINTMAX_T:
          printf(specs.data[spec_idx].substr, va_arg(args, uintmax_t));
          break;
        case TYPE_SIZE_T:
          printf(specs.data[spec_idx].substr, va_arg(args, size_t));
          break;

        // Pointers
        case TYPE_POINTER:
          printf(specs.data[spec_idx].substr, va_arg(args, void *));
          break;
        case TYPE_STRING:
          printf(specs.data[spec_idx].substr, va_arg(args, char *));
          break;

        // Reference %n
        case TYPE_POINTER_INT:
          *va_arg(args, int *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_SIGNED_INT8:
          *va_arg(args, signed char *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_SHORT:
          *va_arg(args, short *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_LONG:
          *va_arg(args, long *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_LONG_LONG:
          *va_arg(args, long long *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_INTMAX_T:
          *va_arg(args, intmax_t *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_SSIZE_T:
          *va_arg(args, ssize_t *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_PTRDIFF_T:
          *va_arg(args, ptrdiff_t *) = spec_count + struct_count;
          break;

        // Floating point
        case TYPE_FLOAT:
          printf(specs.data[spec_idx].substr, va_arg(args, double));
          break;
        case TYPE_DOUBLE:
          printf(specs.data[spec_idx].substr, va_arg(args, double));
          break;
        case TYPE_LONG_DOUBLE:
          printf(specs.data[spec_idx].substr, va_arg(args, long double));
          break;

        case TYPE_NONE:
          break;
        }
        p += strlen(specs.data[spec_idx].substr);
        spec_idx++;
        spec_count++;
      } else {
        putchar(*p);
        p++;
      }
    } else if (*p == '%' && *(p + 1) == '%') {
      putchar('%');
      p += 2;
    } else if (*p == '{' && *(p + 1) == '}') {
      display_t *d = va_arg(args, display_t *);
      if (!d || !d->display_fn || !d->self) { // Invalid pointer
        p += 2;
        continue;
      }

      if (d->display_fn(d->self) == -1) { // Error from display_fn
        p += 2;
        continue;
      }

      p += 2;
      struct_count++;
    } else {
      putchar(*p);
      p++;
    }
  }

  format_specs_array_t_free(&specs);

  return spec_count + struct_count;
}

int display_vprintln(const char *__restrict format, va_list args) {
  int result = display_vprint(format, args);
  if (result != -1)
    putchar('\n');

  return result;
}

int display_print(const char *__restrict format, ...) {
  va_list args;
  va_start(args, format);
  int result = display_vprint(format, args);
  va_end(args);

  return result;
}

int display_println(const char *__restrict format, ...) {
  va_list args;
  va_start(args, format);
  int result = display_vprintln(format, args);
  va_end(args);

  return result;
}

int display_vfprint(FILE *file, const char *__restrict format, va_list args) {
  if (!format || !file)
    return -1;

  int spec_count = 0, struct_count = 0;
  format_specs_array_t specs = find_format_specifiers(format);
  const char *p = format;
  int spec_idx = 0;

  while (*p) {
    if (*p == '%' && *(p + 1) != '%') {
      if (spec_idx < specs.count) {
        switch (specs.data[spec_idx].type) {
        // Signed integers
        case TYPE_INT:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, int));
          break;
        case TYPE_SIGNED_INT8:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, int));
          break;
        case TYPE_SHORT:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, int));
          break;
        case TYPE_LONG:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, long));
          break;
        case TYPE_LONG_LONG:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, long long));
          break;
        case TYPE_INTMAX_T:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, intmax_t));
          break;
        case TYPE_SSIZE_T:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, ssize_t));
          break;
        case TYPE_PTRDIFF_T:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, ptrdiff_t));
          break;

        // Unsigned integers
        case TYPE_UINT:
          fprintf(file, specs.data[spec_idx].substr,
                  va_arg(args, unsigned int));
          break;
        case TYPE_UINT8:
          fprintf(file, specs.data[spec_idx].substr,
                  va_arg(args, unsigned int));
          break;
        case TYPE_USHORT:
          fprintf(file, specs.data[spec_idx].substr,
                  va_arg(args, unsigned int));
          break;
        case TYPE_ULONG:
          fprintf(file, specs.data[spec_idx].substr,
                  va_arg(args, unsigned long));
          break;
        case TYPE_ULONG_LONG:
          fprintf(file, specs.data[spec_idx].substr,
                  va_arg(args, unsigned long long));
          break;
        case TYPE_UINTMAX_T:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, uintmax_t));
          break;
        case TYPE_SIZE_T:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, size_t));
          break;

        // Pointers
        case TYPE_POINTER:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, void *));
          break;
        case TYPE_STRING:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, char *));
          break;

        // Reference %n
        case TYPE_POINTER_INT:
          *va_arg(args, int *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_SIGNED_INT8:
          *va_arg(args, signed char *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_SHORT:
          *va_arg(args, short *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_LONG:
          *va_arg(args, long *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_LONG_LONG:
          *va_arg(args, long long *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_INTMAX_T:
          *va_arg(args, intmax_t *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_SSIZE_T:
          *va_arg(args, ssize_t *) = spec_count + struct_count;
          break;
        case TYPE_POINTER_PTRDIFF_T:
          *va_arg(args, ptrdiff_t *) = spec_count + struct_count;
          break;

        // Floating point
        case TYPE_FLOAT:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, double));
          break;
        case TYPE_DOUBLE:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, double));
          break;
        case TYPE_LONG_DOUBLE:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, long double));
          break;

        case TYPE_NONE:
          break;
        }
        p += strlen(specs.data[spec_idx].substr);
        spec_idx++;
        spec_count++;
      } else {
        putc(*p, file);
        p++;
      }
    } else if (*p == '%' && *(p + 1) == '%') {
      putc('%', file);
      p += 2;
    } else if (*p == '{' && *(p + 1) == '}') {
      display_t *d = va_arg(args, display_t *);
      if (!d || !d->fdisplay_fn || !d->self) { // Invalid pointer
        p += 2;
        continue;
      }

      if (d->fdisplay_fn(d->self, file) == -1) { // Error from fdisplay_fn
        p += 2;
        continue;
      }

      p += 2;
      struct_count++;
    } else {
      putc(*p, file);
      p++;
    }
  }

  format_specs_array_t_free(&specs);

  return spec_count + struct_count;
}

int display_vfprintln(FILE *file, const char *__restrict format, va_list args) {
  int result = display_vfprint(file, format, args);
  if (result != -1)
    putc('\n', file);

  return result;
}

int display_fprint(FILE *file, const char *__restrict format, ...) {
  va_list args;
  va_start(args, format);
  int result = display_vfprint(file, format, args);
  va_end(args);

  return result;
}

int display_fprintln(FILE *file, const char *__restrict format, ...) {
  va_list args;
  va_start(args, format);
  int result = display_vfprintln(file, format, args);
  va_end(args);

  return result;
}

#ifdef DISPLAY_STRIP_PREFIX
#define print display_print
#define println display_println
#define vprint display_vprint
#define vprintln display_vprintln
#define fprint display_fprint
#define fprintln display_fprintln
#define vfprint display_vfprint
#define vfprintln display_vfprintln
#endif // DISPLAY_STRIP_PREFIX

#endif // DISPLAY_IMPLEMENTATION
