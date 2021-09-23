#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 8192
#define BUFSIZE2 32768
#define DEFAULT_PORT 5320

/* ルーティングテーブルを表示させるコマンド */
#define COMMAND_ROUTE "/bin/netstat -rn"

/* ARPテーブルを表示させるコマンド */
#define COMMAND_ARP "/sbin/arp -an"

/* TCPコネクションテーブルを表示させるコマンド */
#define COMMAND_TCP "/bin/netstat -tn"

/* NICの情報を表示させるコマンド */
#define COMMAND_NIC "/sbin/ifconfig -a"

enum
{
    CMD_NAME,
    SRC_PORT
};

int execute(char *command, char *buf, int bufmax);

int main(int argc, char *argv[])
{
    struct sockaddr_in server;
    struct sockaddr_in client;
    int len;
    int port;
    int s;
    int s0;

    /* 引数の処理 */
    if (argc == 2)
    {
        if ((port = atoi(argv[SRC_PORT])) == 0)
        {
            struct servent *se;
            if ((se = getservbyname(argv[SRC_PORT], "tcp")) != NULL)
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

    /* TCPでソケットをオープンする */
    if ((s0 = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* サーバのアドレスを設定する */
    memset(&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&server, sizeof server) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    /* コネクション確率要求受付開始 */
    if ((listen(s0, 5)) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    /* コネクション受け付けループ */
    while (1)
    {
        /* コネクション受付処理 */
        len = sizeof client;
        if ((s = accept(s0, (struct sockaddr *)&client, (socklen_t*)&len)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        /* 接続したクライアントのIPアドレスの表示 */
        printf("connected from '%s'\n", inet_ntoa(client.sin_addr));

        /* サーバ処理メインルーチン */
        while (1)
        {
            char recv_buf[BUFSIZE];
            char cmd1[BUFSIZE];
            char cmd2[BUFSIZE];
            int cn;
            int i;
            char send_head[BUFSIZE];
            char send_data[BUFSIZE2];
            int hn;
            int dn;

            /* コマンドの受信 */
            recv_buf[0] = '\0';
            for (i = 0; i < BUFSIZE - 1; i++)
            {
                if (recv(s, &recv_buf[i], 1, 0) <= 0)
                    goto exit_loop;
                if (recv_buf[i] == '\n')
                    break;
            }
            recv_buf[i] = '\0';
            printf("receive '%s'\n", recv_buf);

            /* 受信コマンドの処理 */
            if ((cn = sscanf(recv_buf, "%s%s", cmd1, cmd2)) <= 0)
                continue;
            if (cn == 2 && strcmp(cmd1, "show") == 0)
            {
                if (strcmp(cmd2, "route") == 0)
                    dn = execute(COMMAND_ROUTE, send_data, BUFSIZE2);
                else if (strcmp(cmd2, "arp") == 0)
                    dn = execute(COMMAND_ARP, send_data, BUFSIZE2);
                else if (strcmp(cmd2, "tcp") == 0)
                    dn = execute(COMMAND_TCP, send_data, BUFSIZE2);
                else if (strcmp(cmd2, "nic") == 0)
                    dn = execute(COMMAND_NIC, send_data, BUFSIZE2);
                else
                    dn = snprintf(send_data, BUFSIZE2, "parameter error '%s'\n"
                                                      "show [route|arp|tcp|nic]\n",
                                  cmd2);
            }
            else
            {
                if (strcmp(cmd1, "quit") == 0)
                    goto exit_loop;

                send_data[0] = '\0';
                if (cn != 1 && strcmp(cmd1, "help") != 0)
                    snprintf(send_data, BUFSIZE2 - 1, "command error '%s'\n", cmd1);
                strncat(send_data, "Command:\n"
                                   "  show route\n"
                                   "  show arp\n"
                                   "  show tcp\n"
                                   "  show nic\n"
                                   "  quit\n"
                                   "  help\n",
                        BUFSIZE2 - strlen(send_data) - 1);
                dn = strlen(send_data);
            }

            /* アプリケーションヘッダの送信 */
            hn = snprintf(send_head, BUFSIZE - 1, "Content-Length: %d\n\n", dn);
            if (send(s, send_head, hn, 0) < 0)
                break;
            send_head[hn] = '\0';
            printf("%s\n", send_head);

            /* アプリケーションデータの送信 */
            if (send(s, send_data, dn, 0))
                break;
            printf("%s\n", send_data);
        }
    exit_loop:
        printf("connection closed.\n");
        close(s);
    }
    close(s0);

    return 0;
}

int execute(char *command, char *buf, int bufmax)
{
    FILE *fp;
    int i;

    if ((fp = popen(command, "r")) == NULL)
    {
        perror(command);
        i = snprintf(buf, BUFSIZE, "server error: '%s' cannot execute.\n", command);
    }
    else
    {
        i = 0;
        while (i < bufmax - 1 && (buf[i] = fgetc(fp)) != EOF)
            i++;
        buf[i] = '\0';
        pclose(fp);
    }
    return i;
}
