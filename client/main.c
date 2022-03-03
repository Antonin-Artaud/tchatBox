#include <stdio.h>
#include <gtk/gtk.h>
#include "client.h"
#include "chatHistory.h"
#include "gtk_chat_history.h"
#include "memory_util.h"

GtkWidget *entryMessage = NULL;
GtkWidget *canvas = NULL;
GtkWidget *pTable = NULL;
GtkWidget *pWindow = NULL;

clientListener buildClientListener();

static const char DEBUG_NAME[] = "Harry_Potter";

void OnDestroy ()
{
    gtk_main_quit();
}

gboolean showHistory (GtkWidget* widget, cairo_t *context, gpointer user_data)
{
    gtk_chat_history_build_index(context);

    return FALSE;
}

void ui_sendMessage(GtkWidget *pWidget, gpointer pData)
{
    char messageEntry[stdBufferSize];

    memcpy(messageEntry, gtk_entry_get_text((struct _GtkEntry*) entryMessage), strlen(gtk_entry_get_text((struct _GtkEntry*) entryMessage)) + 1);

    client_sendMessage(messageEntry, strlen(gtk_entry_get_text((struct _GtkEntry*) entryMessage)) + 1);
}

void selectEmoji()
{

}

void goToGTK(int argc,char **argv)
{
    GtkWidget *btnSendMessage = NULL;
    GtkWidget *btnEmoji = NULL;

    gtk_init(&argc,&argv);

    pWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_position     (GTK_WINDOW (pWindow), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size (GTK_WINDOW (pWindow), 360, 720);
    gtk_window_set_title        (GTK_WINDOW (pWindow), "Client");
    gtk_container_set_border_width(GTK_CONTAINER(pWindow), 10);

    pTable = gtk_table_new(2, 3, TRUE);
    gtk_container_add(GTK_CONTAINER(pWindow), GTK_WIDGET(pTable));

    btnSendMessage = gtk_button_new_with_label("Send message");
    gtk_table_attach_defaults(GTK_TABLE(pTable), btnSendMessage, 2, 4, 9, 10);

    btnEmoji = gtk_button_new_with_label("Emoji");
    gtk_table_attach_defaults(GTK_TABLE(pTable), btnEmoji, 4, 5, 9, 10);

    entryMessage = gtk_entry_new();
    gtk_table_attach_defaults(GTK_TABLE(pTable), entryMessage, 0, 2, 9, 10);

    canvas = gtk_drawing_area_new();
    gtk_table_attach_defaults(GTK_TABLE(pTable), canvas, 0, 5, 0, 9);

    gtk_widget_show_all (pWindow);

    g_signal_connect(G_OBJECT(pWindow), "destroy", G_CALLBACK (OnDestroy), NULL);
    g_signal_connect(G_OBJECT(btnSendMessage), "released", G_CALLBACK(ui_sendMessage), NULL);
    g_signal_connect(G_OBJECT(btnEmoji), "released", G_CALLBACK(selectEmoji), NULL);
    g_signal_connect(G_OBJECT(canvas), "draw", G_CALLBACK(showHistory), NULL);



    if (client_init("127.0.0.1", 2000) == -1)
    {
        exit(EXIT_FAILURE);
    }

    client_setName(DEBUG_NAME, sizeof(DEBUG_NAME));
    client_setListener(buildClientListener());

    gtk_main ();
    gtk_widget_destroy(pWindow);
}

void onChatMessage(char* senderName, int senderNameLength, char* chatMessage, int chatMessageLength)
{
    //printf("New chat entry by %s: %s\n", senderName, chatMessage);

    char *clonedSenderName = memory_util_deep_copy(senderName, senderNameLength);
    char *clonedChatMessage = memory_util_deep_copy(chatMessage, chatMessageLength);

    chatHistoryEntry entry = {
            senderNameLength,
            clonedSenderName,
            chatMessageLength,
            clonedChatMessage,
            0,
            0
    };
    chat_history_push(entry);

    gtk_chat_history_display();
    gtk_widget_queue_draw(canvas);
}

void onPrivateMessage(char* senderName, int senderNameLength, char* privateMessage, int privateMessageLength)
{
    //printf("New private chat entry by %s: %s\n", senderName, privateMessage);

    char *clonedSenderName = memory_util_deep_copy(senderName, senderNameLength);
    char *clonedPrivateMessage = memory_util_deep_copy(privateMessage, privateMessageLength);

    chatHistoryEntry entry = {
            senderNameLength,
            clonedSenderName,
            privateMessageLength,
            clonedPrivateMessage,
            1,
            0
    };
    chat_history_push(entry);

    gtk_chat_history_display();
    gtk_widget_queue_draw(canvas);
}

void onPrivateMessageDelivered(char* targetName, int targetNameLength, char* privateMessage, int privateMessageLength)
{
    //printf("New private chat entry by %s: %s\n", targetName, privateMessage);

    char *clonedSenderName = memory_util_deep_copy(targetName, targetNameLength);
    char *clonedPrivateMessage = memory_util_deep_copy(privateMessage, privateMessageLength);

    chatHistoryEntry entry = {
            targetNameLength,
            clonedSenderName,
            privateMessageLength,
            clonedPrivateMessage,
            1,
            0
    };
    chat_history_push(entry);

    gtk_chat_history_display();
    gtk_widget_queue_draw(canvas);
}

void onPeerConnectionNotification()
{
    char *notification = "Une personne nous a rejoint !";
    int notificationLength = strlen(notification);

    char *clonedChatMessage = memory_util_deep_copy(notification, notificationLength);

    chatHistoryEntry entry = {
            0,
            NULL,
            notificationLength,
            clonedChatMessage,
            0,
            0
    };
    chat_history_push(entry);

    gtk_chat_history_display();
    gtk_widget_queue_draw(canvas);
}

void onPeerDisconnectionNotification()
{
    char *notification = "Une personne nous a quitté !";
    int notificationLength = strlen(notification);

    char *clonedChatMessage = memory_util_deep_copy(notification, notificationLength);

    chatHistoryEntry entry = {
            0,
            NULL,
            notificationLength,
            clonedChatMessage,
            0,
            0
    };
    chat_history_push(entry);

    gtk_chat_history_display();
    gtk_widget_queue_draw(canvas);
}

void onPrivateMessageFailed(int reason)
{
    chatHistoryEntry entry;
    switch (reason)
    {
        case PRIVATE_MESSAGE_FAILED_USER_NOT_FOUND:
        {
            char *privateMessageError = "Unable to deliver the private message: user not found!";
            int privateMessageErrorLength = strlen(privateMessageError);

            char *clonedPrivateMessageError = memory_util_deep_copy(privateMessageError, privateMessageErrorLength);

            chatHistoryEntry entry = {
                    0,
                    NULL,
                    privateMessageErrorLength,
                    clonedPrivateMessageError,
                    1,
                    1
            };

            chat_history_push(entry);
            gtk_chat_history_display();
            gtk_widget_queue_draw(canvas);

            break;
        }

        case PRIVATE_MESSAGE_FAILED_MESSAGE_TOO_BIG:
        {
            char *privateMessageError = "Unable to deliver the private message: message is too big!";
            int privateMessageErrorLength = strlen(privateMessageError);

            char *clonedPrivateMessageError = memory_util_deep_copy(privateMessageError, privateMessageErrorLength);

            chatHistoryEntry entry = {
                    0,
                    NULL,
                    privateMessageErrorLength,
                    clonedPrivateMessageError,
                    1,
                    1
            };

            chat_history_push(entry);

            gtk_chat_history_display();
            gtk_widget_queue_draw(canvas);

            break;
        }

        default:
        {
            char *privateMessageError = "Unable to deliver the private message";
            int privateMessageErrorLength = strlen(privateMessageError);

            char *clonedPrivateMessageError = memory_util_deep_copy(privateMessageError, privateMessageErrorLength);

            chatHistoryEntry entry = {
                    0,
                    NULL,
                    privateMessageErrorLength,
                    clonedPrivateMessageError,
                    1,
                    1
            };

            chat_history_push(entry);

            gtk_chat_history_display();
            gtk_widget_queue_draw(canvas);
        }
    }
}

clientListener buildClientListener()
{
    clientListener clientListener;

    clientListener.onChatMessage = onChatMessage;
    clientListener.onPrivateMessage = onPrivateMessage;
    clientListener.onPrivateMessageDelivered = onPrivateMessageDelivered;
    clientListener.onPeerConnectionNotification = onPeerConnectionNotification;
    clientListener.onPeerDisconnectionNotification = onPeerDisconnectionNotification;
    clientListener.onPrivateMessageFailed = onPrivateMessageFailed;

    return clientListener;
}

int main(int argc,char **argv)
{
    goToGTK(argc, argv);
    return EXIT_SUCCESS;
}
