#include "local_http.h"

#include "app/identity.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET LocalSocket;
#define LOCAL_INVALID_SOCKET INVALID_SOCKET
#define LOCAL_SOCKET_ERROR SOCKET_ERROR
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int LocalSocket;
#define LOCAL_INVALID_SOCKET (-1)
#define LOCAL_SOCKET_ERROR (-1)
#endif

#define LOCAL_HTTP_HOST "127.0.0.1"
#define LOCAL_HTTP_PORT 24050
#define LOCAL_HTTP_MAX_RESPONSE (64u * 1024u * 1024u)
#define LOCAL_HTTP_READ_CHUNK 8192u

static int g_http_init_count = 0;

static void set_error(
    char *error,
    size_t error_size,
    const char *message
)
{
    if (!error || error_size == 0)
        return;

    snprintf(
        error,
        error_size,
        "%s",
        message ? message : "Local HTTP error"
    );
}

static void set_socket_error(
    char *error,
    size_t error_size,
    const char *prefix
)
{
    if (!error || error_size == 0)
        return;

#ifdef _WIN32
    snprintf(
        error,
        error_size,
        "%s (WinSock error %d)",
        prefix ? prefix : "Socket error",
        WSAGetLastError()
    );
#else
    snprintf(
        error,
        error_size,
        "%s: %s",
        prefix ? prefix : "Socket error",
        strerror(errno)
    );
#endif
}

static void close_socket(LocalSocket socket_handle)
{
    if (socket_handle == LOCAL_INVALID_SOCKET)
        return;

#ifdef _WIN32
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

static bool socket_set_nonblocking(
    LocalSocket socket_handle,
    bool enabled
)
{
#ifdef _WIN32
    u_long mode = enabled ? 1UL : 0UL;
    return ioctlsocket(socket_handle, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket_handle, F_GETFL, 0);

    if (flags < 0)
        return false;

    if (enabled)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;

    return fcntl(socket_handle, F_SETFL, flags) == 0;
#endif
}

static bool socket_set_io_timeout(
    LocalSocket socket_handle,
    int timeout_ms
)
{
    if (timeout_ms < 1)
        timeout_ms = 1;

#ifdef _WIN32
    DWORD timeout = (DWORD)timeout_ms;

    return
        setsockopt(
            socket_handle,
            SOL_SOCKET,
            SO_RCVTIMEO,
            (const char *)&timeout,
            (int)sizeof(timeout)
        ) == 0 &&
        setsockopt(
            socket_handle,
            SOL_SOCKET,
            SO_SNDTIMEO,
            (const char *)&timeout,
            (int)sizeof(timeout)
        ) == 0;
#else
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    return
        setsockopt(
            socket_handle,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)
        ) == 0 &&
        setsockopt(
            socket_handle,
            SOL_SOCKET,
            SO_SNDTIMEO,
            &timeout,
            sizeof(timeout)
        ) == 0;
#endif
}

