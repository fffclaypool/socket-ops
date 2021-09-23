#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>

#define MSGSIZE 1024
#define BUFSIZE (MSGSIZE + 1)
#define DEFAULT_PORT 5320

enum
{
    CMD_NAME,
    DST_IP,
    DST_PORT
};

int main(int argc, char *argv[])
{
    struct sockaddr_in server;
    unsigned long dst_ip;
    int port;
    int s;
    int zero;

    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "Usage: %s hostname [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* サーバのIPアドレスを調べる */
    if ((dst_ip = inet_addr(argv[DST_IP])) == INADDR_NONE)
    {
        struct hostent *he;
        if ((he = gethostbyname(argv[DST_IP])) == NULL)
        {
            fprintf(stderr, "gethostbyname error\n");
            exit(EXIT_FAILURE);
        }
        memcpy((char *)&dst_ip, (char *)he->h_addr, he->h_length);
    }

    /* サーバのポート番号を調べる */
    if (argc == 3)
    {
        if ((port = atoi(argv[DST_PORT])) == 0)
        {
            struct servent *se;
            if ((se = getservbyname(argv[DST_PORT], "udp")) != NULL)
                port = (int)ntohs((u_int16_t)se->s_port);
            else
            {
                fprintf(stderr, "getservbyname error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    else
        port = DEFAULT_PORT;

    /* UDPでソケットを開く */
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* サーバのアドレスを設定する */
    memset(&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = dst_ip;
    server.sin_port = htons(port);

    zero = 0;
    printf("UDP>");
    fflush(stdout);

    while (1)
    {
        char buf[BUFSIZE];
        char cmd[BUFSIZE];
        int n;
        struct timeval tv;
        fd_set readfd;

        /* selectのタイムアウトの設定 */
        tv.tv_sec = 600;
        tv.tv_usec = 0;

        /* selectによるイベント待ち */
        FD_ZERO(&readfd);
        FD_SET(0, &readfd);
        FD_SET(s, &readfd);
        if ((select(s + 1, &readfd, NULL, NULL, &tv)) <= 0)
        {
            fprintf(stderr, "\nTimeout\n");
            break;
        }

        /* 標準入力からの入力コマンドの処理 */
        if (FD_ISSET(0, &readfd))
        {
            if ((n = read(0, buf, BUFSIZE - 1)) <= 0)
                break;
            buf[n] = '\0';
            sscanf(buf, "%s", cmd);
            if (strcmp(cmd, "quit") == 0)
                break;
            if (sendto(s, buf, n, 0, (struct sockaddr *)&server, sizeof server) < 0)
                break;
        }

        /* サーバからの応答メッセージの処理 */
        if (FD_ISSET(s, &readfd))
        {
            if ((n = recvfrom(s, buf, MSGSIZE, 0, (struct sockaddr *)0, (socklen_t*)&zero)) < 0)
                break;
            buf[n] = '\0';
            printf("%s", buf);
            if (n < MSGSIZE)
                printf("UDP>");
            fflush(stdout);
        }
    }
    close(s);

    return 0;
}
