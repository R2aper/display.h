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
#ifdef _WIN32
#define ssize_t int
#else
#include <unistd.h>
#endif

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

/*------------------------Print to stdout------------------------*/

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

/*------------------------Print to stdout------------------------*/

/*-------------------------Print to FILE-------------------------*/

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

/*-------------------------Print to FILE-------------------------*/

/*------------------------Print to string------------------------*/

/// @brief Writes formatted text to the specified string buffer
/// @return The number of characters that would have been written, or a negative
/// value on failure.
int display_vsnprint(char *buf, size_t size, const char *__restrict format, va_list args);

/// @brief Writes formatted text to the specified string buffer, followed by a
/// newline
/// @return The number of characters that would have been written, or a negative
/// value on failure.
int display_vsnprintln(char *buf, size_t size, const char *__restrict format, va_list args);

/// @brief Writes formatted text to the specified string buffer
/// @return The number of characters that would have been written, or a negative
/// value on failure.
int display_snprint(char *buf, size_t size, const char *__restrict format, ...);

/// @brief Writes formatted text to the specified string buffer, followed by a
/// newline
/// @return The number of characters that would have been written, or a negative
/// value on failure.
int display_snprintln(char *buf, size_t size, const char *__restrict format, ...);

/*------------------------Print to string------------------------*/


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

  TYPE_PERCENT, // %
  TYPE_NONE,    // none

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
        if ((*p == 'h' || *p == 'l') && (length[0] == 'h' || length[0] == 'l')) {
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
        type = TYPE_PERCENT;
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
      } else
        type = TYPE_NONE;

      if (type != TYPE_NONE) {
        count++;
        specs.data =
            (struct format_spec_t *)realloc(specs.data, count * sizeof(struct format_spec_t));
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
  size_t spec_idx = 0;

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
        case TYPE_PERCENT:
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
  size_t spec_idx = 0;

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
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, unsigned int));
          break;
        case TYPE_UINT8:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, unsigned int));
          break;
        case TYPE_USHORT:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, unsigned int));
          break;
        case TYPE_ULONG:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, unsigned long));
          break;
        case TYPE_ULONG_LONG:
          fprintf(file, specs.data[spec_idx].substr, va_arg(args, unsigned long long));
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
        case TYPE_PERCENT:
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

