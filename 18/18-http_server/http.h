#ifndef __HTTP_H__
#define __HTTP_H__

#define HTTP_STR           "HTTP"
#define HTTPV0_STR         "HTTP/1.0"
#define HTTPV1_STR         "HTTP/1.1"
#define HTTP_GET           "GET"
#define HTTP_POST          "POST"
#define HTTP_CLOSE         "Close"
#define HTTP_KEEP_ALIVE    "Keep-Alive"
#define HOST_HDR           "\nHost:"
#define CONTENT_LENGTH_HDR "\nContent-Length:"
#define CONTENT_TYPE_HDR   "\nContent-Type:"
#define CACHE_CONTROL_HDR  "\nCache-Control:"
#define CONNECTION_HDR     "\nConnection:"
#define DATE_HDR           "\nDate:"
#define EXPIRES_HDR        "\nExpires:"
#define AGE_HDR            "\nAge:"
#define LAST_MODIFIED_HDR	"\nLast-Modified:"
#define IF_MODIFIED_SINCE_HDR	"\nIf-Modified_Since:"
#define PRAGMA_HDR              "\nPragma:"
#define RANGE_HDR               "\nRange:"
#define IF_RANGE_HDR            "\nIf-Range:"
#define ETAG_HDR                "\nETag:"

enum { GET = 1, POST = 2 };

int find_http_header(char *data, int len);

char* http_header_str_val(const char* buf, const char *key, const int key_len, char* value, int value_len);

char* http_get_url(char * data, int data_len, char* value, int value_len);

enum { HTTP_09, HTTP_10, HTTP_11 }; /* http version */

#endif
