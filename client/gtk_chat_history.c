//
// Created by warydra on 04/09/2020.
//
#include <assert.h>
#include "gtk_chat_history.h"
#include "chatHistory.h"

typedef struct index_ctx gtk_chat_history_build_index_ctx;

struct index_ctx {
    cairo_t *gtk_ctx;
    int x;
    int y;
};

void gtk_chat_history_build_index_lambdaFunc(chatHistoryEntry entry, void *genericCtx) {
    gtk_chat_history_build_index_ctx *ctx = genericCtx;

    assert(ctx->y - TEXT_INITIAL_MARGIN <= MAX_ENTRIES * TEXT_MARGIN);

    cairo_select_font_face (ctx->gtk_ctx, "Broadway", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size (ctx->gtk_ctx, 15);
    cairo_move_to  (ctx->gtk_ctx, ctx->x, ctx->y); // 600 max

    if (entry.isPrivateMessage)
    {
        cairo_set_source_rgb(ctx->gtk_ctx, 0, 0, 1);
    }
    else
    {
        cairo_set_source_rgb(ctx->gtk_ctx, 0, 0, 0);
    }

    if (entry.isPrivateMessage && entry.isPrivateMessageError)
    {
        cairo_set_source_rgb(ctx->gtk_ctx, 1, 0, 0);
    }

    if (entry.senderUsername != NULL)
    {
        cairo_show_text(ctx->gtk_ctx, entry.senderUsername);
        cairo_show_text(ctx->gtk_ctx, " - ");
    }

    cairo_show_text (ctx->gtk_ctx, entry.message);

    ctx->y += TEXT_MARGIN;
}

void gtk_chat_history_build_index(cairo_t *gtk_ctx) {
    gtk_chat_history_build_index_ctx ctx = {
            gtk_ctx,
            0,
            TEXT_INITIAL_MARGIN
    };
    chat_history_foreach(gtk_chat_history_build_index_lambdaFunc, &ctx);
}

void gtk_chat_history_display_lambdaFunc(chatHistoryEntry entry, void *genericCtx) {
    printf("%s (%d)\n", entry.message, chat_history_count());
}

void gtk_chat_history_display() {
    chat_history_foreach(gtk_chat_history_display_lambdaFunc, NULL);
}