int display_vsnprint(char *buf, size_t size, const char *__restrict format, va_list args) {
  if (!format)
    return -1;
  if (!buf && size > 0)
    return -1;

  format_specs_array_t specs = find_format_specifiers(format);
  const char *p = format;
  size_t spec_idx = 0;

  char *buf_ptr = buf;
  size_t remaining_size = size;
  int total_chars = 0;

  while (*p) {
    if (*p == '%' && *(p + 1) != '%') {
      if (spec_idx < specs.count) {
        int written = 0;
        switch (specs.data[spec_idx].type) {
        // Signed integers
        case TYPE_INT:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, int));
          break;
        case TYPE_SIGNED_INT8:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, int));
          break;
        case TYPE_SHORT:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, int));
          break;
        case TYPE_LONG:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, long));
          break;
        case TYPE_LONG_LONG:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, long long));
          break;
        case TYPE_INTMAX_T:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, intmax_t));
          break;
        case TYPE_SSIZE_T:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, ssize_t));
          break;
        case TYPE_PTRDIFF_T:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, ptrdiff_t));
          break;

        // Unsigned integers
        case TYPE_UINT:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, unsigned int));
          break;
        case TYPE_UINT8:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, unsigned int));
          break;
        case TYPE_USHORT:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, unsigned int));
          break;
        case TYPE_ULONG:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, unsigned long));
          break;
        case TYPE_ULONG_LONG:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, unsigned long long));
          break;
        case TYPE_UINTMAX_T:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, uintmax_t));
          break;
        case TYPE_SIZE_T:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, size_t));
          break;

        // Pointers
        case TYPE_POINTER:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, void *));
          break;
        case TYPE_STRING:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, char *));
          break;

        // Reference %n
        case TYPE_POINTER_INT:
          *va_arg(args, int *) = total_chars;
          break;
        case TYPE_POINTER_SIGNED_INT8:
          *va_arg(args, signed char *) = total_chars;
          break;
        case TYPE_POINTER_SHORT:
          *va_arg(args, short *) = total_chars;
          break;
        case TYPE_POINTER_LONG:
          *va_arg(args, long *) = total_chars;
          break;
        case TYPE_POINTER_LONG_LONG:
          *va_arg(args, long long *) = total_chars;
          break;
        case TYPE_POINTER_INTMAX_T:
          *va_arg(args, intmax_t *) = total_chars;
          break;
        case TYPE_POINTER_SSIZE_T:
          *va_arg(args, ssize_t *) = total_chars;
          break;
        case TYPE_POINTER_PTRDIFF_T:
          *va_arg(args, ptrdiff_t *) = total_chars;
          break;

        // Floating point
        case TYPE_FLOAT:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, double));
          break;
        case TYPE_DOUBLE:
          written =
              snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr, va_arg(args, double));
          break;
        case TYPE_LONG_DOUBLE:
          written = snprintf(buf_ptr, remaining_size, specs.data[spec_idx].substr,
                             va_arg(args, long double));
          break;

        case TYPE_NONE:
          break;
        case TYPE_PERCENT:
          break;
        }

        if (written > 0) {
          if ((size_t)written < remaining_size) {
            buf_ptr += written;
            remaining_size -= written;
          } else {
            if (remaining_size > 0)
              buf_ptr += remaining_size - 1;
            remaining_size = (remaining_size > 0) ? 1 : 0;
          }
          total_chars += written;
        }

        p += strlen(specs.data[spec_idx].substr);
        spec_idx++;
      } else {
        if (remaining_size > 1) {
          *buf_ptr++ = *p;
          remaining_size--;
        }
        total_chars++;
        p++;
      }
    } else if (*p == '%' && *(p + 1) == '%') {
      if (remaining_size > 1) {
        *buf_ptr++ = '%';
        remaining_size--;
      }
      total_chars++;
      p += 2;
    } else if (*p == '{' && *(p + 1) == '}') {
      display_t *d = va_arg(args, display_t *);
      if (!d || !d->sndisplay_fn || !d->self) { // Invalid pointer
        p += 2;
        continue;
      }

      int written = d->sndisplay_fn(d->self, buf_ptr, remaining_size);
      if (written >= 0) {
        if ((size_t)written < remaining_size) {
          buf_ptr += written;
          remaining_size -= written;
        } else {
          if (remaining_size > 0)
            buf_ptr += remaining_size - 1;
          remaining_size = (remaining_size > 0) ? 1 : 0;
        }
        total_chars += written;
      }

      p += 2;
    } else {
      if (remaining_size > 1) {
        *buf_ptr++ = *p;
        remaining_size--;
      }
      total_chars++;
      p++;
    }
  }

  if (size > 0) {
    *buf_ptr = '\0';
  }

  format_specs_array_t_free(&specs);

  return total_chars;
}

int display_vsnprintln(char *buf, size_t size, const char *__restrict format, va_list args) {
  int written = display_vsnprint(buf, size, format, args);
  if (written < 0)
    return written;

  if ((size_t)written < size - 1) {
    buf[written] = '\n';
    buf[written + 1] = '\0';
  }

  return written + 1;
}

int display_snprint(char *buf, size_t size, const char *__restrict format, ...) {
  va_list args;
  va_start(args, format);
  int result = display_vsnprint(buf, size, format, args);
  va_end(args);

  return result;
}

int display_snprintln(char *buf, size_t size, const char *__restrict format, ...) {
  va_list args;
  va_start(args, format);
  int result = display_vsnprintln(buf, size, format, args);
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
#define snprint display_snprint
#define snprintln display_snprintln
#define vsnprint display_vsnprint
#define vsnprintln display_vsnprintln
#endif // DISPLAY_STRIP_PREFIX

#endif // DISPLAY_IMPLEMENTATION
/*
MIT License

Copyright (c) 2025 R2aper

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
