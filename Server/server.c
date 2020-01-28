#include <stdio.h> /* for printf() and fprintf() */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <pthread.h>
#include <fcntl.h>

#define MAXPENDING 5 /* Maximum outstanding connection requests */
#define MAXBUFSIZE 10000
#define MAXFILESIZE 8240
#define MAXNAMESIZE 200

void DieWithError(char *errorMessage);
void *ThreadMain(void *arg);

struct ThreadArgs
{
    int cmdSock;
};

/* サーバの引数は特になし */
int main(int argc, char *argv[])
{
    /* 作業環境をホームディレクトリまたはルートディレクトリに変更 */
    if (strcmp(argv[2], "root")) // ルートユーザーでない
    {
        char dir[] = "/home";
        char cdir[26];
        snprintf(cdir, 26, "%s/%s/Server", dir, argv[2]);
        chdir(cdir);
    }
    else // ルートユーザーである
    {
        chdir("/");
    }

    int servSock;
    int clntCmdSock; // コマンド制御用
    struct sockaddr_in servAddr;
    struct sockaddr_in clntAddr;
    unsigned short servCmdPort = atoi(argv[1]);
    unsigned short servFilePort;
    unsigned int clntLen;
    pthread_t threadID;
    struct ThreadArgs *threadArgs;

    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(servCmdPort);

    if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        DieWithError("bind() failed");

    if (listen(servSock, MAXPENDING) < 0)
        DieWithError("listen() failed");

    while (1)
    {
        char clntIP[20];
        clntLen = sizeof(clntAddr);
        if ((clntCmdSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntLen)) < 0)
            DieWithError("accept() failed");
        strcpy(clntIP, inet_ntoa(clntAddr.sin_addr));
        printf("Handling client %s\n", clntIP);

        /* 接続してきたクライアントにIPアドレスを返す */
        if (send(clntCmdSock, clntIP, strlen(clntIP), 0) != strlen(clntIP))
            DieWithError("send() failed");
        char recvCmd[5];
        int recvSize;
        /* コマンドを受信 */
        if ((recvSize = recv(clntCmdSock, recvCmd, 4, 0)) < 0)
            DieWithError("recv() failed");
        recvCmd[4] = '\0';
        if(strcmp(recvCmd, "IPOK"))
            DieWithError("Unexpected error occured.");

        if ((threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL)
            DieWithError("malloc() failed");
        threadArgs->cmdSock = clntCmdSock;

        if (pthread_create(&threadID, NULL, ThreadMain, (void *)threadArgs) != 0)
            DieWithError("pthread_create() failed");
        printf("with thread %ld\n", (long int)threadID);
    }
}

