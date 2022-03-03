/******************/
/* Socket Serveur */
/******************/

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>

#define numPort 2000

#define maxDataLength 1000
#define stdBufferSize 1000
#define chatMaxMessageSize 200
#define chatHistoryCapacity 20

#define SOCKET_MAX_KEEP_ALIVE_TTL 30
#define HANDLE_MESSAGE_SUCCESS 0
#define HANDLE_MESSAGE_ERROR_UNKNOWN_MESSAGE -1
#define HANDLE_MESSAGE_ERROR_BAD_REQUEST -2

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
    uint length;
};

typedef struct Client client;

struct Client
{
    unsigned int numThread;
    pthread_t *thread;
    int descripteur;
    time_t lastKeepAlive;
    int uid;
    char *debugName;
};

typedef struct ConnexionClient connexionsclient;

struct ConnexionClient
{
    client **tab;
    unsigned int tabSize;
    unsigned int nbConnexions;
    sem_t mutex;
};

typedef struct ChatHistoryEntry chatHistoryEntry;

struct ChatHistoryEntry
{
    char *content;
    char *senderName;
};

typedef struct ChatHistory chatHistory;

struct ChatHistory
{
    chatHistoryEntry *messages[chatHistoryCapacity];
    int size;
    sem_t mutex;
};

chatHistory ChatHistory;
connexionsclient connexionsClient = {NULL, 0, 0, {}};
int debugUidCounter = 0;
int numFile = 0;

chatHistoryEntry *popFromChatHistory_NoLock();
void pushToChatHistory(chatHistoryEntry *entry);
void pushChatHistoryToFile();
void AddClientConnection();
void DestroyConnectionClient(client *pclient);
client *ConnexionClientCreer(unsigned int num, int desc);
void DeleteClient(client *connexion);
void *RecvClientMessage(void *pt);
client *getClientByUID(int uid);
client *debugGetClientByName(char *name);
void *KeepAliveCheckerThreadFunc();
void sendMessage(client *client, messageType type, char *data, int length);
int handleMessage(client *client, messageType type, char *data, int length);
void sendMessageToConnectedClients(messageType type, char *data, int length);
void DeleteConnectionClient();

chatHistoryEntry *popFromChatHistory_NoLock()
{
    chatHistoryEntry *message = ChatHistory.messages[0];

    if (--ChatHistory.size > 0)
    {
        memcpy(&ChatHistory.messages[0], &ChatHistory.messages[1], ChatHistory.size * sizeof(chatHistoryEntry *));
    }

    return message;
}

void pushToChatHistory(chatHistoryEntry *entry)
{
    printf("push\n");

    sem_wait(&ChatHistory.mutex);

    if (ChatHistory.size >= chatHistoryCapacity)
    {
        free(popFromChatHistory_NoLock());
    }

    ChatHistory.messages[ChatHistory.size++] = entry;

    pushChatHistoryToFile();

    sem_post(&ChatHistory.mutex);
}

void pushChatHistoryToFile()
{
    time_t DateMessage;

    DateMessage = time(NULL);

    FILE *file = NULL;

    char *nameFile = NULL;
    char ChatHistoryFile[stdBufferSize];

    int numIndexChatHistory = 0;

    nameFile = (char*)malloc(sizeof(char));

    sprintf(nameFile, "HistoryFile-%d", numFile);

    file = fopen(nameFile, "a+");

    printf("File Name : %s -> %p\n", nameFile, nameFile);

    printf("Size of history : %d\n", ChatHistory.size);

    for (int i = 0; i < ChatHistory.size; i++)
    {
        printf("History -> %s : %s\n", ChatHistory.messages[i]->senderName, ChatHistory.messages[i]->content);

        numIndexChatHistory = i;
    }

    unsigned int lineRead = 0;

    if (file != NULL)
    {
        int i = 0;

        while ((i = fgetc(file)) != EOF)
        {
            if (i == '\n')
            {
                lineRead++;
            }
        }
    }
    else
    {
        printf("Read Error on file : %s\n", nameFile);
    }

    if (file != NULL && lineRead < 500)
    {
        sprintf(ChatHistoryFile, "%s : %s -> %s", ChatHistory.messages[numIndexChatHistory]->senderName, ChatHistory.messages[numIndexChatHistory]->content, ctime(&DateMessage));

        fprintf(file, "%s", ChatHistoryFile);

        fclose(file);
    }
    else
    {
        fclose(file);

        numFile++;

        sprintf(nameFile, "HistoryFile-%d", numFile);

        file = fopen(nameFile, "a");

        if (file != NULL)
        {
            sprintf(ChatHistoryFile, "%s : %s -> %s", ChatHistory.messages[numIndexChatHistory]->senderName, ChatHistory.messages[numIndexChatHistory]->content, ctime(&DateMessage));

            fprintf(file, "%s", ChatHistoryFile);

            fclose(file);
        }
        else
        {
            printf("Wrinting error on file : %s", nameFile);
        }
    }

    free(nameFile);
}

