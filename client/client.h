#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <semaphore.h>
#include <gtk/gtk.h>

#define stdBufferSize 1000
#define maxDataLength 1000
#define HANDLE_MESSAGE_SUCCESS 0
#define HANDLE_MESSAGE_ERROR_UNKNOWN_MESSAGE -1

#define PRIVATE_MESSAGE_FAILED_USER_NOT_FOUND 1
#define PRIVATE_MESSAGE_FAILED_MESSAGE_TOO_BIG 2

typedef enum MessageType messageType;

enum MessageType
{
    CHAT_MESSAGE,                    // le client demande l'envoi d'un message dans le chat.
    CHAT_ENTRY_MESSAGE,              // le serveur informe les clients d'un nouveau message dans le chat.
    CHAT_ENTRY_LIST_MESSAGE,         // le serveur envoie au client après sa connexion l'historique des messages du chat.
    PRIVATE_MESSAGE,                 // le client demande l'envoi d'un message à un utilisateur précis
    PRIVATE_MESSAGE_ENTRY,           // le serveur informe le client qu'il a recu un message de l'utilisateur x.
    PRIVATE_MESSAGE_FAILED,          // le serveur informe le client que le message privé n'a pas pu être délivré.
    PRIVATE_MESSAGE_DELIVERED,       // le serveur informe le client qu'un message privé a été délivré.
    KEEP_ALIVE,                      // le client envoie un KEEP_ALIVE pour garder la connexion ouverte.
    KEEP_ALIVE_SERVER,               // le serveur renvoie une réponse pour le KEEP_ALIVE afin d'informer le client que la connexion est toujours ouverte.
    PEER_CONNECTION_NOTIFICATION,    // le client envoie aux clients qu'une personne s'est connectée.
    PEER_DISCONNECTION_NOTIFICATION, // le serveur envoie aux clients qu'une personne s'est déconnectée.
    DEBUG_SET_NAME,                  // le client définie son nom (a des fins de test car le nom de doit pas être stocké sur le serveur mais dans la bdd).
    DEBUG_SET_DATE,                  // le client admin défini la date et l'heure de reception des messages de l'historique.
};

typedef struct MessageHeader messageHeader;

struct MessageHeader
{
    messageType type;
    unsigned int length;
};

typedef struct ClientListener clientListener;

struct ClientListener
{
    void (*onChatMessage)(char* senderName, int senderNameLength, char* chatMessage, int chatMessageLength);
    void (*onPrivateMessage)(char* senderName, int senderNameLength, char* privateMessage, int privateMessageLength);
    void (*onPrivateMessageFailed)(int reason);
    void (*onPrivateMessageDelivered)(char* targetName, int targetNameLength, char* privateMessage, int privateMessageLength);
    void (*onPeerConnectionNotification)();
    void (*onPeerDisconnectionNotification)();
};

void client_setName(char* name, int nameLength);
void client_sendMessage(char* chatMessage, int chatMessageLength);
int client_init(char* serverIP, int serverPort);
void client_setListener(clientListener listener);

#endif // CLIENT_H
