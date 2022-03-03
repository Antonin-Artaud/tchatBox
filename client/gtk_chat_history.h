//
// Created by warydra on 04/09/2020.
//

#ifndef SUPER_CHAT_GTK_CHAT_HISTORY_H
#define SUPER_CHAT_GTK_CHAT_HISTORY_H

#include <cairo.h>

static const int TEXT_INITIAL_MARGIN = 20;
static const int TEXT_MARGIN = 20;

void gtk_chat_history_build_index(cairo_t *);
void gtk_chat_history_display();

#endif //SUPER_CHAT_GTK_CHAT_HISTORY_H