static bool socket_connect_loopback(
    LocalSocket socket_handle,
    int timeout_ms,
    char *error,
    size_t error_size
)
{
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(LOCAL_HTTP_PORT);

    if (
        inet_pton(
            AF_INET,
            LOCAL_HTTP_HOST,
            &address.sin_addr
        ) != 1
    )
    {
        set_error(error, error_size, "Failed to prepare Tosu loopback address");
        return false;
    }

    if (!socket_set_nonblocking(socket_handle, true))
    {
        set_socket_error(error, error_size, "Failed to configure nonblocking connect");
        return false;
    }

    int result = connect(
        socket_handle,
        (const struct sockaddr *)&address,
        (int)sizeof(address)
    );

    if (result == 0)
    {
        if (!socket_set_nonblocking(socket_handle, false))
        {
            set_socket_error(error, error_size, "Failed to restore blocking socket mode");
            return false;
        }

        return true;
    }

#ifdef _WIN32
    const int connect_error = WSAGetLastError();
    const bool in_progress =
        connect_error == WSAEWOULDBLOCK ||
        connect_error == WSAEINPROGRESS ||
        connect_error == WSAEINVAL;
#else
    const bool in_progress =
        errno == EINPROGRESS ||
        errno == EWOULDBLOCK;
#endif

    if (!in_progress)
    {
        set_socket_error(error, error_size, "Could not connect to Tosu");
        return false;
    }

    if (timeout_ms < 1)
        timeout_ms = 1;

    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(socket_handle, &write_set);

    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

#ifdef _WIN32
    result = select(0, NULL, &write_set, NULL, &timeout);
#else
    result = select(socket_handle + 1, NULL, &write_set, NULL, &timeout);
#endif

    if (result == 0)
    {
        set_error(error, error_size, "Timed out connecting to Tosu");
        return false;
    }

    if (result < 0)
    {
        set_socket_error(error, error_size, "Tosu connect select failed");
        return false;
    }

    int socket_error = 0;
#ifdef _WIN32
    int socket_error_size = (int)sizeof(socket_error);
#else
    socklen_t socket_error_size = sizeof(socket_error);
#endif

    if (
        getsockopt(
            socket_handle,
            SOL_SOCKET,
            SO_ERROR,
            (char *)&socket_error,
            &socket_error_size
        ) != 0
    )
    {
        set_socket_error(error, error_size, "Could not validate Tosu connection");
        return false;
    }

    if (socket_error != 0)
    {
#ifdef _WIN32
        if (error && error_size > 0)
        {
            snprintf(
                error,
                error_size,
                "Could not connect to Tosu (WinSock error %d)",
                socket_error
            );
        }
#else
        if (error && error_size > 0)
        {
            snprintf(
                error,
                error_size,
                "Could not connect to Tosu: %s",
                strerror(socket_error)
            );
        }
#endif
        return false;
    }

    if (!socket_set_nonblocking(socket_handle, false))
    {
        set_socket_error(error, error_size, "Failed to restore blocking socket mode");
        return false;
    }

    return true;
}

static bool socket_send_all(
    LocalSocket socket_handle,
    const unsigned char *data,
    size_t size,
    char *error,
    size_t error_size
)
{
    size_t offset = 0;

    while (offset < size)
    {
        size_t remaining = size - offset;
        int request_size =
            remaining > (size_t)INT_MAX
                ? INT_MAX
                : (int)remaining;

#ifdef MSG_NOSIGNAL
        int sent = send(
            socket_handle,
            (const char *)data + offset,
            request_size,
            MSG_NOSIGNAL
        );
#else
        int sent = send(
            socket_handle,
            (const char *)data + offset,
            request_size,
            0
        );
#endif

        if (sent <= 0)
        {
            set_socket_error(error, error_size, "Failed sending request to Tosu");
            return false;
        }

        offset += (size_t)sent;
    }

    return true;
}

static const unsigned char *find_bytes(
    const unsigned char *data,
    size_t data_size,
    const char *needle,
    size_t needle_size
)
{
    if (!data || !needle || needle_size == 0 || data_size < needle_size)
        return NULL;

    for (size_t i = 0; i + needle_size <= data_size; ++i)
    {
        if (memcmp(data + i, needle, needle_size) == 0)
            return data + i;
    }

    return NULL;
}

static int ascii_lower(int value)
{
    if (value >= 'A' && value <= 'Z')
        return value - 'A' + 'a';

    return value;
}

static bool ascii_equal_nocase_n(
    const unsigned char *a,
    size_t a_size,
    const char *b
)
{
    if (!a || !b)
        return false;

    size_t b_size = strlen(b);

    if (a_size != b_size)
        return false;

    for (size_t i = 0; i < a_size; ++i)
    {
        if (ascii_lower(a[i]) != ascii_lower((unsigned char)b[i]))
            return false;
    }

    return true;
}

static bool ascii_contains_nocase(
    const unsigned char *data,
    size_t data_size,
    const char *needle
)
{
    if (!data || !needle)
        return false;

    const size_t needle_size = strlen(needle);

    if (needle_size == 0 || data_size < needle_size)
        return false;

    for (size_t i = 0; i + needle_size <= data_size; ++i)
    {
        bool same = true;

        for (size_t j = 0; j < needle_size; ++j)
        {
            if (
                ascii_lower(data[i + j]) !=
                ascii_lower((unsigned char)needle[j])
            )
            {
                same = false;
                break;
            }
        }

        if (same)
            return true;
    }

    return false;
}

typedef struct
{
    bool headers_ready;
    size_t body_offset;
    bool has_content_length;
    size_t content_length;
    bool chunked;
} HttpHeaderInfo;

