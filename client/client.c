/*****************/
/* Socket Client */
/*****************/

#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "client.h"

void client_sendChatMessage(char *chatMessage, int chatMessageLength);
void client_sendPrivateMessage(char *username, int usenamelength, char *message, int messagelength);
void client_sendTCPMessage(messageType type, char *data, int length);
int client_handleTCPMessage(messageType type, char *data, int length);

int client_socket;
clientListener client_listener;

void *client_keepAliveFunc()
{
    while (1)
    {
        sleep(5);
        client_sendTCPMessage(KEEP_ALIVE, NULL, 0);
    }
}

void *client_tcpMessageHandlerFunc()
{
    while (1)
    {
        messageHeader header;

        if (recv(client_socket, &header, sizeof(messageHeader), 0) < 0)
        {
            return NULL;
        }

        if (header.length >= maxDataLength)
        {
            printf("Essaye de recevoir trop de données ! (type: %d length: %d)\n", header.type, header.length);
            return NULL;
        }

        char data[header.length + 1];

        if (header.length != 0)
        {
            if (recv(client_socket, data, header.length, 0) < 0)
            {
                return NULL;
            }
        }

        data[header.length] = '\0';

        int handleResult = client_handleTCPMessage(header.type, data, header.length);

        if (handleResult != HANDLE_MESSAGE_SUCCESS)
        {
            printf("La gestion du message a échouée. Error %d (type: %d length: %d)\n", handleResult, header.type, header.length);
            return NULL;
        }
    }
}

void client_setName(char* name, int nameLength)
{
    client_sendTCPMessage(DEBUG_SET_NAME, name, nameLength);
}

void client_sendMessage(char* chatMessage, int chatMessageLength)
{
    if (chatMessageLength == 0)
    {
        return;
    }

    if (chatMessage[0] == '@')
    {
        int separator = -1;

        for (int i = 0; i < chatMessageLength; i++)
        {
            if (chatMessage[i] == ' ')
            {
                separator = i;
                break;
            }
        }

        if (separator != -1)
        {
            int targetUsernameLength = separator - 1;
            int messageLength = chatMessageLength - separator - 1;

            char targetUsername[targetUsernameLength];
            char message[messageLength];

            memcpy(targetUsername, &chatMessage[1], targetUsernameLength);
            memcpy(message, &chatMessage[separator + 1], messageLength);

            client_sendPrivateMessage(targetUsername, targetUsernameLength, message, messageLength);

            return;
        }
    }

    client_sendChatMessage(chatMessage, chatMessageLength);
}

void client_sendChatMessage(char *chatMessage, int chatMessageLength)
{
    client_sendTCPMessage(CHAT_MESSAGE, chatMessage, chatMessageLength);
}

void client_sendPrivateMessage(char *username, int usenamelength, char *message, int messagelength)
{
    int dataSize = usenamelength + messagelength + 1;

    char data[dataSize];

    memcpy(data, username, usenamelength);
    memcpy(&data[usenamelength + 1], message, messagelength);

    data[usenamelength] = '\0';

    client_sendTCPMessage(PRIVATE_MESSAGE, data, dataSize);
}

void client_sendTCPMessage(messageType type, char *data, int length)
{
    messageHeader header = {type, length};

    if (length >= maxDataLength)
    {
        printf("Impossible d'envoyer le message au client: le nombre de données est trop grand ! (type: %d length: %d)\n", header.type, length);

        return;
    }

    send(client_socket, &header, sizeof(messageHeader), 0);

    if (header.length != 0)
    {
        send(client_socket, data, header.length, 0);
    }
}