void AddClientConnection(client *pClient)
{
    sem_wait(&connexionsClient.mutex);

    if ((connexionsClient.tabSize - connexionsClient.nbConnexions) <= 0)
    {
        if (connexionsClient.tabSize != 0)
        {
            int newTabSize = connexionsClient.tabSize * 2;

            connexionsClient.tab = (client **)realloc(connexionsClient.tab, sizeof(client *) * newTabSize);
            connexionsClient.tabSize = newTabSize;

            printf("ensure capacity of connection tab array\n");
        }
        else
        {
            connexionsClient.tab = (client **)malloc(sizeof(client *) * 5);
            connexionsClient.tabSize = 5;

            printf("init connection tab array\n");
        }
    }

    printf("add new client to index %d\n", connexionsClient.nbConnexions);

    connexionsClient.tab[connexionsClient.nbConnexions++] = pClient;

    sem_post(&connexionsClient.mutex);
}

void DestroyConnectionClient(client *pClient)
{
    sem_wait(&connexionsClient.mutex);

    for (unsigned int i = 0; i < connexionsClient.nbConnexions; i++)
    {
        if (connexionsClient.tab[i] == pClient)
        {
            if (--connexionsClient.nbConnexions > 0)
            {
                memcpy(&connexionsClient.tab[i], &connexionsClient.tab[i + 1], (connexionsClient.nbConnexions - i) * sizeof(client *));
            }

            break;
        }
    }

    sem_post(&connexionsClient.mutex);
}

client *ConnexionClientCreer(unsigned int num, int desc)
{
    client *connexion = (client *)malloc(sizeof(client));

    if (connexion)
    {
        connexion->numThread = num;
        connexion->descripteur = desc;
        connexion->thread = (pthread_t *)malloc(sizeof(pthread_t));
        connexion->lastKeepAlive = time(NULL);
        connexion->uid = debugUidCounter++;
        connexion->debugName = NULL;

        if (connexion->thread == NULL)
        {
            perror("Erreur de creation de la structure pthread\n");

            free(connexion);

            connexion = NULL;
        }
        else
        {
            printf("Creation du gestionnaire de connexion client numero %u avec le descripteur %d\n", connexion->numThread, connexion->descripteur);

            AddClientConnection(connexion);

            sendMessageToConnectedClients(PEER_CONNECTION_NOTIFICATION, NULL, 0);

            /*sem_wait(&ChatHistory.mutex);

            char chatHistory[(chatMaxMessageSize + 1) * ChatHistory.size];

            int chatHistoryLength = 0;

            printf("num: %d\n", ChatHistory.size);

            for (int i = 0; i < ChatHistory.size; i++)
            {
                char* message = ChatHistory.messages[i];

                int messageLength = strlen(message);

                printf("%s %d\n", message, messageLength);

                memcpy(&chatHistory[chatHistoryLength], message, messageLength);

                chatHistory[messageLength] = '\n';
                chatHistoryLength += messageLength + 1;
            }

            sem_post(&ChatHistory.mutex);

            sendMessage(connexion, CHAT_ENTRY_LIST_MESSAGE, chatHistory, chatHistoryLength);*/

            sem_wait(&ChatHistory.mutex);

            for (int i = 0; i < ChatHistory.size; i++)
            {
                chatHistoryEntry *entry = ChatHistory.messages[i];

                char *name = entry->senderName;

                int nameLength = name != NULL ? strlen(name) : 0;

                char *message = entry->content;

                int messageSize = strlen(message);

                int chatEntryDataSize = nameLength + messageSize + 1;

                char chatEntryData[chatEntryDataSize];

                memcpy(chatEntryData, name, nameLength);
                memcpy(&chatEntryData[nameLength + 1], message, messageSize);

                chatEntryData[nameLength] = '\0';

                sendMessage(connexion, CHAT_ENTRY_MESSAGE, chatEntryData, chatEntryDataSize);
            }

            sem_post(&ChatHistory.mutex);
        }
    }
    else
    {
        perror("Erreur de creation de la structure Client");
    }

    return connexion;
}