static bool parse_header_info(
    const unsigned char *data,
    size_t data_size,
    HttpHeaderInfo *out_info
)
{
    if (!data || !out_info)
        return false;

    memset(out_info, 0, sizeof(*out_info));

    const unsigned char *header_end =
        find_bytes(data, data_size, "\r\n\r\n", 4);

    if (!header_end)
        return true;

    out_info->headers_ready = true;
    out_info->body_offset = (size_t)(header_end - data) + 4;

    const unsigned char *line =
        find_bytes(data, data_size, "\r\n", 2);

    if (!line || line >= header_end)
        return false;

    const unsigned char *cursor = line + 2;

    while (cursor < header_end)
    {
        const size_t remaining = (size_t)(header_end - cursor);
        const unsigned char *line_end =
            find_bytes(cursor, remaining, "\r\n", 2);

        if (!line_end)
            line_end = header_end;

        const unsigned char *colon = NULL;

        for (const unsigned char *p = cursor; p < line_end; ++p)
        {
            if (*p == ':')
            {
                colon = p;
                break;
            }
        }

        if (colon)
        {
            const size_t name_size = (size_t)(colon - cursor);
            const unsigned char *value = colon + 1;

            while (value < line_end && (*value == ' ' || *value == '\t'))
                ++value;

            const size_t value_size = (size_t)(line_end - value);

            if (ascii_equal_nocase_n(cursor, name_size, "Content-Length"))
            {
                char number[64];
                size_t copy_size = value_size;

                if (copy_size >= sizeof(number))
                    copy_size = sizeof(number) - 1;

                memcpy(number, value, copy_size);
                number[copy_size] = '\0';

                char *end = NULL;
                unsigned long long parsed = strtoull(number, &end, 10);

                if (end != number && parsed <= (unsigned long long)SIZE_MAX)
                {
                    out_info->has_content_length = true;
                    out_info->content_length = (size_t)parsed;
                }
            }
            else if (
                ascii_equal_nocase_n(cursor, name_size, "Transfer-Encoding") &&
                ascii_contains_nocase(value, value_size, "chunked")
            )
            {
                out_info->chunked = true;
            }
        }

        if (line_end >= header_end)
            break;

        cursor = line_end + 2;
    }

    return true;
}

static bool chunked_message_complete(
    const unsigned char *data,
    size_t data_size
)
{
    size_t offset = 0;

    while (offset < data_size)
    {
        const unsigned char *line_end =
            find_bytes(data + offset, data_size - offset, "\r\n", 2);

        if (!line_end)
            return false;

        size_t line_size = (size_t)(line_end - (data + offset));

        if (line_size == 0 || line_size >= 64)
            return false;

        char size_text[64];
        memcpy(size_text, data + offset, line_size);
        size_text[line_size] = '\0';

        char *end = NULL;
        unsigned long long chunk_size = strtoull(size_text, &end, 16);

        if (end == size_text)
            return false;

        offset = (size_t)(line_end - data) + 2;

        if (chunk_size == 0)
        {
            return data_size >= offset + 2;
        }

        if (chunk_size > (unsigned long long)SIZE_MAX)
            return false;

        size_t chunk_bytes = (size_t)chunk_size;

        if (chunk_bytes > data_size - offset)
            return false;

        offset += chunk_bytes;

        if (
            offset + 2 > data_size ||
            data[offset] != '\r' ||
            data[offset + 1] != '\n'
        )
        {
            return false;
        }

        offset += 2;
    }

    return false;
}

static bool raw_response_complete(
    const unsigned char *data,
    size_t data_size
)
{
    HttpHeaderInfo info;

    if (!parse_header_info(data, data_size, &info))
        return false;

    if (!info.headers_ready)
        return false;

    if (info.body_offset > data_size)
        return false;

    const size_t body_size = data_size - info.body_offset;

    if (info.chunked)
    {
        return chunked_message_complete(
            data + info.body_offset,
            body_size
        );
    }

    if (info.has_content_length)
        return body_size >= info.content_length;

    return false;
}

