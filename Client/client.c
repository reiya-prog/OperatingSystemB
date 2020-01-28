#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <sys/wait.h>
#include <arpa/inet.h> /* for sockaddr_in and inet_addr() */
#include <stdlib.h>    /* for atoi() and exit() */
#include <string.h>    /* for memset() */
#include <unistd.h>    /* for close() */
#include <fcntl.h>
#include <sys/stat.h>

#define MAXBUFSIZE 200
#define MAXFILESIZE 8240
#define MAXINPUTSIZE 206
#define MAXNAMESIZE 200

void DieWithError(char *errorMessage);

void myfgets(char *string, int len)
{
    fgets(string, len, stdin);
    char *ln = strchr(string, '\n'); // 改行の位置を検索
    if (ln != NULL)                  // 改行を終端文字に置き換える
    {
        *ln = '\0';
    }
    else // 念のため標準入力を空にしておく
    {
        while (1)
        {
            int c = getchar();
            if (c == '\n' || c == EOF)
                break;
        }
    }
}

/* クライアントの引数はポート番号と実行ユーザー名 */
int main(int argc, char *argv[])
{
    /* 作業環境をホームディレクトリまたはルートディレクトリに変更 */
    if (strcmp(argv[2], "root")) // ルートユーザーでない
    {
        char dir[] = "/home";
        char cdir[26];
        snprintf(cdir, 26, "%s/%s", dir, argv[2]);
        chdir(cdir);
    }
    else // ルートユーザーである
    {
        chdir("/");
    }

    /* サーバとの制御用コネクション確立 */
    int clntSock;
    int cmdSock;
    struct sockaddr_in servAddr;
    struct sockaddr_in clntAddr;
    unsigned short servPort = atoi(argv[1]);
    unsigned short clntPort;
    char *servIP = "172.168.0.1";
    char clntIP[20];
    char recvCmd[5];

    int recvStatus;
    char recvMessage[MAXBUFSIZE];
    int recvSize;

    if ((cmdSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(servIP);
    servAddr.sin_port = htons(servPort);

    if (connect(cmdSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        DieWithError("connect() failed");

    /* クライアントのIPアドレスを接続情報から取得 */
    if ((recvSize = recv(cmdSock, clntIP, 20, 0)) < 0)
        DieWithError("recv() failed");
    // printf("size:%d\n", recvSize);
    clntIP[recvSize] = '\0';
    /* IPアドレス取得確認コマンドの送信 */
    if (send(cmdSock, "IPOK", strlen("IPOK"), 0) != strlen("IPOK"))
        DieWithError("send() sent a different number of bytes than expected");

    /* サーバからの応答を確認 */
    if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
        DieWithError("recv() failed");
    if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
        DieWithError("recv() failed");
    recvMessage[recvSize] = '\0';
    printf("%d %s\nUser IP address : %s\n", recvStatus, recvMessage, clntIP);
    for (int i = recvSize - 1; i >= 0; --i)
        recvMessage[i] = '\0';

    /* ユーザー名の入力 */
    printf("ユーザー名を入力してください。\n> ");
    char userName[11];
    userName[0] = '\0';
    do
    {
        myfgets(userName, 21);
        if (strlen(userName) == 0)
        {
            printf("ユーザー名を入力してください。\n> ");
        }
    } while (strlen(userName) == 0);
    // printf("USER %s\n", userName);

    /* USERコマンドの送信 */
    if (send(cmdSock, "USER", strlen("USER"), 0) != strlen("USER"))
        DieWithError("send() sent a different number of bytes than expected");
    if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
        DieWithError("recv() failed");
    recvCmd[recvSize] = '\0';
    if (strcmp("USER", recvCmd))
        DieWithError("Unexpected error occured.");

    /* ユーザーネームの送信 */
    if (send(cmdSock, userName, strlen(userName), 0) != strlen(userName))
        DieWithError("send() sent a different number of bytes than expected");

    /* サーバからの応答を確認 */
    if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
        DieWithError("recv() failed");
    if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
        DieWithError("recv() failed");
    recvMessage[recvSize] = '\0';
    printf("%d %s\n", recvStatus, recvMessage);
    for (int i = recvSize - 1; i >= 0; --i)
        recvMessage[i] = '\0';

    /* パスワードの入力 */
    printf("パスワードを入力してください。\n> ");
    char passWord[21];
    passWord[0] = '\0';
    do
    {
        myfgets(passWord, 21);
        if (strlen(passWord) == 0)
        {
            printf("パスワードを入力してください。\n> ");
        }
    } while (strlen(passWord) == 0);
    char passMask[strlen(passWord)];
    for (int i = 0; i < strlen(passWord); ++i)
        passMask[i] = '*';
    passMask[strlen(passWord)] = '\0';
    // printf("PASS %s\n", passMask);

    /* PASSコマンドの送信 */
    if (send(cmdSock, "PASS", strlen("PASS"), 0) != strlen("PASS"))
        DieWithError("send() sent a different number of bytes than expected");
    if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
        DieWithError("recv() failed");
    recvCmd[recvSize] = '\0';
    if (strcmp("PASS", recvCmd))
        DieWithError("Unexpected error occured.");

    /* パスワードの送信 */
    if (send(cmdSock, passWord, strlen(passWord), 0) != strlen(passWord))
        DieWithError("send() sent a different number of bytes than expected");

    /* サーバからの応答を確認 */
    if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
        DieWithError("recv() failed");
    if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
        DieWithError("recv() failed");
    recvMessage[recvSize] = '\0';
    printf("%d %s\n", recvStatus, recvMessage);
    for (int i = recvSize - 1; i >= 0; --i)
        recvMessage[i] = '\0';

    /*  入力されたコマンドに応じて処理 */
    while (1)
    {
        int fileSock = -1;
        struct sockaddr_in clntFileAddr;
        int fileAddrLen;
        printf("> ");
        char input[MAXINPUTSIZE];
        char command[6];
        char fileName[MAXNAMESIZE];
        char filePath[MAXNAMESIZE];
        fileName[0] = '\0';
        myfgets(input, MAXINPUTSIZE - 1); // コマンド入力

        int point;
        for (int i = 0; i <= strlen(input); ++i)
        {
            if (input[i] == ' ' || input[i] == '\0')
            {
                point = i;
                break;
            }
            else
            {
                command[i] = input[i];
            }
        }
        command[point] = '\0';
        if (point < strlen(input))
        {
            for (int i = point + 1; i < strlen(input); ++i)
            {
                if (input[i] != '\0')
                    filePath[i - point - 1] = input[i];
            }
            filePath[strlen(input) - point - 1] = '\0';
            for (int i = strlen(filePath) - 1; i >= 0; --i)
            {
                if (filePath[i] == '/')
                {
                    for (int j = i + 1; j < strlen(input); ++j)
                        fileName[j - i - 1] = filePath[j];
                    fileName[strlen(input) - i - 1] = '\0';
                    break;
                }
                else if (i == 0)
                {
                    strcpy(fileName, filePath);
                }
            }
        }
        // printf("path:%sfin\nname:%sfin\n", filePath, fileName);

        /* コマンドに応じて処理 */
        /* exitコマンド：コネクションの終了 */
        if (!strcmp(command, "exit"))
        {
            char cmd[] = "QUIT";
            if (send(cmdSock, cmd, strlen(cmd), 0) != strlen(cmd))
                DieWithError("send() sent a different number of bytes than expected");
            if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
                DieWithError("recv() failed");
            recvCmd[recvSize] = '\0';
            if (strcmp(cmd, recvCmd))
                DieWithError("Unexpected error occured.");

            /* サーバからの応答を確認 */
            if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
                DieWithError("recv() failed");
            if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf("%d %s\n", recvStatus, recvMessage);
            for (int i = recvSize - 1; i >= 0; --i)
                recvMessage[i] = '\0';

            /* ソケットを閉じる */
            close(cmdSock);
            break;
        }
        /* fetchコマンド：入力されたファイル名のファイルをサーバからダウンロード */
        else if (!strcmp(command, "fetch"))
        {
            if (strlen(fileName) == 0)
            {
                printf("ファイル名を入力してください。\n");
                continue;
            }

            /* 該当のファイルが存在するか確認 */
            char cmd[] = "FILE";
            if (send(cmdSock, cmd, strlen(cmd), 0) != strlen(cmd))
                DieWithError("send() sent a different number of bytes than expected");
            if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
                DieWithError("recv() failed");
            recvCmd[recvSize] = '\0';
            if (strcmp(cmd, recvCmd))
                DieWithError("Unexpected error occured.");

            if (send(cmdSock, filePath, strlen(filePath), 0) != strlen(filePath))
                DieWithError("send() sent a different number of bytes than expected");

            /* サーバからの応答を確認 */
            if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
                DieWithError("recv() failed");
            if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf("%d %s\n", recvStatus, recvMessage);
            for (int i = recvSize - 1; i >= 0; --i)
                recvMessage[i] = '\0';

            if (recvStatus == 553)
                continue;

            /* まずファイル受信用のコネクションを確立 */
            strcpy(cmd, "PORT");
            if (send(cmdSock, cmd, strlen(cmd), 0) != strlen(cmd))
                DieWithError("send() sent a different number of bytes than expected");
            if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
                DieWithError("recv() failed");
            recvCmd[recvSize] = '\0';
            if (strcmp(cmd, recvCmd))
                DieWithError("Unexpected error occured.");

            /* データ転送用コネクションを受け付けるために着信接続用のソケットを作成 */
            if ((clntSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
                DieWithError("socket() failed");

            memset(&clntAddr, 0, sizeof(clntAddr));
            clntAddr.sin_family = AF_INET;
            clntAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            clntAddr.sin_port = htons(0); // PORT番号を動的に確保

            if (bind(clntSock, (struct sockaddr *)&clntAddr, sizeof(clntAddr)) < 0)
                DieWithError("bind() failed");

            if (listen(clntSock, 5) < 0)
                DieWithError("listen() failed");

            /* PORT番号の取得 */
            unsigned int clntLen = sizeof(clntAddr);
            if (getsockname(clntSock, (struct sockaddr *)&clntAddr, (socklen_t *)&clntLen) < 0)
                DieWithError("getsockname() failed");

            /* PORTコマンドで送信するデータを整形(IPアドレス+PORT番号を指定の書式に) */
            char sendPortMessage[31];
            /* IPアドレスを整形 */
            char sendClntIP[4][5];
            int num = 0, cnt = 0;
            for (int i = 0; i < strlen(clntIP); ++i)
            {
                if (clntIP[i] != '.')
                {
                    sendClntIP[num][cnt] = clntIP[i];
                    ++cnt;
                }
                else
                {
                    sendClntIP[num][cnt] = '\0';
                    cnt = 0;
                    ++num;
                }
            }
            sendClntIP[3][cnt] = '\0';

            /* PORT番号を整形 */
            unsigned short port = ntohs(clntAddr.sin_port);
            unsigned short upperPort = port / 256;
            unsigned short lowerPort = port - upperPort * 256;

            /* 送信するデータを作成 */
            snprintf(sendPortMessage, 30, "%s,%s,%s,%s,%d,%d", sendClntIP[0], sendClntIP[1], sendClntIP[2], sendClntIP[3], upperPort, lowerPort);
            // printf("PORT %s\n", sendPortMessage);

            printf("PORT %s\n", sendPortMessage);
            /* 作成したデータを送信 */
            if (send(cmdSock, sendPortMessage, strlen(sendPortMessage), 0) != strlen(sendPortMessage))
                DieWithError("send() sent a different number of bytes than expected");

            fileAddrLen = sizeof(clntFileAddr);
            if ((fileSock = accept(clntSock, (struct sockaddr *)&clntFileAddr, &fileAddrLen)) < 0)
                DieWithError("accept() failed");

            /* サーバからの応答を確認 */
            if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
                DieWithError("recv() failed");
            if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf("%d %s\n", recvStatus, recvMessage);
            for (int i = recvSize - 1; i >= 0; --i)
                recvMessage[i] = '\0';

            /* コマンドをサーバに送信 */
            strcpy(cmd, "RETR");
            if (send(cmdSock, cmd, strlen(cmd), 0) != strlen(cmd))
                DieWithError("send() sent a different number of bytes than expected");
            if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
                DieWithError("recv() failed");
            recvCmd[recvSize] = '\0';
            if (strcmp(cmd, recvCmd))
                DieWithError("Unexpected error occured.");

            if (send(cmdSock, filePath, strlen(filePath), 0) != strlen(filePath))
                DieWithError("send() sent a different number of bytes than expected");

            int writeFD;
            ssize_t cc;
            if ((writeFD = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0777)) == -1)
                DieWithError("open() failed");

            /* ネットワークからデータを読み出してファイルに書き込み */
            while (1)
            {
                unsigned char *buffer = (unsigned char *)malloc(sizeof(unsigned char) * MAXFILESIZE);
                int recvStatus = 350;
                ssize_t writeSize;
                /* サーバからファイル送信状況を取得 */
                if ((recvSize = recv(cmdSock, (char *)&writeSize, sizeof(int), 0)) < 0)
                    DieWithError("recv() failed");

                /* ファイル受信完了 */
                if ((int)writeSize < 0)
                {
                    if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                        DieWithError("send() failed");
                    break;
                }

                ssize_t readSize;
                /* ファイルデータ受信 */
                if ((readSize = read(fileSock, buffer, (int)writeSize)) < 0)
                    DieWithError("recv() failed");
                if (write(writeFD, buffer, (int)writeSize) == -1)
                    DieWithError("write() failed");
                printf("    file writing...  write size : %d\n", (int)writeSize);
                if ((lseek(writeFD, 0, SEEK_END)) < 0)
                    DieWithError("lseek() failed");
                if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                    DieWithError("send() failed");
                /* 確保したバッファのメモリを解放 */
                free(buffer);
            }

            /* サーバからの応答を確認 */
            if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
                DieWithError("recv() failed");
            if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf("%d %s\n", recvStatus, recvMessage);
            for (int i = recvSize - 1; i >= 0; --i)
                recvMessage[i] = '\0';

            /* ファイルディスクリプタを閉じる */
            close(writeFD);
            /* ファイル受信用のコネクションをクローズ */
            close(fileSock);
        }
        /* storeコマンド：入力されたファイル名のファイルをサーバにアップロード */
        else if (!strcmp(command, "store"))
        {
            if (strlen(filePath) == 0)
            {
                printf("ファイル名を入力してください。\n");
                continue;
            }

            /* ファイルが存在するかどうかを確認 */
            struct stat st;
            int ret;
            if (stat(filePath, &st) != 0) // ファイルが見つからなかった場合
            {
                printf("File is not found.\n");
                continue;
            }

            /* まずファイル受信用のコネクションを確立 */
            char cmd[] = "PORT";
            if (send(cmdSock, cmd, strlen(cmd), 0) != strlen(cmd))
                DieWithError("send() sent a different number of bytes than expected");
            if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
                DieWithError("recv() failed");
            recvCmd[recvSize] = '\0';
            if (strcmp(cmd, recvCmd))
                DieWithError("Unexpected error occured.");

            /* データ転送用コネクションを受け付けるために着信接続用のソケットを作成 */
            if ((clntSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
                DieWithError("socket() failed");

            memset(&clntAddr, 0, sizeof(clntAddr));
            clntAddr.sin_family = AF_INET;
            clntAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            clntAddr.sin_port = htons(0); // PORT番号を動的に確保

            if (bind(clntSock, (struct sockaddr *)&clntAddr, sizeof(clntAddr)) < 0)
                DieWithError("bind() failed");

            if (listen(clntSock, 5) < 0)
                DieWithError("listen() failed");

            /* PORT番号の取得 */
            unsigned int clntLen = sizeof(clntAddr);
            if (getsockname(clntSock, (struct sockaddr *)&clntAddr, (socklen_t *)&clntLen) < 0)
                DieWithError("getsockname() failed");

            /* PORTコマンドで送信するデータを整形(IPアドレス+PORT番号を指定の書式に) */
            char sendPortMessage[31];
            /* IPアドレスを整形 */
            char sendClntIP[4][5];
            int num = 0, cnt = 0;
            for (int i = 0; i < strlen(clntIP); ++i)
            {
                if (clntIP[i] != '.')
                {
                    sendClntIP[num][cnt] = clntIP[i];
                    ++cnt;
                }
                else
                {
                    sendClntIP[num][cnt] = '\0';
                    cnt = 0;
                    ++num;
                }
            }
            sendClntIP[3][cnt] = '\0';

            /* PORT番号を整形 */
            unsigned short port = ntohs(clntAddr.sin_port);
            unsigned short upperPort = port / 256;
            unsigned short lowerPort = port - upperPort * 256;

            /* 送信するデータを作成 */
            snprintf(sendPortMessage, 30, "%s,%s,%s,%s,%d,%d", sendClntIP[0], sendClntIP[1], sendClntIP[2], sendClntIP[3], upperPort, lowerPort);
            // printf("PORT %s\n", sendPortMessage);
            printf("PORT %s\n", sendPortMessage);

            /* 作成したデータを送信 */
            if (send(cmdSock, sendPortMessage, strlen(sendPortMessage), 0) != strlen(sendPortMessage))
                DieWithError("send() sent a different number of bytes than expected");

            fileAddrLen = sizeof(clntFileAddr);
            if ((fileSock = accept(clntSock, (struct sockaddr *)&clntFileAddr, &fileAddrLen)) < 0)
                DieWithError("accept() failed");

            /* サーバからの応答を確認 */
            if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
                DieWithError("recv() failed");
            if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf("%d %s\n", recvStatus, recvMessage);
            for (int i = recvSize - 1; i >= 0; --i)
                recvMessage[i] = '\0';

            strcpy(cmd, "STOR");
            if (send(cmdSock, cmd, strlen(cmd), 0) != strlen(cmd))
                DieWithError("send() sent a different number of bytes than expected");
            if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
                DieWithError("recv() failed");
            recvCmd[recvSize] = '\0';
            if (strcmp(cmd, recvCmd))
                DieWithError("Unexpected error occured.");

            if (send(cmdSock, fileName, strlen(fileName), 0) != strlen(fileName))
                DieWithError("send() sent a different number of bytes than expected");

            int openFD;
            unsigned char *buffer = (unsigned char *)malloc(sizeof(unsigned char) * MAXFILESIZE);
            ssize_t readSize;

            if ((openFD = open(filePath, O_RDONLY)) == -1)
                DieWithError("open() failed");

            while ((readSize = read(openFD, buffer, MAXFILESIZE - 1)) > 0)
            {
                if (send(cmdSock, (char *)&readSize, sizeof(int), 0) != sizeof(int))
                    DieWithError("send() failed");
                if (write(fileSock, buffer, (int)readSize) < 0)
                    DieWithError("send() failed");

                if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
                    DieWithError("recv() failed");
                if (recvStatus != 350)
                    DieWithError("Unexpected error occured.");
            }

            /* サーバに終了通知を送信 */
            recvStatus = -1;
            if (send(cmdSock, (char *)&recvStatus, sizeof(int), 0) != sizeof(int))
                DieWithError("send() failed");

            /* サーバからの応答を確認 */
            if ((recvSize = recv(cmdSock, (char *)&recvStatus, sizeof(int), 0)) < 0)
                DieWithError("recv() failed");
            if ((recvSize = recv(cmdSock, recvMessage, MAXBUFSIZE, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf("%d %s\n", recvStatus, recvMessage);
            for (int i = recvSize - 1; i >= 0; --i)
                recvMessage[i] = '\0';

            /* 確保したバッファのメモリを解放 */
            free(buffer);
            /* ファイルディスクリプタをクローズ */
            close(openFD);
            /* ファイル送信用のコネクションをクローズ */
            close(fileSock);
        }
        /* lsコマンド：サーバからファイル一覧を取得 */
        else if (!strcmp(command, "ls"))
        {
            printf("ls command\n");
            /* 一覧を要求 */
            char cmd[] = "LIST";
            if (send(cmdSock, cmd, strlen(cmd), 0) != strlen(cmd))
                DieWithError("send() sent a different number of bytes than expected");
            if ((recvSize = recv(cmdSock, recvCmd, 4, 0)) < 0)
                DieWithError("recv() failed");
            recvCmd[recvSize] = '\0';
            if (strcmp(cmd, recvCmd))
                DieWithError("Unexpected error occured.");

            if ((recvSize = recv(cmdSock, recvMessage, 100000, 0)) < 0)
                DieWithError("recv() failed");
            recvMessage[recvSize] = '\0';
            printf("%s", recvMessage);
        }
        else if (!strcmp(command, "myls"))
        {
            int pipeFD[2];
            pipe(pipeFD);
            pid_t pid = fork();
            if (pid == 0) // 子プロセス
            {
                execlp("ls", "ls", "-l", NULL);
            }
            else // 親プロセス
            {
                wait(&pid);
            }
        }
        else if (!strcmp(command, "cd"))
        {
            if (chdir(filePath) != 0)
                printf("%s : そのようなファイルやディレクトリはありません。", filePath);
        }
        /* manコマンド：コマンドの一覧を表示 */
        else if (!strcmp(command, "man"))
        {
            printf("fetch <ファイル名> 　: 入力されたファイル名のファイルをサーバからダウンロード\n"
                   "store <ファイル名>　 : 入力されたファイル名のファイルをサーバへアップロード\n"
                   "cd <ディレクトリ名>  : 自分のカレントディレクトリを変更\n"
                   "ls   : ファイルの一覧情報をサーバから受信\n"
                   "myls : 自分のファイルの一覧情報を表示\n"
                   "man  : コマンド一覧を表示\n"
                   "exit : ログアウト(プログラムの終了)\n");
        }
        else
        {
            printf("正しいコマンドを入力してください。\n"
                   "コマンドの一覧は\"man\"で見られます。\n");
        }
    }
}