/*
TODO:
*/
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>
#include <stdio.h>

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

// TODO:

#endif // DISPLAY_H

#ifdef DISPLAY_IMPLEMENTATION

// TODO:

#endif // DISPLAY_IMPLEMENTATION
