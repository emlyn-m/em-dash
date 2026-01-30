#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctime>

int http_get(char* hostname, char* path, int port, char** out, time_t* pingp) {
        
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    int sockfd;
        
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    time_t time_start = time(NULL);
    char port_buf[6]; memset(port_buf, 0, 6); snprintf(port_buf, 6, "%d", port);
    if (getaddrinfo(hostname, port_buf, &hints, &res)) {
        fprintf(stderr, "error in getaddrinfo"); 
        return 1; 
    }
    
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(sockfd,res->ai_addr,res->ai_addrlen)) {
        fprintf(stderr, "error in connect");
        return 1;
    };
    
    const uint32_t header_size = 2048;
    char header[header_size];
    snprintf(header, header_size, "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n", path, hostname);
    
    send(sockfd, header, strlen(header), 0);
    
    const uint32_t read_buf_maxsize = 4096;
    char buf[read_buf_maxsize]; memset(buf, 0, read_buf_maxsize);

    if (!recv(sockfd, buf, read_buf_maxsize, 0)) {
        fprintf(stderr, "read 0 bytes :(\n");
    };
    time_t time_end = std::time(NULL);
    *pingp = (time_end - time_start);
    
    
    char* body_offset = strstr(buf, "\r\n\r\n") + 4*sizeof(char);
    int body_length = strlen(body_offset);
    if (!(*out = (char*) malloc(sizeof(char) * (body_length + 1)))) {
        fprintf(stderr, "failed to realloc output buffer to size %d\n", body_length + 1);
        return 1;
    }; 
    (*out)[body_length] = '\0';
    // safe to bypass warning - we realloc based on buffer size anyway
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstringop-truncation"
    strncpy(*out, body_offset, body_length);
    #pragma GCC diagnostic pop
        
    return 0;
}