static bool append_bytes(
    unsigned char **data,
    size_t *size,
    size_t *capacity,
    const unsigned char *source,
    size_t source_size
)
{
    if (!data || !size || !capacity || (!source && source_size > 0))
        return false;

    if (source_size > LOCAL_HTTP_MAX_RESPONSE - *size)
        return false;

    const size_t required = *size + source_size;

    if (required + 1 > *capacity)
    {
        size_t next_capacity = *capacity ? *capacity : LOCAL_HTTP_READ_CHUNK;

        while (next_capacity < required + 1)
        {
            if (next_capacity > LOCAL_HTTP_MAX_RESPONSE / 2)
            {
                next_capacity = LOCAL_HTTP_MAX_RESPONSE + 1;
                break;
            }

            next_capacity *= 2;
        }

        if (next_capacity > LOCAL_HTTP_MAX_RESPONSE + 1)
            return false;

        unsigned char *next = realloc(*data, next_capacity);

        if (!next)
            return false;

        *data = next;
        *capacity = next_capacity;
    }

    if (source_size > 0)
        memcpy(*data + *size, source, source_size);

    *size = required;
    (*data)[*size] = '\0';

    return true;
}

static bool decode_chunked_body(
    const unsigned char *data,
    size_t data_size,
    unsigned char **out_body,
    size_t *out_size
)
{
    if (!out_body || !out_size)
        return false;

    *out_body = NULL;
    *out_size = 0;

    unsigned char *body = NULL;
    size_t body_size = 0;
    size_t body_capacity = 0;
    size_t offset = 0;

    while (offset < data_size)
    {
        const unsigned char *line_end =
            find_bytes(data + offset, data_size - offset, "\r\n", 2);

        if (!line_end)
        {
            free(body);
            return false;
        }

        size_t line_size = (size_t)(line_end - (data + offset));

        if (line_size == 0 || line_size >= 64)
        {
            free(body);
            return false;
        }

        char size_text[64];
        memcpy(size_text, data + offset, line_size);
        size_text[line_size] = '\0';

        char *end = NULL;
        unsigned long long chunk_size = strtoull(size_text, &end, 16);

        if (end == size_text || chunk_size > (unsigned long long)SIZE_MAX)
        {
            free(body);
            return false;
        }

        offset = (size_t)(line_end - data) + 2;

        if (chunk_size == 0)
            break;

        size_t chunk_bytes = (size_t)chunk_size;

        if (
            chunk_bytes > data_size - offset ||
            offset + chunk_bytes + 2 > data_size
        )
        {
            free(body);
            return false;
        }

        if (
            !append_bytes(
                &body,
                &body_size,
                &body_capacity,
                data + offset,
                chunk_bytes
            )
        )
        {
            free(body);
            return false;
        }

        offset += chunk_bytes;

        if (data[offset] != '\r' || data[offset + 1] != '\n')
        {
            free(body);
            return false;
        }

        offset += 2;
    }

    if (!body)
    {
        body = malloc(1);

        if (!body)
            return false;

        body[0] = '\0';
    }

    *out_body = body;
    *out_size = body_size;
    return true;
}

static bool parse_http_response(
    const unsigned char *raw,
    size_t raw_size,
    LocalHttpResponse *out_response,
    char *error,
    size_t error_size
)
{
    if (!raw || !out_response)
        return false;

    HttpHeaderInfo info;

    if (!parse_header_info(raw, raw_size, &info) || !info.headers_ready)
    {
        set_error(error, error_size, "Tosu returned an invalid HTTP response");
        return false;
    }

    const unsigned char *status_end =
        find_bytes(raw, raw_size, "\r\n", 2);

    if (!status_end)
    {
        set_error(error, error_size, "Tosu returned an invalid HTTP status line");
        return false;
    }

    char status_line[128];
    size_t status_size = (size_t)(status_end - raw);

    if (status_size >= sizeof(status_line))
        status_size = sizeof(status_line) - 1;

    memcpy(status_line, raw, status_size);
    status_line[status_size] = '\0';

    int status_code = 0;

    if (sscanf(status_line, "HTTP/%*u.%*u %d", &status_code) != 1)
    {
        set_error(error, error_size, "Could not parse Tosu HTTP status");
        return false;
    }

    if (info.body_offset > raw_size)
    {
        set_error(error, error_size, "Tosu HTTP body offset is invalid");
        return false;
    }

    const unsigned char *body_start = raw + info.body_offset;
    const size_t available_body = raw_size - info.body_offset;

    unsigned char *body = NULL;
    size_t body_size = 0;

    if (info.chunked)
    {
        if (!decode_chunked_body(body_start, available_body, &body, &body_size))
        {
            set_error(error, error_size, "Could not decode Tosu chunked response");
            return false;
        }
    }
    else
    {
        body_size =
            info.has_content_length
                ? info.content_length
                : available_body;

        if (body_size > available_body)
        {
            set_error(error, error_size, "Tosu response ended before Content-Length bytes arrived");
            return false;
        }

        body = malloc(body_size + 1);

        if (!body)
        {
            set_error(error, error_size, "Out of memory storing Tosu response");
            return false;
        }

        if (body_size > 0)
            memcpy(body, body_start, body_size);

        body[body_size] = '\0';
    }

    out_response->status_code = status_code;
    out_response->body = body;
    out_response->body_size = body_size;

    return true;
}

