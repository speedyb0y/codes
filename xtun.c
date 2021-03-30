/*
    gcc -Wall -Werror -fwhole-program xtun.c -o xtun
*/

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/un.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct sockaddr sockaddr;
typedef struct sockaddr_un sockaddr_un;

#define TUNNEL_PORT 55555

#define IO_CLEAR   0xfffffbffU // NOT WAITING, NOT RESULT
#define IO_WAITING 0xfffffc00U
#define IO_ERR     0xfffffc01U

#define ACCEPT_N 128

typedef struct client_s client_s;

struct client_s {
    client_s* next;
    client_s* prev;
    //
    u16 remove;
    u16 fd;
    u32 fdCloseRes;
    u32 iReadRes;
    u32 iWriteRes;
    u32 oReadRes;
    u32 oWriteRes;
    void* iRead;
    void* iWrite;
    void* oRead;
    void* oWrite;
    //
    u32 iReadCancel;
    u32 iWriteCancel;
    u32 oReadCancel;
    u32 oWriteCancel;
};

int main (void) {

    client_s* clients = NULL;

    u32 acceptedFDs[ACCEPT_N];

    // WAIT FOR CONNECTIONS
    const int sockListen = socket(AF_UNIX, SOCK_DGRAM, 0);

    if (sockListen == -1) {
        printf("ERROR: FAILED TO OPEN LISTENING SOCKET: %s\n", strerror(errno));
        return 1;
    }

    // PREVENT EADDRINUSE
    const int sockOptReuseAddr = 1;

    if (setsockopt(sockListen, SOL_SOCKET, SO_REUSEADDR, (char*)&sockOptReuseAddr, sizeof(sockOptReuseAddr)) < 0) {
        printf("ERROR: FAILED TO SET REUSE ADDR: %s\n", strerror(errno));
        return 1;
    }

    const sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = "\x00xtun\x00" };

    if (bind(sockListen, (sockaddr*)&addr, sizeof(addr))) {
        printf("ERROR: FAILED TO BIND: %s\n", strerror(errno));
        return 1;
    }

    if (listen(sockListen, 65536)) {
        printf("ERROR: FAILED TO LISTEN FOR CLIENTS: %s\n", strerror(errno));
        return 1;
    }

    for (uint i = 0; i != ACCEPT_N; i++)
        acceptedFDs[i] = accept(sockListen, NULL, 0);

    while (1) {

        // NEW CLIENTS
        for (uint i = 0; i != ACCEPT_N; i++) {

            //acceptedFDs[i] = accept(sockListen, NULL, 0);

            if (acceptedFDs[i] < IO_ERR) {
                // NEW CLIENT

                client_s* const client = malloc(sizeof(client_s));

                client->remove         = 0;
                client->fd             = acceptedFDs[i];
                client->iRead         = malloc(512*1024);
                client->iWrite        = malloc(512*1024);
                client->oRead        = malloc(512*1024);
                client->oWrite       = malloc(512*1024);
                client->iReadRes      = IO_CLEAR;
                client->iWriteRes     = IO_CLEAR;
                client->oReadRes     = IO_CLEAR;
                client->oWriteRes    = IO_CLEAR;
                client->fdCloseRes     = IO_CLEAR;
                client->iReadCancel   = IO_CLEAR;
                client->iWriteCancel  = IO_CLEAR;
                client->oReadCancel  = IO_CLEAR;
                client->oWriteCancel = IO_CLEAR;
                client->prev           = clients;
                client->next           = NULL;

                if (clients)
                    clients->next = client;

                clients = client;

            } else {
                printf("ERROR: FAILED TO ACCEPT CLIENT: %s\n", strerror(acceptedFDs[i] - IO_ERR));
            }

            // TODO: FIXME: ENQUEUE ANOTHER ACCEPT() AT THIS SLOT
        }

        client_s* client = clients;

        while (client) {

            client_s* const prev = client->prev;

            if (!client->remove) {
                // REDIRECIONA

            }

            // REMOVE CLIENT
            if (client->remove) {

                if (client->fd &&
                    client->fdCloseRes != IO_CLEAR &&
                    client->fdCloseRes != IO_WAITING) {
                    // TODO: FIXME: COLOCAR COMO EM ALGUM CASO, PODE TER QUE MANDAR DE NOVO?
                    // SE FOR, FILTRAR NESTE IF
                    if (1) {
                        client->fd = 0;
                        client->fdCloseRes = IO_CLEAR;
                    }
                }

                if (client->iReadRes == IO_WAITING &&
                    client->iReadCancel != IO_WAITING) {
                    // CANCEL IT
                }

                if (client->iWriteRes == IO_WAITING &&
                    client->iWriteCancel != IO_WAITING) {

                }

                if (client->oReadRes == IO_WAITING &&
                    client->oReadCancel != IO_WAITING) {
                    // CANCEL IT
                }

                if (client->oWriteRes == IO_WAITING &&
                    client->oWriteCancel != IO_WAITING) {

                }

                // TODO: FIXME: se close() der erro, tem que desconsiderar mesmo assim o fd?
                if (client->fd &&
                    client->fdCloseRes != IO_WAITING) {
                    // TODO: SUBMETER O CLOSE AO URING
                    //close(client->fd);
                }

                if (client->fd           == 0 &&
                    client->fdCloseRes   != IO_WAITING &&
                    client->iReadRes     != IO_WAITING &&
                    client->iReadCancel  != IO_WAITING &&
                    client->iWriteRes    != IO_WAITING &&
                    client->iWriteCancel != IO_WAITING &&
                    client->oReadRes     != IO_WAITING &&
                    client->oReadCancel  != IO_WAITING &&
                    client->oWriteRes    != IO_WAITING &&
                    client->oWriteCancel != IO_WAITING)  {
                    // TERMINA DE REMOVER ELE

                    free(client->iRead);
                    free(client->iWrite);
                    free(client->oRead);
                    free(client->oWrite);

                    if (client->next)
                        client->next->prev = client->prev;

                    if (client->prev)
                        client->prev->next = client->next;
                    else
                        clients = NULL;

                    free(client);
                }
            }

            client = prev;
        }
    }

    return 0;
}
