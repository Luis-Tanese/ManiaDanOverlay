#ifndef MANIADANOVERLAY_LOCAL_HTTP_H
#define MANIADANOVERLAY_LOCAL_HTTP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    int status_code;
    unsigned char *body;
    size_t body_size;
} LocalHttpResponse;

bool LocalHttpInit(void);
void LocalHttpShutdown(void);

bool LocalHttpGet(
    const char *path,
    int connect_timeout_ms,
    int io_timeout_ms,
    LocalHttpResponse *out_response,
    char *error,
    size_t error_size
);

void LocalHttpResponseFree(
    LocalHttpResponse *response
);

#endif