void DeleteClient(client *connexion)
{
    if (connexion)
    {
        if (connexion->thread)
        {
            free(connexion->thread);
        }

        if (connexion->debugName != NULL)
        {
            free(connexion->debugName);
        }

        close(connexion->descripteur);

        free(connexion);

        DestroyConnectionClient(connexion);

        sendMessageToConnectedClients(PEER_DISCONNECTION_NOTIFICATION, NULL, 0);

        printf("Destruction du gestionnaire client numero %u avec le descripteur %d\n", connexion->numThread, connexion->descripteur);
    }
}

void *RecvClientMessage(void *pt)
{
    while (1)
    {
        client *pConnexion = (client *)pt;
        client connexion = *((client *)pt);

        printf("Connexion client (numThread = %d) : activé\n", connexion.numThread);

        messageHeader header;

        if (recv(connexion.descripteur, &header, sizeof(messageHeader), 0) <= 0)
        {
            DeleteClient(pConnexion);

            pthread_exit(NULL);
        }

        if (header.length >= maxDataLength)
        {
            printf("Connexion client (numThread = %d) : A essaye d'envoyer trop de données ! (type: %d length: %d)\n", connexion.numThread, header.type, header.length);
        }

        char data[header.length + 1];

        if (header.length != 0)
        {
            if (recv(connexion.descripteur, data, header.length, 0) <= 0)
            {
                DeleteClient(pConnexion);

                pthread_exit(NULL);
            }
        }

        data[header.length] = '\0';

        int handleResult = handleMessage(pConnexion, header.type, data, header.length);

        if (handleResult != HANDLE_MESSAGE_SUCCESS)
        {
            printf("Connexion client (numThread = %d) : La gestion du message a échouée. Error %d (type: %d length: %d)\n", connexion.numThread, handleResult, header.type, header.length);

            DeleteClient(pConnexion);

            pthread_exit(NULL);
        }
    }
}

client *getClientByUID(int uid)
{
    sem_wait(&connexionsClient.mutex);

    client *targetClient = NULL;

    for (unsigned int i = 0; i < connexionsClient.nbConnexions; i++)
    {
        client *client = connexionsClient.tab[i];

        if (client->uid == uid)
        {
            targetClient = client;
            
            break;
        }
    }

    sem_post(&connexionsClient.mutex);

    return targetClient;
}

client *debugGetClientByName(char *name)
{
    sem_wait(&connexionsClient.mutex);

    client *targetClient = NULL;

    for (unsigned int i = 0; i < connexionsClient.nbConnexions; i++)
    {
        client *client = connexionsClient.tab[i];

        if (client->debugName != NULL)
        {
            if (strcmp(client->debugName, name) == 0)
            {
                targetClient = client;
                
                break;
            }
        }
    }

    sem_post(&connexionsClient.mutex);

    return targetClient;
}

void *KeepAliveCheckerThreadFunc()
{
    while (1)
    {
        sem_wait(&connexionsClient.mutex);

        for (unsigned int i = 0; i < connexionsClient.nbConnexions; i++)
        {
            client *client = connexionsClient.tab[i];
            time_t currentTime = time(NULL);

            if (currentTime - client->lastKeepAlive >= SOCKET_MAX_KEEP_ALIVE_TTL)
            {
                printf("Client %d dead. Disconnected.\n", client->numThread);

                DeleteClient(client);
            }
        }

        sem_post(&connexionsClient.mutex);

        sleep(5);
    }
}

void sendMessage(client *client, messageType type, char *data, int length)
{
    messageHeader header = {type, length};

    if (length >= maxDataLength)
    {
        printf("Impossible d'envoyer le message au client: le nombre de données est trop grand ! (type: %d length: %d)\n", header.type, length);

        return;
    }

    send(client->descripteur, &header, sizeof(messageHeader), 0);

    if (header.length != 0)
    {
        send(client->descripteur, data, length, 0);
    }
}

int isEmptyMessage(char *message, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (message[i] > ' ')
        {
            return 0;
        }        
    }

    return 1;
}


