#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

#include "../EAP-SIM-GW/CoACommon.h"
#include "../EAP-SIM-GW/PSPacket.h"

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

int main(int argc, char *argv[])
{
    int sock, newsockfd, portno, clilen;
    char buffer[2048];
    struct sockaddr_in serv_addr, cli_addr;
    int  n,nBytesReceived,res;
#ifdef WIN32
    WSADATA wsaData;
#endif
    CPSPacket psPacket;
    //SPSRequest
    unsigned int uiReqNum=1;
    unsigned int uiReqType=SS7GW_IMSI_REQ;
    char szNumberOfTriplets[3];
    int len=0;
    std::multimap<__uint16_t,SPSReqAttr*> mmRequest;
    CPSPacket spPacket;
    __uint32_t ui32ReqNum=1;
    __uint16_t ui16ReqType;
    __uint16_t ui16PackLen;
    char szTextParse[2048];
    //char szIMSI[20];
    //char* pszIMSI[6]={"250993254188441","250540000000011","250540000000010","250018302304209","250018302304208","250027415378891"  };
    fd_set read_set, error_set, write_set;
    struct timeval tv;
    char* imsi_file;
    FILE* fIMSI=NULL;
    char imsi[50];
    int port;
    SPSRequest *pspRequest;
    int nBytesProcessed;

#ifdef WIN32
    if(WSAStartup(MAKEWORD(2,2), &wsaData)) {
      printf("Error inititialing Winsock: %ld. Initialization failed.", SOCK_ERR);
      exit(1);
    }
#endif

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

    /* First call to socket() function */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
      printf("Unable to create socket AF_INET,SOCK_STREAM.\n");
      exit(1);
    }
    /* Initialize socket structure */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr( argv[1] );
    serv_addr.sin_port = htons(port);

    if(connect(sock,(sockaddr*) &serv_addr, sizeof( serv_addr ))==-1)
       {
            printf("Failed connecting to host %s. Error code=%ld. Initialization failed.", argv[1], SOCK_ERR);
            shutdown( sock, 2 );
#ifdef WIN32
            closesocket( sock );
            WSACleanup();
#else
            close(sock);
#endif
            exit(1);
        }
    printf("Connected to %s.\n-----------------------------------------------\n",argv[1]);

#ifdef WIN32
    u_long iMode=1;
    if(ioctlsocket(sock,FIONBIO,&iMode) != 0) {
        // Catch error
        printf("Error setting socket in non-blocking mode: %ld. Initialization failed.", SOCK_ERR);
        return -1;
    }
#else
    fcntl(sock, F_SETFL, O_NONBLOCK);  // set to non-blocking
#endif


    while(true) {
        tv.tv_sec = 0;  // time-out
        tv.tv_usec = 100;
        FD_ZERO( &read_set );
        FD_SET( sock, &read_set );

        if ( (res=select( sock + 1, &read_set, NULL, NULL, &tv )) !=0  ) {

            nBytesReceived = recv(sock, buffer, 2048, 0);
            if(nBytesReceived <= 0) {
                printf("Error receiving data on socket: %ld\n" ,SOCK_ERR);
                exit(1);
            }

            printf("\n%d bytes received.\n",nBytesReceived);

            nBytesProcessed = 0;
            while(nBytesProcessed<nBytesReceived) {
               pspRequest=(SPSRequest *)(buffer+nBytesProcessed);
               mmRequest.clear();
               spPacket.Parse(pspRequest, 2048, ui32ReqNum,ui16ReqType,ui16PackLen,mmRequest, 1);
               spPacket.Parse(pspRequest, 2048, szTextParse, 2048);
               printf("%s\n",szTextParse );

               if(ui16PackLen >= 0)
                  nBytesProcessed += ui16PackLen;
               else
                  break;
            }
            printf("\n----------------------------\n");

        }
        else {
            char c;
            if(kbhit()) {
#ifdef WIN32
                c=getch();
#else
                c=getchar();
#endif
                if(c =='q') {
                    if(fIMSI)
                        fclose(fIMSI);
                    exit(0);
                }
                if(c >= '1' and c<='5') {
                    fIMSI=fopen(imsi_file,"r");
                    if(!fIMSI) {
                        printf("Unable to open imsi file %s\n",imsi_file);
                        continue;
                    }

                    //for(int i=0;i<6;i++) {
                    while(fgets(imsi,50,fIMSI)) {
                        if(imsi[strlen(imsi)-1] == '\n')
                            imsi[strlen(imsi)-1] = '\0';
                        printf("Requesting %c triplets for IMSI %s\n",c,imsi);

                        sprintf(szNumberOfTriplets,"%02d",c-'0');

                        if(psPacket.Init((SPSRequest*)buffer,2048,uiReqNum++,uiReqType)) {
                            // error - buffer too small
                            printf("SPSRequest.Init failed, buffer too small" );
                            break;
                        }

                        if(!psPacket.AddAttr((SPSRequest*)buffer,2048,SS7GW_IMSI,(const void *)imsi,strlen(imsi))) {
                            // error - buffer too small
                            printf("SPSRequest.AddAttr szIMSI failed, buffer too small" );
                            break;
                        }

                        if(!(len=psPacket.AddAttr((SPSRequest*)buffer,2048,SS7GW_TRIP_NUM,(const void *)szNumberOfTriplets,strlen(szNumberOfTriplets)))) {
                            // error - buffer too small
                            printf("SPSRequest.AddAttr szNumberOfTriplets failed, buffer too small" );
                            break;
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