int client_handleTCPMessage(messageType type, char *data, int length)
{

    switch (type)
    {
        case CHAT_ENTRY_MESSAGE:
        {
            int separator = -1;

            for (int i = 0; i < length; i++)
            {
                if (data[i] == '\0')
                {
                    separator = i;

                    break;
                }
            }

            if (separator == -1)
            {
                printf("Unable to find the separator in CHAT_ENTRY_MESSAGE: %s\n", data);

                return HANDLE_MESSAGE_SUCCESS;
            }

            int senderUsernameSize = separator;
            int messageSize = length - separator - 1;

            if (messageSize == 0)
            {
                printf("Empty chat entry in CHAT_ENTRY_MESSAGE: %s\n", data);

                return HANDLE_MESSAGE_SUCCESS;
            }

            char senderUsername[senderUsernameSize + 1];
            char message[messageSize + 1];

            senderUsername[senderUsernameSize] = '\0';
            message[messageSize] = '\0';

            memcpy(senderUsername, data, senderUsernameSize);
            memcpy(message, &data[separator + 1], messageSize);

            if (client_listener.onChatMessage != NULL)
            {
                client_listener.onChatMessage(senderUsername, senderUsernameSize, message, messageSize);
            }

            return HANDLE_MESSAGE_SUCCESS;
        }

        case CHAT_ENTRY_LIST_MESSAGE:
        {
            printf("CHAT_ENTRY_LIST_MESSAGE: \n%s", data);
            return HANDLE_MESSAGE_SUCCESS;
        }

        case PRIVATE_MESSAGE_ENTRY:
        {
            int separator = -1;

            for (int i = 0; i < length; i++)
            {
                if (data[i] == '\0')
                {
                    separator = i;

                    break;
                }
            }

            if (separator == -1)
            {
                printf("Unable to find the separator in PRIVATE_MESSAGE_ENTRY: %s\n", data);

                return HANDLE_MESSAGE_SUCCESS;
            }

            int senderUsernameSize = separator;
            int messageSize = length - separator - 1;

            if (messageSize == 0)
            {
                printf("Empty chat entry in PRIVATE_MESSAGE_ENTRY: %s\n", data);

                return HANDLE_MESSAGE_SUCCESS;
            }

            char senderUsername[senderUsernameSize + 1];
            char message[messageSize + 1];

            senderUsername[senderUsernameSize] = '\0';
            message[messageSize] = '\0';

            memcpy(senderUsername, data, senderUsernameSize);
            memcpy(message, &data[separator + 1], messageSize);

            if (client_listener.onPrivateMessage != NULL)
            {
                client_listener.onPrivateMessage(senderUsername, senderUsernameSize, message, messageSize);
            }

            return HANDLE_MESSAGE_SUCCESS;
        }

        case PRIVATE_MESSAGE_FAILED:
        {
            char reason = data[0];
            switch (reason)
            {
                case PRIVATE_MESSAGE_FAILED_USER_NOT_FOUND:
                {
                    char targetName[length - 1];

                    memcpy(targetName, &data[1], length - 1);

                    printf("Unable to deliver the private message: user %s not found!\n", targetName);

                    break;
                }

                case PRIVATE_MESSAGE_FAILED_MESSAGE_TOO_BIG:
                {
                    printf("Unable to deliver the private message: message is too big!\n");

                    break;
                }

                default:
                {
                    printf("Unable to deliver the private message: %d\n", reason);

                    break;
                }
            }

            if (client_listener.onPrivateMessageFailed != NULL)
            {
                client_listener.onPrivateMessageFailed(reason);
            }

            return HANDLE_MESSAGE_SUCCESS;
        }

        case PRIVATE_MESSAGE_DELIVERED:
        {
            int separator = -1;

            for (int i = 0; i < length; i++)
            {
                if (data[i] == '\0')
                {
                    separator = i;

                    break;
                }
            }

            if (separator == -1)
            {
                printf("Unable to find the separator in PRIVATE_MESSAGE_DELIVERED: %s\n", data);

                return HANDLE_MESSAGE_SUCCESS;
            }

            int targetUsernameLength = separator;
            int messageSize = length - separator - 1;

            if (messageSize == 0)
            {
                printf("Empty chat entry in PRIVATE_MESSAGE_DELIVERED: %s\n", data);

                return HANDLE_MESSAGE_SUCCESS;
            }

            char targetUsername[targetUsernameLength + 1];
            char message[messageSize + 1];

            targetUsername[targetUsernameLength] = '\0';
            message[messageSize] = '\0';

            memcpy(targetUsername, data, targetUsernameLength);
            memcpy(message, &data[separator + 1], messageSize);

            if (client_listener.onPrivateMessageDelivered != NULL)
            {
                client_listener.onPrivateMessageDelivered(targetUsername, targetUsernameLength, message, messageSize);
            }

            return HANDLE_MESSAGE_SUCCESS;
        }

        case KEEP_ALIVE_SERVER:
        {
            //printf("KEEP_ALIVE_SERVER received.\n");

            return HANDLE_MESSAGE_SUCCESS;
        }

        case PEER_CONNECTION_NOTIFICATION:
        {
            //printf("Une personne nous a rejoint.\n");

            if (client_listener.onPeerConnectionNotification != NULL)
            {
                client_listener.onPeerConnectionNotification();
            }

            return HANDLE_MESSAGE_SUCCESS;
        }

        case PEER_DISCONNECTION_NOTIFICATION:
        {
            //printf("Une personne nous a quittée.\n");

            if (client_listener.onPeerDisconnectionNotification != NULL)
            {
                client_listener.onPeerDisconnectionNotification();
            }

            return HANDLE_MESSAGE_SUCCESS;
        }

        default:
        {
            return HANDLE_MESSAGE_ERROR_UNKNOWN_MESSAGE;
        }
    }
}

int client_init(char* serverIP, int serverPort)
{
    struct sockaddr_in adr;

    adr.sin_family = PF_INET;
    adr.sin_port = htons(serverPort);

    struct hostent *host;

    if ((host = gethostbyname(serverIP)) == NULL)
    {
        perror("gethostbyname");
        return -1;
    }

    memcpy(&(adr.sin_addr.s_addr), host->h_addr_list[0], host->h_length);

    printf("Connexion vers la machine ");

    unsigned char *adrIP = (unsigned char *)&(adr.sin_addr.s_addr);

    printf("%d.%d.%d.%d sur le port %u\n", *(adrIP + 0), *(adrIP + 1), *(adrIP + 2), *(adrIP + 3), serverPort);

    int socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);

    if (socketDescriptor < 0)
    {
        printf("Problemes pour creer la socket\n");
        return -1;
    }

    printf("socket cree\n");

    if (connect(socketDescriptor, (struct sockaddr *)&adr, sizeof(adr)) < 0)
    {
        printf("Problemes pour se connecter au serveur\n");
        return -1;
    }

    client_socket = socketDescriptor;
    printf("socket connectee\n");

    pthread_t keepAliveThreadId;

    if (pthread_create(&keepAliveThreadId, NULL, client_keepAliveFunc, NULL) != 0)
    {
        perror("Erreur de lancement du thread\n");
        return -1;
    }

    pthread_t tcpMessageHandlerThreadId;

    if (pthread_create(&tcpMessageHandlerThreadId, NULL, client_tcpMessageHandlerFunc, NULL) != 0)
    {
        perror("Erreur de lancement du thread\n");
        return -1;
    }

    return 0;
}

void client_setListener(clientListener listener)
{
    client_listener = listener;
}