int handleMessage(client *sender, messageType type, char *data, int length)
{
    printf("Connexion client (PID = %d) : Reception de %d, \"%s\"\n", getpid(), type, data);

    switch (type)
    {
        case CHAT_MESSAGE:
        {
            // char chatResponse[stdBufferSize];

            // printf("Repondre au client (PID = %d) : ", getpid());
            // fgets(chatResponse, stdBufferSize, stdin);
            // sendMessage(sender, CHAT_ENTRY_MESSAGE, chatResponse, strlen(chatResponse));

            if (length > 0 && length < chatMaxMessageSize)
            {
                int messageSize = length;
                char *message = (char *)malloc(messageSize);
                memcpy(message, data, messageSize);

                if (isEmptyMessage(message, messageSize)) {
                    return HANDLE_MESSAGE_SUCCESS;
                }

                char *name = sender->debugName;
                int nameLength = name != NULL ? strlen(name) : 0;

                int chatEntryDataSize = nameLength + messageSize + 1;
                char chatEntryData[chatEntryDataSize];

                memcpy(chatEntryData, name, nameLength);
                memcpy(&chatEntryData[nameLength + 1], message, messageSize);

                chatEntryData[nameLength] = '\0';

                sendMessageToConnectedClients(CHAT_ENTRY_MESSAGE, chatEntryData, chatEntryDataSize);

                char *nameCopy = NULL;

                if (nameLength != 0)
                {
                    nameCopy = malloc(nameLength + 1);

                    memcpy(nameCopy, name, nameLength);

                    nameCopy[nameLength] = '\0';
                }

                chatHistoryEntry *entry = (chatHistoryEntry *)malloc(sizeof(chatHistoryEntry));

                entry->senderName = nameCopy;
                entry->content = message;

                pushToChatHistory(entry);
            }

            return HANDLE_MESSAGE_SUCCESS;
        }

        case PRIVATE_MESSAGE:
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
                printf("Unable to find the separator in PRIVATE_MESSAGE: %s\n", data);
                return HANDLE_MESSAGE_ERROR_BAD_REQUEST;
            }

            if (separator == 0)
            {
                printf("Empty target name in PRIVATE_MESSAGE: %s\n", data);

                return HANDLE_MESSAGE_ERROR_BAD_REQUEST;
            }

            int targetSize = separator;
            int messageSize = length - separator - 1;

            if (messageSize == 0)
            {
                printf("Empty chat entry in PRIVATE_MESSAGE: %s\n", data);

                return HANDLE_MESSAGE_ERROR_BAD_REQUEST;
            }

            if (messageSize > chatMaxMessageSize)
            {
                printf("Message too big! (length: %d max: %d)\n", messageSize, chatMaxMessageSize);

                char failedData[1];

                failedData[0] = PRIVATE_MESSAGE_FAILED_MESSAGE_TOO_BIG;

                sendMessage(sender, PRIVATE_MESSAGE_FAILED, failedData, 1);

                return HANDLE_MESSAGE_SUCCESS;
            }

            char targetUsername[targetSize + 1];
            char message[messageSize + 1];

            targetUsername[targetSize] = '\0';
            message[messageSize] = '\0';

            memcpy(targetUsername, data, targetSize);
            memcpy(message, &data[separator + 1], messageSize);

            // Seulement pour les tests. Une requête dans la base de données devra être faite pour obtenir l'id de l'utilisateur à l'aide du nom spécifié (SELECT uid FROM users WHERE name = 'targetUsername')
            client *target = debugGetClientByName(targetUsername);
            // FIN

            if (target == NULL)
            {
                printf("PRIVATE_MESSAGE: User %s not found.\n", targetUsername);

                char failedData[1 + targetSize];

                failedData[0] = PRIVATE_MESSAGE_FAILED_USER_NOT_FOUND;

                memcpy(&failedData[1], targetUsername, targetSize);
                sendMessage(sender, PRIVATE_MESSAGE_FAILED, failedData, 1 + targetSize);

                return HANDLE_MESSAGE_SUCCESS;
            }

            printf("PRIVATE_MESSAGE: targetUsername: %s message: %s\n", targetUsername, message);

            char *name = sender->debugName;
            int nameLength = name != NULL ? strlen(name) : 0;

            int privateMessageEntrySize = nameLength + messageSize + 1;
            char privateMessageEntry[privateMessageEntrySize];

            memcpy(privateMessageEntry, name, nameLength);
            memcpy(&privateMessageEntry[nameLength + 1], message, messageSize);

            privateMessageEntry[nameLength] = '\0';

            sendMessage(target, PRIVATE_MESSAGE_ENTRY, privateMessageEntry, privateMessageEntrySize);

            int privateMessageDeliveredEntrySize = targetSize + messageSize + 1;
            char privateMessageDeliveredEntry[privateMessageDeliveredEntrySize];

            memcpy(privateMessageDeliveredEntry, name, targetSize);
            memcpy(&privateMessageDeliveredEntry[targetSize + 1], message, messageSize);

            privateMessageDeliveredEntry[targetSize] = '\0';

            sendMessage(sender, PRIVATE_MESSAGE_DELIVERED, privateMessageDeliveredEntry, privateMessageDeliveredEntrySize);

            return HANDLE_MESSAGE_SUCCESS;
        }

        case KEEP_ALIVE:
        {
            sender->lastKeepAlive = time(NULL);

            sendMessage(sender, KEEP_ALIVE_SERVER, NULL, 0);

            return HANDLE_MESSAGE_SUCCESS;
        }

        case DEBUG_SET_NAME:
        {
            char *debugName = malloc(length + 1);

            strcpy(debugName, data);

            debugName[length] = '\0';
            sender->debugName = debugName;

            printf("DEBUG_SET_NAME: %s (uid %d)\n", sender->debugName, sender->uid);

            return HANDLE_MESSAGE_SUCCESS;
        }

        case DEBUG_SET_DATE:
        {
            printf("DEBUG_SET_DATE\n");
        }

        default:
        {
            return HANDLE_MESSAGE_ERROR_UNKNOWN_MESSAGE;
        }
    }
}

