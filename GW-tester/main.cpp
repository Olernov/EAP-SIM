#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <string>
#ifdef WIN32
    #include <conio.h>
    #include <Winsock2.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <errno.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/unistd.h>
    #include <sys/fcntl.h>
    #include <termios.h>
#endif
#include <map>

#include "ps_common.h"
#include "PSPacket.h"

const int PARSE_SUCCESS = 0;

enum Mode
{
    MANUAL_TESTS,
    AUTO_TESTS
};

#ifdef WIN32
    #define SOCK_ERR WSAGetLastError()
#else
    #define SOCK_ERR errno
#endif

#ifndef WIN32
int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}
#endif


int ConnectToSS7Gateway(const char* ipAddress, int port)
{
#ifdef WIN32
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData)) {
      printf("Error inititialing Winsock: %d. Initialization failed.", SOCK_ERR);
      return -1;
    }
#endif

    /* First call to socket() function */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
      printf("Unable to create socket AF_INET,SOCK_STREAM.\n");
      return -1;
    }
    /* Initialize socket structure */
    struct sockaddr_in serv_addr;
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ipAddress);
    serv_addr.sin_port = htons(port);

    if(connect(sock,(sockaddr*) &serv_addr, sizeof( serv_addr ))==-1) {
        printf("Failed connecting to host %s. Error code=%d. Initialization failed.", ipAddress, SOCK_ERR);
        shutdown( sock, 2 );
#ifdef WIN32
        closesocket( sock );
        WSACleanup();
#else
        close(sock);
#endif
        return -1;
    }
    printf("Connected to %s.\n-----------------------------------------------\n", ipAddress);

#ifdef WIN32
    u_long iMode=1;
    if(ioctlsocket(sock, FIONBIO, &iMode) != 0) {
        // Catch error
        printf("Error setting socket in non-blocking mode: %d. Initialization failed.", SOCK_ERR);
        return -1;
    }
#else
    fcntl(sock, F_SETFL, O_NONBLOCK);  // set to non-blocking
#endif
    return sock;
}


std::string PrintBinaryDump(const unsigned char* data, size_t size)
{
    const size_t buffer_size = 2048;
    char buffer[buffer_size];
    size_t i = 0;
    for (; i < size; i++) {
        if (3 * (i + 1) >= buffer_size - 1)
            break;
        sprintf(&buffer[3 * i], "%02X ", data[i]);
    }
    buffer[3 * (i + 1)] = '\0';
    return std::string(buffer);

}


int ParseNextResponseFromBuffer(unsigned char* buffer, int dataLen)
{
    __uint32_t requestNum;
    __uint16_t requestType;
    __uint16_t packetLen = 0;
    char szTextParse[2048];
    std::multimap<__uint16_t, SPSReqAttrParsed> mmRequest;
    const int VALIDATE_PACKET = 1;
    CPSPacket spPacket;
    SPSRequest *pspRequest = (SPSRequest *)buffer;
    mmRequest.clear();
    int parseRes = spPacket.Parse(pspRequest, dataLen, requestNum, requestType, packetLen, mmRequest, VALIDATE_PACKET);
    if (parseRes == PARSE_SUCCESS) {
        if (requestType == SS7GW_QUINTUPLET_RESP || requestType == SS7GW_QUINTUPLET_CONF) {
            std::cout << "requestType: " << std::hex << requestType << ", requestNum: " << requestNum << std::endl;
            for(auto it = mmRequest.begin(); it != mmRequest.end(); it++) {
                size_t dataLen = it->second.m_usDataLen;
                std::cout << "AttrID: " << std::hex << it->second.m_usAttrType
                          << ", len: " << std::dec << dataLen << std::endl;
                unsigned char* data = static_cast<unsigned char*>(it->second.m_pvData);
                std::cout  << PrintBinaryDump(data, dataLen) << std::endl;
            }
        }
        else {
            spPacket.Parse(pspRequest, 2048, szTextParse, 2048);
            printf("%s\n",szTextParse );
        }
    }
    else {
        std::cout << "Parsing response packet failed" << std::endl;
        return dataLen;
    }
    return packetLen;
}

