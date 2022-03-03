#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chatHistory.h"
#include <assert.h>
#include <gtk/gtk.h>

void chat_history_push(chatHistoryEntry entry);
chatHistoryEntry chat_history_pop();
int chat_history_count();
chatHistoryEntry chat_history_at(int position);

chatHistoryList *list;

void chat_history_push(chatHistoryEntry entry)
{
    chatHistoryList *newSlot = (chatHistoryList *) malloc(sizeof (chatHistoryList));

    newSlot->current = entry;
    newSlot->next = NULL;

    if (list == NULL)
    {
        list = newSlot;
    }
    else
    {
        if (chat_history_count() > MAX_ENTRIES) {
            list = list->next; // supprime le premier element
        }

        chatHistoryList* last = list;

        while (last->next != NULL) {
            last = last->next;
        }

        last->next = newSlot;
    }
}

chatHistoryEntry chat_history_pop()
{
    chatHistoryEntry pop = list->current;
    list = list->next;
    return pop;
}

int chat_history_count()
{
    int count = 0;
    chatHistoryList *listPos = list;

    while (listPos != NULL)
    {
        count++;
        listPos = listPos->next;
    }

    return count;
}

chatHistoryEntry chat_history_at(int pos)
{
    assert(pos >= 0 && pos < chat_history_count());

    chatHistoryList *listPos = list;

    for (int i = 0; i < pos; i++)
    {
        listPos = listPos->next;
    }

    return listPos->current;
}

void chat_history_foreach(void (*action(chatHistoryEntry entry, void *ctx)), void *ctx) {
    chatHistoryList *listPos = list;

    while (listPos != NULL)
    {
        action(listPos->current, ctx);
        listPos = listPos->next;
    }
}