void sendMessageToConnectedClients(messageType type, char *data, int length)
{
    sem_wait(&connexionsClient.mutex);

    for (unsigned int i = 0; i < connexionsClient.nbConnexions; i++)
    {
        sendMessage(connexionsClient.tab[i], type, data, length);
    }

    sem_post(&connexionsClient.mutex);
}

void DeleteConnectionClient()
{
    sem_wait(&connexionsClient.mutex);

    for (unsigned int i = 0; i < connexionsClient.nbConnexions; i++)
    {
        DeleteClient(connexionsClient.tab[i]);
    }

    sem_post(&connexionsClient.mutex);

    printf("Destruction du tableau de gestionnaires de connexion client\n");
}

int main()
{
    sem_init(&connexionsClient.mutex, PTHREAD_PROCESS_SHARED, 1);
    sem_init(&ChatHistory.mutex, PTHREAD_PROCESS_SHARED, 1);

    struct sockaddr_in adresseDuServeur;

    adresseDuServeur.sin_family = PF_INET;
    adresseDuServeur.sin_port = htons(numPort);
    adresseDuServeur.sin_addr.s_addr = INADDR_ANY;

    /* Etape 2 */
    /***********/

    int descripteurDeSocketServeur = socket(PF_INET, SOCK_STREAM, 0);

    if (descripteurDeSocketServeur < 0)
    {
        printf("Problemes pour creer la socket!\n");

        return -1;
    }

    printf("Socket cree\n");

    /* Etape 3 */
    /***********/

    if (bind(descripteurDeSocketServeur, (struct sockaddr *)&adresseDuServeur, sizeof(struct sockaddr_in)) < 0)
    {
        printf("Problemes pour faire le bind!\n");

        return -1;
    }

    printf("Socket liee\n");

    /* Etape 4 */
    /***********/

    if (listen(descripteurDeSocketServeur, 1) < 0)
    {
        printf("Problemes pour faire le listen!\n");

        return -1;
    }

    pthread_t keepAliveCheckerThreadFunc;

    if (pthread_create(&keepAliveCheckerThreadFunc, NULL, KeepAliveCheckerThreadFunc, NULL) != 0)
    {
        perror("serveur : Erreur de lancement du keepalivechecker thread\n");
        
        return -1;
    }

    while (1)
    {
        int descripteurDeSocketClient;
        struct sockaddr_in adresseDuClient;
        unsigned int longueurDeAdresseDuClient;

        printf("Serveur (PID = %d) : En attente...\n", getpid());

        descripteurDeSocketClient = accept(descripteurDeSocketServeur, (struct sockaddr *)&adresseDuClient, &longueurDeAdresseDuClient);

        if (descripteurDeSocketClient < 0)
        {
            perror("Serveur: Erreur accept\n");

            return -1;
        }
        else
        {
            client *connexion = ConnexionClientCreer(longueurDeAdresseDuClient, descripteurDeSocketClient);

            if (connexion)
            {
                if (pthread_create(connexion->thread, NULL, RecvClientMessage, connexion) != 0)
                {
                    perror("serveur : Erreur de lancement du thread\n");

                    return -1;
                }
            }
        }
    }

    return 0;
}
