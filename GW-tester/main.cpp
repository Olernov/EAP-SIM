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
      printf("Error inititialing Winsock: %ld. Initialization failed.", SOCK_ERR);
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
        printf("Failed connecting to host %s. Error code=%ld. Initialization failed.", ipAddress, SOCK_ERR);
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
        printf("Error setting socket in non-blocking mode: %ld. Initialization failed.", SOCK_ERR);
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
    int i = 0;
    for (; i < size; i++) {
        if (3 * (i + 1) >= buffer_size - 1)
            break;
        sprintf(&buffer[3 * i], "%02X ", data[i]);
    }
    buffer[3 * (i + 1)] = '\0';
    return std::string(buffer);

}

int main(int argc, char *argv[])
{
    char buffer[2048];
    int  n, nBytesReceived, res;

    CPSPacket psPacket;
    //SPSRequest
    unsigned int uiReqNum=1;

    char szNumberOfTriplets[3];


    CPSPacket spPacket;
    __uint32_t ui32ReqNum = 1;
    __uint16_t ui16ReqType;
    __uint16_t ui16PackLen;
    char szTextParse[2048];
    //char szIMSI[20];
    //char* pszIMSI[6]={"250993254188441","250540000000011","250540000000010","250018302304209","250018302304208","250027415378891"  };

    char* imsi_file;
    FILE* fIMSI=NULL;
    char imsi[50];
    int port;
    SPSRequest *pspRequest;
    int nBytesProcessed;

    if(argc<2) {
        printf("Usage: gw-tester ip_address [port] [IMSI_file]");
        exit(1);
    }
    if(argc>2)
        port=atoi(argv[2]);
    else
        port=5100;

    if(argc>3)
        imsi_file=argv[3];
    else
        imsi_file="IMSI.txt";

    int sock = ConnectToSS7Gateway(argv[1], port);
    if (sock < 0) {
        exit(1);
    }

    std::cout << "Enter number of vectors from 1 to 5 for triplets or from 6 to 9 for quintuplets: ";
    while(true) {
        fd_set read_set;
        struct timeval tv;
        tv.tv_sec = 0;  // time-out
        tv.tv_usec = 200;
        FD_ZERO( &read_set );
        FD_SET( sock, &read_set );

        if ( (res = select( sock + 1, &read_set, NULL, NULL, &tv )) !=0  ) {
            nBytesReceived = recv(sock, buffer, 2048, 0);
            if(nBytesReceived <= 0) {
                printf("Error receiving data on socket: %ld\n" ,SOCK_ERR);
                exit(1);
            }

            printf("\n%d bytes received.\n",nBytesReceived);

            nBytesProcessed = 0;
            while(nBytesProcessed < nBytesReceived) {
               std::multimap<__uint16_t,SPSReqAttr*> mmRequest;
               pspRequest=(SPSRequest *)(buffer+nBytesProcessed);
               mmRequest.clear();
               spPacket.Parse(pspRequest, 2048, ui32ReqNum, ui16ReqType, ui16PackLen, mmRequest, 1);
               if (ui16ReqType == SS7GW_QUINTUPLET_RESP || ui16ReqType == SS7GW_QUINTUPLET_CONF) {
                   std::cout << "requestType: " << std::hex << ui16ReqType << ", requestNum: " << ui32ReqNum << std::endl;
                   for(auto it = mmRequest.begin(); it != mmRequest.end(); it++) {
                       size_t dataLen = ntohs(((SPSReqAttr*)(it->second))->m_usAttrLen) - sizeof(SPSReqAttr);
                       std::cout << "AttrID: " << std::hex << ntohs(((SPSReqAttr*)(it->second))->m_usAttrType)
                                 << ", len: " << std::dec << dataLen << std::endl;
                       unsigned char* data = (unsigned char*)(it->second) + sizeof(SPSReqAttr);
                       std::cout  << PrintBinaryDump(data, dataLen) << std::endl;
                   }
               }
               else {
                       spPacket.Parse(pspRequest, 2048, szTextParse, 2048);
                       printf("%s\n",szTextParse );
               }
               if(ui16PackLen >= 0)
                  nBytesProcessed += ui16PackLen;
               else
                  break;
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

                if(c >= '1' and c<='9') {
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
                    fIMSI=fopen(imsi_file,"r");
                    if(!fIMSI) {
                        printf("Unable to open imsi file %s\n",imsi_file);
                        continue;
                    }

                    //for(int i=0;i<6;i++) {
                    while(fgets(imsi,50,fIMSI)) {
                        if(imsi[strlen(imsi)-1] == '\n')
                            imsi[strlen(imsi)-1] = '\0';
                        printf("Requesting %d auth vectors for IMSI %s\n", vectorsNum, imsi);

                        if(psPacket.Init((SPSRequest*)buffer, 2048, uiReqNum++, requestType)) {
                            // error - buffer too small
                            printf("SPSRequest.Init failed, buffer too small" );
                            break;
                        }

                        if(!psPacket.AddAttr((SPSRequest*)buffer, 2048, SS7GW_IMSI,(const void *)imsi,strlen(imsi))) {
                            // error - buffer too small
                            printf("SPSRequest.AddAttr szIMSI failed, buffer too small" );
                            break;
                        }
                        unsigned long len;
                        if (requestType == SS7GW_QUINTUPLET_REQ) {
                            unsigned short vectorsNumN = htons(vectorsNum);
                            len = psPacket.AddAttr((SPSRequest*)buffer,2048, ATTR_REQUESTED_VECTORS, (const void *)&vectorsNumN,
                                                   sizeof(vectorsNumN)) ;
                        }
                        else {
                           sprintf(szNumberOfTriplets, "%02d", vectorsNum);
                           len = psPacket.AddAttr((SPSRequest*)buffer,2048, SS7GW_TRIP_NUM, (const void *)szNumberOfTriplets,
                                                  strlen(szNumberOfTriplets)) ;
                        }

                        n=send(sock,buffer,len,0);
                        if(n <= 0) {
                            printf("Error sending data on socket: %ld\n" ,SOCK_ERR);
                            break;
                        }
                    }
                    if (fIMSI) fclose(fIMSI);

                    printf("\nIMSI are sent, waiting for responses ...\n");
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