bool LocalHttpInit(void)
{
    if (g_http_init_count > 0)
    {
        ++g_http_init_count;
        return true;
    }

#ifdef _WIN32
    WSADATA data;

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        return false;
#endif

    g_http_init_count = 1;
    return true;
}

void LocalHttpShutdown(void)
{
    if (g_http_init_count <= 0)
        return;

    --g_http_init_count;

    if (g_http_init_count == 0)
    {
#ifdef _WIN32
        WSACleanup();
#endif
    }
}

void LocalHttpResponseFree(
    LocalHttpResponse *response
)
{
    if (!response)
        return;

    free(response->body);
    response->body = NULL;
    response->body_size = 0;
    response->status_code = 0;
}

bool LocalHttpGet(
    const char *path,
    int connect_timeout_ms,
    int io_timeout_ms,
    LocalHttpResponse *out_response,
    char *error,
    size_t error_size
)
{
    if (!out_response)
    {
        set_error(error, error_size, "Local HTTP response pointer is null");
        return false;
    }

    LocalHttpResponseFree(out_response);

    if (g_http_init_count <= 0)
    {
        set_error(error, error_size, "Local HTTP runtime is not initialized");
        return false;
    }

    if (!path || path[0] != '/')
    {
        set_error(error, error_size, "Local HTTP path must begin with '/'");
        return false;
    }

    LocalSocket socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (socket_handle == LOCAL_INVALID_SOCKET)
    {
        set_socket_error(error, error_size, "Could not create Tosu socket");
        return false;
    }

    if (
        !socket_connect_loopback(
            socket_handle,
            connect_timeout_ms,
            error,
            error_size
        )
    )
    {
        close_socket(socket_handle);
        return false;
    }

    if (!socket_set_io_timeout(socket_handle, io_timeout_ms))
    {
        set_socket_error(error, error_size, "Could not configure Tosu socket timeout");
        close_socket(socket_handle);
        return false;
    }

    char request[1024];
    int request_size = snprintf(
        request,
        sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: " LOCAL_HTTP_HOST ":%d\r\n"
        "User-Agent: " MANIADANOVERLAY_USER_AGENT "\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        path,
        LOCAL_HTTP_PORT
    );

    if (request_size <= 0 || (size_t)request_size >= sizeof(request))
    {
        set_error(error, error_size, "Local HTTP request path is too long");
        close_socket(socket_handle);
        return false;
    }

    if (
        !socket_send_all(
            socket_handle,
            (const unsigned char *)request,
            (size_t)request_size,
            error,
            error_size
        )
    )
    {
        close_socket(socket_handle);
        return false;
    }

    unsigned char *raw = NULL;
    size_t raw_size = 0;
    size_t raw_capacity = 0;
    bool receive_failed = false;

    for (;;)
    {
        unsigned char chunk[LOCAL_HTTP_READ_CHUNK];

        int received = recv(
            socket_handle,
            (char *)chunk,
            (int)sizeof(chunk),
            0
        );

        if (received > 0)
        {
            if (
                !append_bytes(
                    &raw,
                    &raw_size,
                    &raw_capacity,
                    chunk,
                    (size_t)received
                )
            )
            {
                set_error(error, error_size, "Tosu response exceeded local HTTP memory limit");
                receive_failed = true;
                break;
            }

            if (raw_response_complete(raw, raw_size))
                break;

            continue;
        }

        if (received == 0)
            break;

        if (raw && raw_response_complete(raw, raw_size))
            break;

        set_socket_error(error, error_size, "Timed out or failed receiving Tosu response");
        receive_failed = true;
        break;
    }

    close_socket(socket_handle);

    if (receive_failed)
    {
        free(raw);
        return false;
    }

    if (!raw || raw_size == 0)
    {
        set_error(error, error_size, "Tosu returned no HTTP data");
        free(raw);
        return false;
    }

    bool parsed = parse_http_response(
        raw,
        raw_size,
        out_response,
        error,
        error_size
    );

    free(raw);
    return parsed;
}