void ProcessManualRequest(char c, const char* imsiFilename, unsigned char* buffer, int bufferSize,
                          unsigned short requestNum, int socket)
{
    unsigned long requestType;
    unsigned short vectorsNum;
    if (c >= '6') {
        requestType = SS7GW_QUINTUPLET_REQ;
        vectorsNum = c - '5';
    }
    else {
        requestType = SS7GW_IMSI_REQ;
        vectorsNum = c - '0';
    }
    FILE* imsiFile = fopen(imsiFilename, "r");
    if(!imsiFile) {
        printf("Unable to open imsi file %s\n",imsiFilename);
        return;
    }
    char imsi[50];
    while(fgets(imsi, 50, imsiFile)) {
        if(imsi[strlen(imsi)-1] == '\n')
            imsi[strlen(imsi)-1] = '\0';
        printf("Requesting %d auth vectors for IMSI %s\n", vectorsNum, imsi);
        CPSPacket psPacket;

        if(psPacket.Init((SPSRequest*)buffer, bufferSize, requestNum++, requestType)) {
            // error - buffer too small
            printf("SPSRequest.Init failed, buffer too small" );
            return;
        }

        if(!psPacket.AddAttr((SPSRequest*)buffer, 2048, SS7GW_IMSI,(const void *)imsi, strlen(imsi))) {
            // error - buffer too small
            printf("SPSRequest.AddAttr imsi failed, buffer too small" );
            return;
        }
        unsigned long len;
        if (requestType == SS7GW_QUINTUPLET_REQ) {
            unsigned short vectorsNumN = htons(vectorsNum);
            len = psPacket.AddAttr((SPSRequest*)buffer,2048, ATTR_REQUESTED_VECTORS, (const void *)&vectorsNumN,
                                   sizeof(vectorsNumN)) ;
        }
        else {
            char numberOfTriplets[3];
            sprintf(numberOfTriplets, "%02d", vectorsNum);
            len = psPacket.AddAttr((SPSRequest*)buffer,2048, SS7GW_TRIP_NUM, (const void *)numberOfTriplets,
                                  strlen(numberOfTriplets)) ;
        }

        if(send(socket, (char*)buffer, len, 0) <= 0) {
            printf("Error sending data on socket: %d\n" ,SOCK_ERR);
            return;
        }
    }
    if (imsiFile) {
        fclose(imsiFile);
    }

    std::cout << std::endl <<"IMSI are sent, waiting for responses ..." << std::endl;
}

int main(int argc, char *argv[])
{
    unsigned char buffer[2048];
    unsigned int requestNum = 1;
    char* imsi_file;
    int port;
    Mode mode = MANUAL_TESTS;

    if(argc<2) {
        printf("Usage: gw-tester ip_address [port] [IMSI_file]");
        exit(1);
    }
    if(argc>2)
        port=atoi(argv[2]);
    else
        port=5100;

    if(argc>3) {
        imsi_file = argv[3];
    }
    else {
        imsi_file = "IMSI.txt";
    }

    int sock = ConnectToSS7Gateway(argv[1], port);
    if (sock < 0) {
        exit(1);
    }

    std::cout << "Enter number of vectors: 1-5 for triplets, 6-9 for quintuplets or 'a' for auto tests: ";
    while(true) {
        fd_set read_set;
        struct timeval tv;
        tv.tv_sec = 0;  // time-out
        tv.tv_usec = 200;
        FD_ZERO( &read_set );
        FD_SET( sock, &read_set );

        if ( select( sock + 1, &read_set, NULL, NULL, &tv ) !=0  ) {
            int bytesReceived = recv(sock, (char*)buffer, 2048, 0);
            if(bytesReceived <= 0) {
                printf("Error receiving data on socket: %d\n" ,SOCK_ERR);
                exit(1);
            }

            printf("\n%d bytes received.\n",bytesReceived);

            int bytesProcessed = 0;
            while(bytesProcessed < bytesReceived) {
                bytesProcessed += ParseNextResponseFromBuffer(buffer + bytesProcessed, bytesReceived);
            }
            printf("\n----------------------------\n");
        }
        else {
            // select time-out
            char c;
            if(kbhit()) {
#ifdef WIN32
                c=getch();
#else
                c=getchar();
#endif
                if(c =='q') {
                    exit(0);
                }
                if (c == 'a') {
                    mode = AUTO_TESTS;

                }

                if(c >= '1' and c <= '9') {
                    mode = MANUAL_TESTS;
                    ProcessManualRequest(c, imsi_file, buffer, sizeof(buffer), requestNum, sock);
                }
            }
        }
    }
#ifdef WIN32
    closesocket( sock );
    WSACleanup();
#else
    close(sock);
#endif
    return 0;
}