void *ThreadMain(void *threadArgs)
{
    int cmdSock;
    int fileSock = -1;
    struct sockaddr_in clntFileAddr;

    int recvStatus;
    char recvCmd[5];
    char recvMessage[MAXBUFSIZE];
    char sendMessage[] = "FTP server ready.";

    char userName[11];
    int recvSize;
    pthread_detach(pthread_self());

    cmdSock = ((struct ThreadArgs *)threadArgs)->cmdSock;
    free(threadArgs);

    recvStatus = 220;

    /* 接続が確立したことをクライアントに通知 */
    if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
        DieWithError("send() failed");
    if (send(cmdSock, sendMessage, strlen(sendMessage), 0) != strlen(sendMessage))
        DieWithError("send() failed");

    while (1)
    {
        /* コマンドを受信 */
        if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
            DieWithError("recv() failed");
        recvCmd[recvSize] = '\0';
        printf("%s", recvCmd);
        if (send(cmdSock, recvCmd, strlen(recvCmd), 0) != strlen(recvCmd))
            DieWithError("send() failed");

        /* ユーザーネーム受信 */
        if (!strcmp(recvCmd, "USER"))
        {
            /* ユーザーネームを受け取る */
            if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf(" %s\n", recvMessage);
            strcpy(userName, recvMessage); // ユーザーネームの保存

            /* クライアントに応答を送信 */
            char recvUserCmd[41];
            for (int i = 0; i < 41; ++i)
                recvUserCmd[i] = '\0';
            recvStatus = 331;
            snprintf(recvUserCmd, 40, "Password required for %s.", recvMessage);
            if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                DieWithError("send() failed");
            if (send(cmdSock, recvUserCmd, strlen(recvUserCmd), 0) != strlen(recvUserCmd))
                DieWithError("send() failed");
            for (int i = recvSize - 1; i >= 0; --i)
                recvMessage[i] = '\0';
        }
        /* パスワード受信 */
        else if (!strcmp(recvCmd, "PASS"))
        {
            /* パスワードを受け取る */
            if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf(" %s\n", recvMessage);

            /* クライアントに応答を送信 */
            char recvPassCmd[41];
            for (int i = 0; i < 41; ++i)
                recvPassCmd[i] = '\0';
            recvStatus = 230;
            snprintf(recvPassCmd, 40, "User %s logged in.", userName);
            if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                DieWithError("send() failed");
            if (send(cmdSock, recvPassCmd, strlen(recvPassCmd), 0) != strlen(recvPassCmd))
                DieWithError("send() failed");
            for (int i = recvSize - 1; i >= 0; --i)
                recvMessage[i] = '\0';
        }
        /* クライアントとの接続を切断 */
        else if (!strcmp(recvCmd, "QUIT"))
        {
            /* クライアントに応答を送信 */
            char recvQuitCmd[41];
            for (int i = 0; i < 41; ++i)
                recvQuitCmd[i] = '\0';
            recvStatus = 221;
            snprintf(recvQuitCmd, 40, "Goodbye..");
            if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                DieWithError("send() failed");
            if (send(cmdSock, recvQuitCmd, strlen(recvQuitCmd), 0) != strlen(recvQuitCmd))
                DieWithError("send() failed");
            close(cmdSock);
            printf("\nQUIT command successful.\n");
            break;
        }
        /* ファイル送受信用のコネクションを確立 */
        else if (!strcmp(recvCmd, "PORT"))
        {
            /* IPアドレスとポート番号をクライアントから取得 */
            if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf(" %s\n", recvMessage);

            /* IPアドレスとポート番号の解析 */
            char clntIP[17], upperPort[4], lowerPort[4];
            for (int i = 0; i < 17; ++i)
                clntIP[i] = '\0';
            for (int i = 0; i < 4; ++i)
                upperPort[i] = '\0';
            for (int i = 0; i < 4; ++i)
                lowerPort[i] = '\0';
            int cnt = 0, num = 0;
            for (int i = 0; i < strlen(recvMessage); ++i)
            {
                if (cnt < 4)
                {
                    if (recvMessage[i] != ',')
                        clntIP[i] = recvMessage[i];
                    else
                    {
                        clntIP[i] = '.';
                        ++cnt;
                        if (cnt == 4)
                            clntIP[i] = '\0';
                    }
                }
                else if (cnt == 4)
                {
                    if (recvMessage[i] != ',')
                    {
                        upperPort[num] = recvMessage[i];
                        ++num;
                    }
                    else
                    {
                        upperPort[num] = '\0';
                        num = 0;
                        ++cnt;
                    }
                }
                else if (cnt == 5)
                {
                    lowerPort[num] = recvMessage[i];
                    ++num;
                }
            }
            lowerPort[num] = '\0';
            unsigned short clntPort = atoi(upperPort) * 256 + atoi(lowerPort);
            for (int i = recvSize - 1; i >= 0; --i)
                recvMessage[i] = '\0';

            /* データ転送用コネクションを確立する */
            if ((fileSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
                DieWithError("socket() failed");

            memset(&clntFileAddr, 0, sizeof(clntFileAddr));
            clntFileAddr.sin_family = AF_INET;
            clntFileAddr.sin_addr.s_addr = inet_addr(clntIP);
            clntFileAddr.sin_port = htons(clntPort);

            if (connect(fileSock, (struct sockaddr *)&clntFileAddr, sizeof(clntFileAddr)) < 0)
                DieWithError("connect() failed");

            /* クライアントに応答を送信 */
            char recvPortCmd[41];
            for (int i = 0; i < 41; ++i)
                recvPortCmd[i] = '\0';
            recvStatus = 200;
            snprintf(recvPortCmd, 40, "PORT command successful.");
            if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                DieWithError("send() failed");
            if (send(cmdSock, recvPortCmd, strlen(recvPortCmd), 0) != strlen(recvPortCmd))
                DieWithError("send() failed");
            printf("PORT command successful.\n");
        }
        /* サーバのファイルを送信 */
        else if (!strcmp(recvCmd, "RETR"))
        {
            int openFD;
            int clntStatus;
            char fileName[MAXNAMESIZE];
            for (int i = 0; i < MAXNAMESIZE; ++i)
                fileName[i] = '\0';
            unsigned char *buffer = (unsigned char *)malloc(sizeof(unsigned char) * MAXFILESIZE);
            char recvRetrCmd[41];
            for (int i = 0; i < 41; ++i)
                recvRetrCmd[i] = '\0';
            ssize_t readSize;

            if ((recvSize = recv(cmdSock, fileName, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            fileName[recvSize] = '\0';

            if ((openFD = open(fileName, O_RDONLY)) == -1)
                DieWithError("open() failed");
            while ((readSize = read(openFD, buffer, MAXFILESIZE - 1)) > 0)
            {
                if (send(cmdSock, (char *)&readSize, sizeof(int), 0) != sizeof(int))
                    DieWithError("send() failed");
                if (write(fileSock, buffer, (int)readSize) < 0)
                    DieWithError("send() failed");

                if ((recvSize = recv(cmdSock, (char *)&clntStatus, sizeof(int), 0)) < 0)
                    DieWithError("recv() failed");
                if (clntStatus != 350)
                    DieWithError("Unexpected error occured.");
            }

            /* クライアントに終了通知を送信 */
            recvStatus = -1;
            if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                DieWithError("send() failed");

            if ((recvSize = recv(cmdSock, (char *)&clntStatus, sizeof(int), 0)) < 0)
                DieWithError("recv() failed");
            if (clntStatus != 350)
                DieWithError("Unexpected error occured.");

            recvStatus = 226;
            snprintf(recvRetrCmd, 40, "Transfer complete.");

            /* クライアントに応答を送信 */
            if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                DieWithError("send() failed");
            if (send(cmdSock, recvRetrCmd, strlen(recvRetrCmd), 0) != strlen(recvRetrCmd))
                DieWithError("send() failed");

            printf(" %s\nTransfer complete\n", fileName);
            /* 確保したバッファのメモリを解放 */
            free(buffer);
            /* ファイルディスクリプタをクローズ */
            close(openFD);
            /* ファイル送信用のコネクションをクローズ */
            close(fileSock);
            printf("RETR command successful.\n");
        }
        /* サーバにファイルを保存 */
        else if (!strcmp(recvCmd, "STOR"))
        {
            int writeFD;
            ssize_t cc;

            char fileName[MAXNAMESIZE];
            for (int i = 0; i < MAXNAMESIZE; ++i)
                fileName[i] = '\0';

            unsigned char *buffer = (unsigned char *)malloc(sizeof(unsigned char) * MAXFILESIZE);
            char recvRetrCmd[41];
            for (int i = 0; i < 41; ++i)
                recvRetrCmd[i] = '\0';
            ssize_t readSize;

            if ((recvSize = recv(cmdSock, fileName, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            fileName[recvSize] = '\0';
            sleep(1);
            if ((writeFD = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0777)) == -1)
                DieWithError("open() failed");

            /* ネットワークからデータを読み出してファイルに書き込み */
            while (1)
            {
                unsigned char *buffer = (unsigned char *)malloc(sizeof(unsigned char) * MAXFILESIZE);
                int recvStatus = 350;
                ssize_t writeSize;
                /* クライアントからファイル送信状況を取得 */
                if ((recvSize = recv(cmdSock, (char *)&writeSize, sizeof(int), 0)) < 0)
                    DieWithError("recv() failed");

                /* ファイル受信完了 */
                if ((int)writeSize < 0)
                    break;

                ssize_t readSize;
                /* ファイルデータ受信 */
                if ((readSize = read(fileSock, buffer, (int)writeSize)) < 0)
                    DieWithError("recv() failed");
                if (write(writeFD, buffer, (int)writeSize) == -1)
                    DieWithError("write() failed");

                if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                    DieWithError("send() failed");
                /* 確保したバッファのメモリを解放 */
                free(buffer);
            }
            recvStatus = 226;
            snprintf(recvRetrCmd, 40, "Transfer complete.");

            /* クライアントに応答を送信 */
            if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                DieWithError("send() failed");
            if (send(cmdSock, recvRetrCmd, strlen(recvRetrCmd), 0) != strlen(recvRetrCmd))
                DieWithError("send() failed");

            printf(" %s\nTransfer complete\n", fileName);

            /* ファイルディスクリプタを閉じる */
            close(writeFD);
            /* ファイル受信用のコネクションをクローズ */
            close(fileSock);
            printf("STOR command successful.\n");
        }
        /* ファイルの一覧を返す */
        else if (!strcmp(recvCmd, "LIST"))
        {
            char *recvListCmd = (char *)malloc(sizeof(char) * 100000);
            for (int i = 0; i < 100000; ++i)
                recvListCmd[i] = '\0';

            int pipeFD[2];
            pipe(pipeFD);
            pid_t pid = fork();
            if (pid == 0) // 子プロセス
            {
                close(1);
                dup2(cmdSock, 1);
                execlp("ls", "ls", "-l", NULL);
            }
            else // 親プロセス
            {
                wait(&pid);
            }
            printf("\nLIST command successful.\n");
        }
        /* ファイルが存在するかどうかを確認 */
        else if (!strcmp(recvCmd, "FILE"))
        {
            struct stat st;
            int ret;
            char recvFileCmd[41];
            for (int i = 0; i < 41; ++i)
                recvFileCmd[i] = '\0';
            char fileName[MAXNAMESIZE];
            for (int i = 0; i < MAXNAMESIZE; ++i)
                fileName[i] = '\0';

            if ((recvSize = recv(cmdSock, fileName, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            fileName[recvSize] = '\0';
            printf(" %s\n", fileName);

            if (stat(fileName, &st) == 0) // ファイルが見つかった場合
            {
                recvStatus = 250;
                snprintf(recvFileCmd, 40, "FILE command successful.");
            }
            else // ファイルが見つからなかった場合
            {
                recvStatus = 553;
                snprintf(recvFileCmd, 40, "File is not found.");
            }

            for (int i = recvSize - 1; i >= 0; --i)
                fileName[i] = '\0';

            /* クライアントに応答を送信 */
            if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                DieWithError("send() failed");
            if (send(cmdSock, recvFileCmd, strlen(recvFileCmd), 0) != strlen(recvFileCmd))
                DieWithError("send() failed");
            printf("FILE command successful.\n");
        }
        for (int i = strlen(recvCmd) - 1; i >= 0; --i)
            recvCmd[i] = '\0';
    }
    printf("Thread Finish\n");
    return (NULL);
}
