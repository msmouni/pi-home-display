#include "lafvin_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct lafvin_ctx {
    int server_fd;
    int consumer_fd;

    char socket_path[128];
    char json[256];
};

static int lafvin_display_init(void *ctx)
{
    if (!ctx)
        return -1;

    struct lafvin_ctx *lafvin = ctx;

    /* Create UNIX domain socket */
    lafvin->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (lafvin->server_fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, lafvin->socket_path, sizeof(addr.sun_path) - 1);

    unlink(lafvin->socket_path); // Remove existing socket file

    if (bind(lafvin->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(lafvin->server_fd);
        return -1;
    }

    if (listen(lafvin->server_fd, 5) < 0) {
        perror("listen");
        close(lafvin->server_fd);
        return -1;
    }

    printf("Lafvin display IPC server started at %s\n", lafvin->socket_path);

    // TODO: Handle client connections in a separate thread or use non-blocking
    printf("Waiting for client...\n");
    lafvin->consumer_fd = accept(lafvin->server_fd, NULL, NULL);

    printf("Client connected\n");

    return 0;
}

static void lafvin_display_clear(void *ctx) { /* TODO */ (void)ctx; }

static void lafvin_display_update(void *ctx, const display_model_t *model)
{
    if (!ctx)
        return;

    struct lafvin_ctx *lafvin = ctx;

    display_model_to_json(model, lafvin->json, sizeof(lafvin->json));
    send(lafvin->consumer_fd, lafvin->json, strlen(lafvin->json), 0);
}

static void lafvin_display_destroy(void *ctx)
{
    if (!ctx)
        return;

    struct lafvin_ctx *lafvin = ctx;

    close(lafvin->consumer_fd);
    close(lafvin->server_fd);
    unlink(lafvin->socket_path);

    free(lafvin);
}

display_t *lafvin_display_create(char *socket_path)
{
    display_t *disp = malloc(sizeof(display_t));
    if (!disp)
        return NULL;

    struct lafvin_ctx *ctx = malloc(sizeof(struct lafvin_ctx));
    if (!ctx) {
        free(disp);
        return NULL;
    }

    snprintf(ctx->socket_path, sizeof(ctx->socket_path), "%s", socket_path);

    disp->type = DISPLAY_LAFVIN;
    disp->ctx = ctx;
    disp->ops = &(display_ops_t){
        .init = lafvin_display_init,
        .clear = lafvin_display_clear,
        .update = lafvin_display_update,
        .destroy = lafvin_display_destroy,
    };

    return disp;
}