#ifndef CHATHISTORY_H
#define CHATHISTORY_H

#include <gtk/gtk.h>

static const int MAX_ENTRIES = 30;

typedef struct ChatHistoryEntry chatHistoryEntry;

struct ChatHistoryEntry {
    int senderUsernameLength;
    char *senderUsername;
    int messageLength;
    char *message;
    char isPrivateMessage;
    char isPrivateMessageError;
};

typedef struct ChatHistoryList chatHistoryList;

struct ChatHistoryList {
    chatHistoryEntry current;
    chatHistoryList *next;
};


void chat_history_push(chatHistoryEntry entry);
chatHistoryEntry chat_history_pop();
int chat_history_count();
chatHistoryEntry chat_history_at(int position);
void chat_history_foreach(void (*action(chatHistoryEntry entry, void *ctx)), void *ctx);

/*typedef struct Element Element;

struct Element
{
    char *message;
    Element *suivant;
};

typedef struct List List;

struct List
{
    int count;
    Element *premier;
};

List *init_List();
void insertMessage(List *list, char *pMessage);
void displayList(List *list);
int getListSize(List *list);
void indexMessage(List *list, int position, cairo_t *contexte);*/

#endif // CHATHISTORY_H
