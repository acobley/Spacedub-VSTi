#pragma once
#include <cairo/cairo.h>
typedef struct _PangoLayout PangoLayout;
inline void pango_cairo_show_layout (cairo_t*, PangoLayout*) {}
