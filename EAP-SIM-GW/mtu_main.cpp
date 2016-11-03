/*
                Copyright (C) Dialogic Corporation 1997-2009. All Rights Reserved.

 Name:          mtu_main.c

 Description:   Console command line interface to mtu.

 Functions:     main()


 */

#include "mtu.h"
#include "authinfo.h"
#include "PS_Common.h"
#include "PSPacket.h"

#ifdef WIN32
    #include "WinBase.h"
#endif


#define UNABLE_TO_OPEN_INI_FILE    (-1) /* Option requires immediate exit */
#define INI_FILE_UNRECON_OPTION    (-2) /* Unrecognised option */
#define INI_FILE_RANGE_ERR         (-3) /* Option value is out of range */
#define TOO_MANY_ALLOWED_IPS       (-4) /* Option value is out of range */

/*
 * Default values for MTU's command line options:
 */
#define DEFAULT_MODULE_ID       (0x2d)
#define DEFAULT_MAP_ID          (MAP_TASK_ID)
#define DEFAULT_OPTIONS         (0x000f)
#define DEFAULT_MAX_ACTIVE      (0)
#define DEFAULT_VECTORS_NUM     (3)
#define NO_FREE_CONNECTIONS     (-1)
#define FREE_SOCKET             (0)
#define PARSE_SUCCESS           (0)
#define PARSE_ERROR             (-1)
#define STR_TERMINATOR          ('\0')

#ifdef WIN32
    #define SOCK_ERR WSAGetLastError()
    #define sleep(x) Sleep(x)
#else
    #define SOCK_ERR errno
#endif

const int FIXED_IMSI_LEN = 15;

GW_OPTIONS gwOptions;
GTT_ENTRY gttTable[MAX_GTT_TABLE_SIZE];

std::map<u16,SS7_REQUEST> ss7RequestMap;

/*
 * Program name
 */
const char *program = "EAP-SIM-GW";
FILE* fLog=NULL;
char szLogName[1024];

// socket connection pool
int* client_socket;
struct sockaddr_in* client_addr;
int listen_socket, newsockfd;

int emulation_mode=0;
bool bShutdownInProgress=false;



char socket_recv_buffer[65536];
char socket_send_buffer[65536];

CPSPacket pspRequest;
CPSPacket pspResponse;



extern int MTU_smac(MSG *m);
extern int MTU_open_dlg(u16 ss7dialogueID,char* imsi);
extern int MTU_display_recvd_msg(MSG *m);
extern int MTU_send_User_Abort(u16 ss7dialogueID,u16 ss7invokeID);

void log(const char* szFormat, ...) {
    va_list pArguments;
    char timBuf[20];
    char szBuffer[1024];
    char szNewLogName[1024];
    FILE* fNewLog;

    try {
        va_start(pArguments,szFormat);
        vsprintf(szBuffer, szFormat, pArguments);

        time_t ttToday;
        tm* tmToday;
        time(&ttToday);
        tmToday=localtime(&ttToday);
        strftime(timBuf,19,"%H:%M:%S",tmToday);
        if(fLog) {
    #ifdef WIN32
            sprintf(szNewLogName,"%s\\eapsimgw_%4.4d%2.2d%2.2d.txt",gwOptions.log_path,
                  tmToday->tm_year+1900,tmToday->tm_mon+1,tmToday->tm_mday);
    #else
            sprintf(szNewLogName,"%s/eapsimgw_%4.4d%2.2d%2.2d.txt",gwOptions.log_path,
                    tmToday->tm_year+1900,tmToday->tm_mon+1,tmToday->tm_mday);
    #endif
            if(strcmp(szNewLogName,szLogName)) {
                // date changed, start new log file
                fNewLog=fopen(szNewLogName,"a");
                if(fNewLog)  {
                   // new log file opened successfully
                    fclose(fLog);
                    fLog=fNewLog;
                    strcpy(szLogName,szNewLogName);
                }
                else {
                    fprintf(stderr,"EAP-SIM-GW: Date changed, but new log file opening failed. Continue using old log file\n");
                }
            }
            fprintf(fLog, "%s %s\n",timBuf,szBuffer);
            fflush(fLog);
        }
    }
    catch(...) {
        fprintf(stderr,"EAP-SIM-GW: Exception caught in log function\n");
    }
}


void SetDefaultOptions()
{
    gwOptions.mtu_mod_id = DEFAULT_MODULE_ID;
    gwOptions.mtu_map_id = DEFAULT_MAP_ID;
    gwOptions.map_version = MTU_MAPV3;
    gwOptions.mode = MTU_SEND_AUTH_INFO;
    gwOptions.log_options = DEFAULT_OPTIONS;
    gwOptions.base_dlg_id = BASE_DLG_ID;
    gwOptions.num_dlg_ids = NUM_OF_DLG_IDS;
    gwOptions.max_active = DEFAULT_MAX_ACTIVE;
    gwOptions.service_centre[0] = 0;
    gwOptions.log_path[0] = 0;
    gwOptions.gtt_table_size = 0;
    gwOptions.MAP_timeout= 5;
    gwOptions.IP_port = 5100;
    gwOptions.allowed_clients_num = 0;
    gwOptions.max_connections = 10;
    gwOptions.queue_check_period = 10;
}


int read_option(char* inistr,char* addinfo)
{
  u32  temp_u32;
  size_t pos,pos2,pos3;
  char option_name[1024];
  char option_value[1024];
  int i;
  char *p, *p2;

  try {
  pos=strcspn(inistr," =\t");
  strncpy(option_name,inistr,pos);
  option_name[pos]=0;

  for (i=0; option_name[i]; i++)
    option_name[i]=toupper(option_name[i]);

  inistr += pos;

  if(!strcmp(option_name,"GTT")) {
    // global title translation entry
      pos=strspn(inistr," \t\r\n");
      strcpy(option_value,inistr+pos);
      pos2=strcspn(option_value," \t\r\n");
      option_value[pos2]='\0';
      strcpy(gttTable[gwOptions.gtt_table_size].name,option_value);
      inistr += pos+pos2;

      pos=strspn(inistr," \t\r\n");
      strcpy(option_value,inistr+pos);
      pos2=strcspn(option_value," \t\r\n");
      option_value[pos2]='\0';

      // check IMSI prefix format
      if(!strlen(option_value))  {
          sprintf(addinfo,"Zero length IMSI prefix given");
          return(INI_FILE_RANGE_ERR);
      }
      pos3=strspn(option_value,"0123456789");
      if(pos3<strlen(option_value)) {
          if(strcmp(option_value,"+")) {
            sprintf(addinfo,"IMSI prefix should consist of digits or '+' wildcard.");
            return(INI_FILE_RANGE_ERR);
          }
      }
      if(strlen(option_value)>15) {
          sprintf(addinfo,"IMSI prefix too long. It should not exceed 15 digits.");
          return(INI_FILE_RANGE_ERR);
      }

      strcpy(gttTable[gwOptions.gtt_table_size].imsi_prefix,option_value);
      inistr += pos+pos2;

      gttTable[gwOptions.gtt_table_size].numplan = 0;
      pos=strspn(inistr," \t\r\n");
      strcpy(option_value,inistr+pos);
      pos2=strcspn(option_value," \t\r\n");
      option_value[pos2]='\0';
      option_value[0]=toupper(option_value[0]);
      if(!strcmp(option_value,"E.164"))
         gttTable[gwOptions.gtt_table_size].numplan = 1;
      if(!strcmp(option_value,"E.212"))
         gttTable[gwOptions.gtt_table_size].numplan = 6;
      if(!strcmp(option_value,"E.214"))
         gttTable[gwOptions.gtt_table_size].numplan = 7;
      if(!gttTable[gwOptions.gtt_table_size].numplan) {
          sprintf(addinfo,"Invalid translation numplan. Possible values are E.164, E.212 and E.214");
          return(INI_FILE_RANGE_ERR);
      }
      strcpy(gttTable[gwOptions.gtt_table_size].str_numplan,option_value);
      inistr += pos+pos2;

      pos=strspn(inistr," \t\r\n");
      strcpy(option_value,inistr+pos);
      pos2=strcspn(option_value," \t\r\n");
      option_value[pos2]='\0';

      // check translation pattern format
      if(!strlen(option_value))  {
          sprintf(addinfo,"Zero length translation pattern given");
          return(INI_FILE_RANGE_ERR);
      }
      if(!strcmp(option_value,"+")) {
          strcpy(gttTable[gwOptions.gtt_table_size].ptn_replace,option_value);
          gttTable[gwOptions.gtt_table_size].ptn_keep_remove=1;
      }
      else {
          p=strchr(option_value,'/');
          if(!p) {
              sprintf(addinfo,"At least one delimiter needed at translation pattern (or '+' for wildcard)");
              return(INI_FILE_RANGE_ERR);
          }
          if(p==(char*)option_value) {
              sprintf(addinfo,"Translation pattern cannot start with delimiter");
              return(INI_FILE_RANGE_ERR);
          }
          else {
              strcpy(gttTable[gwOptions.gtt_table_size].ptn_replace,option_value);
              gttTable[gwOptions.gtt_table_size].ptn_replace[p-option_value]=0;
              p2=strchr(p+1,'/');
              if(p2) {
                  sprintf(addinfo,"Only one delimiter is allowed at translation pattern");
                  return(INI_FILE_RANGE_ERR);
              }
          }
          if(!strcmp(p+1,"+")) {
              gttTable[gwOptions.gtt_table_size].ptn_keep_remove=1;
          }
          else {
              if(!strcmp(p+1,"-")) {
                  gttTable[gwOptions.gtt_table_size].ptn_keep_remove=0;
              }
              else {
                  sprintf(addinfo,"Only '+' or '-' are allowed after delimiter at translation pattern");
                  return(INI_FILE_RANGE_ERR);
              }

          }
      }
      inistr += pos+pos2;

      pos=strspn(inistr," \t\r\n");
      strcpy(option_value,inistr+pos);
      pos2=strcspn(option_value," \t\r\n");
      option_value[pos2]='\0';
      if(!strlen(option_value)) {
          // reserve translation is not set
          gttTable[gwOptions.gtt_table_size].reserve_numplan=0;
          gwOptions.gtt_table_size++;
          return 0;
      }
      if(option_value[0]=='#') {
          gwOptions.gtt_table_size++;
          return 0;
      }
      option_value[0]=toupper(option_value[0]);
      if(!strcmp(option_value,"E.164"))
         gttTable[gwOptions.gtt_table_size].reserve_numplan = 1;
      if(!strcmp(option_value,"E.212"))
         gttTable[gwOptions.gtt_table_size].reserve_numplan = 6;
      if(!strcmp(option_value,"E.214"))
         gttTable[gwOptions.gtt_table_size].reserve_numplan = 7;
      if(!gttTable[gwOptions.gtt_table_size].reserve_numplan) {
          sprintf(addinfo,"Invalid reserve translation numplan. Possible values are E.164, E.212 and E.214");
          return(INI_FILE_RANGE_ERR);
      }
      strcpy(gttTable[gwOptions.gtt_table_size].str_reserve_numplan,option_value);
      inistr += pos+pos2;

      pos=strspn(inistr," \t\r\n");
      strcpy(option_value,inistr+pos);
      pos2=strcspn(option_value," \t\r\n");
      option_value[pos2]='\0';

      // check translation pattern format
      if(!strlen(option_value))  {
          sprintf(addinfo,"Zero length reserve translation pattern given");
          return(INI_FILE_RANGE_ERR);
      }
      if(!strcmp(option_value,"+")) {
          strcpy(gttTable[gwOptions.gtt_table_size].reserve_ptn_replace,option_value);
          gttTable[gwOptions.gtt_table_size].reserve_ptn_keep_remove=1;
      }
      else {
          p=strchr(option_value,'/');
          if(!p) {
              sprintf(addinfo,"At least one delimiter needed at reserve translation pattern (or '+' for wildcard)");
              return(INI_FILE_RANGE_ERR);
          }
          if(p==(char*)option_value) {
              sprintf(addinfo,"Reserve translation pattern cannot start with delimiter");
              return(INI_FILE_RANGE_ERR);
          }
          else {
              strcpy(gttTable[gwOptions.gtt_table_size].reserve_ptn_replace,option_value);
              gttTable[gwOptions.gtt_table_size].reserve_ptn_replace[p-option_value]=0;
              p2=strchr(p+1,'/');
              if(p2) {
                  sprintf(addinfo,"Only one delimiter is allowed at reserve translation pattern");
                  return(INI_FILE_RANGE_ERR);
              }
          }
          if(!strcmp(p+1,"+")) {
              gttTable[gwOptions.gtt_table_size].reserve_ptn_keep_remove=1;
          }
          else {
              if(!strcmp(p+1,"-")) {
                  gttTable[gwOptions.gtt_table_size].reserve_ptn_keep_remove=0;
              }
              else {
                  sprintf(addinfo,"Only '+' or '-' are allowed after delimiter at reserve translation pattern");
                  return(INI_FILE_RANGE_ERR);
              }

          }
      }

      gwOptions.gtt_table_size++;

    return 0;
  }

  pos=strspn(inistr," =\t");
  strcpy(option_value,inistr+pos);
  pos=strcspn(option_value," #=\t\r\n");
  option_value[pos]='\0';

  if(!strcmp(option_name,"EAPSIMGW_MODULE_ID")) {
    if (strtou32(&temp_u32, option_value) != 0)
          return(INI_FILE_RANGE_ERR);
        gwOptions.mtu_mod_id = (u8)temp_u32;
        return 0;
  }


  if(!strcmp(option_name,"MAP_MODULE_ID")) {
     if (strtou32(&temp_u32, option_value) != 0)
          return(INI_FILE_RANGE_ERR);
        gwOptions.mtu_map_id = (u8)temp_u32;
        return 0;
  }

  if(!strcmp(option_name,"MAP_VERSION")) {
    if (strtou32(&temp_u32, option_value) != 0)
          return(INI_FILE_RANGE_ERR);
    switch(temp_u32)
          {
            case MTU_MAPV1:
            case MTU_MAPV2:
            case MTU_MAPV3:
              gwOptions.map_version = (u8)temp_u32;
              break;
            default:
              return(INI_FILE_RANGE_ERR);
          }
    return 0;
  }

  if(!strcmp(option_name,"SERVICE_CENTRE")) {
     strcpy(gwOptions.service_centre,option_value);
     return 0;
  }

  if(!strcmp(option_name,"BASE_DIALOGUE")) {
      if (strtou32(&temp_u32, option_value) != 0)
        return(INI_FILE_RANGE_ERR);
      gwOptions.base_dlg_id = (u16)temp_u32;
      return 0;
  }

  if(!strcmp(option_name,"NUMBER_OF_DIALOGUES")) {
      if (strtou32(&temp_u32,option_value) != 0)
        return(INI_FILE_RANGE_ERR);
      gwOptions.num_dlg_ids = (u16)temp_u32;
      return 0;
  }

  if(!strcmp(option_name,"LOG_OPTIONS")) {
      if (strtou32(&temp_u32,option_value) != 0)
        return(INI_FILE_RANGE_ERR);
      gwOptions.log_options = (u16)temp_u32;
      return 0;
  }

  if(!strcmp(option_name,"LOG_PATH")) {
     strcpy(gwOptions.log_path,option_value);
     return 0;
  }

  if(!strcmp(option_name,"MAP_TIMEOUT")) {
      if (strtou32(&temp_u32, option_value) != 0)
        return(INI_FILE_RANGE_ERR);
      gwOptions.MAP_timeout = (u16)temp_u32;
      return 0;
  }

  if(!strcmp(option_name,"IP_PORT")) {
      if (strtou32(&temp_u32, option_value) != 0)
        return(INI_FILE_RANGE_ERR);
      gwOptions.IP_port = (u16)temp_u32;
      return 0;
  }

  if(!strcmp(option_name,"MAX_CONNECTIONS")) {
      if (strtou32(&temp_u32, option_value) != 0)
        return(INI_FILE_RANGE_ERR);
      gwOptions.max_connections = (u16)temp_u32;
      return 0;
  }

  if(!strcmp(option_name,"ALLOWED_IP")) {
      if(gwOptions.allowed_clients_num < MAX_ALLOWED_CLIENT_IPS) {
        strncpy(gwOptions.allowed_IP[gwOptions.allowed_clients_num],option_value,40);
        gwOptions.allowed_IP[gwOptions.allowed_clients_num++][39] = '\0';
      }
      else
          return TOO_MANY_ALLOWED_IPS;

      return 0;
  }

  return(INI_FILE_UNRECON_OPTION);
  }
  catch(...) {
      log("Exception caught in read_option");
      return -999;
  }
}


/*
 * Read in initial settings from file EAP-SIM-GW.ini and set the system variables accordingly.
 *
 * Returns 0 on success; on error returns non-zero and
 * writes the parameter index which caused the failure
 * to the variable arg_index.
 */
int read_initial_settings(char* ini_file_name, char* error_str)
{
  int error;
  char inistr[1024];
  char* p;
  char addinfo[1024];

  SetDefaultOptions();

  FILE* fSettings = fopen ( ini_file_name, "rt" );
  if(!fSettings)
      return UNABLE_TO_OPEN_INI_FILE;

  addinfo[0]=0;
  while (fgets(inistr,1024,fSettings))
  {
      p=inistr;
      p=p+strspn(p," \t\r\n");
      if(p[0]=='#' || p[0]==0){
          // this line is a comment or string is only a set of separators, ingore it
          continue;
      }
    if ((error = read_option(p,addinfo)) != 0)
    {
        strcpy(error_str, p);
        if(strlen(addinfo))
            strcat(error_str, addinfo);
        return(error);
    }
  }
  fclose(fSettings);
  return(0);
}

const char* IPAddr2Text(in_addr* pinAddr,char* buffer,int buf_size)
{
#ifdef WIN32
    snprintf(buffer,63,"%d.%d.%d.%d",pinAddr->S_un.S_un_b.s_b1, pinAddr->S_un.S_un_b.s_b2, pinAddr->S_un.S_un_b.s_b3, pinAddr->S_un.S_un_b.s_b4);
    return buffer;
#else
    return inet_ntop(AF_INET,(const void*)pinAddr,buffer,buf_size);
#endif
}


int GetFreeSS7Dialogue()
{
    u16 dlg_id = gwOptions.base_dlg_id;
    std::map<u16, SS7_REQUEST>::iterator iter = ss7RequestMap.begin();
    while(iter != ss7RequestMap.end()) {
        if(iter->first > dlg_id)
            break;
        iter++;
        dlg_id++;
        if(dlg_id >= gwOptions.base_dlg_id + gwOptions.num_dlg_ids) {
            // overflow of open dialogues
            return -1;
        }
    }
    return dlg_id;
}


int CreateNewRequestAndAddToQueue(REQUEST_TYPE requestType, int clientRequestNum, u32 gatewayRequestID,
                                  char* imsi, int requestedVectorsNum, int sockIndex)
{
    int dlg_id = GetFreeSS7Dialogue();
    if (dlg_id < 0)
        return -1;
    SS7_REQUEST ss7Request(requestType, clientRequestNum, gatewayRequestID, imsi, requestedVectorsNum, sockIndex, dlg_id);

    ss7RequestMap.insert( std::pair<u16,SS7_REQUEST>(dlg_id,ss7Request) );

    return dlg_id;
}


void AddULongAttrToPacket(CPSPacket& psPacket, u32 attrID, u32 attrValue, u8* buffer, u32 bufferSize)
{
    u32 attrValueN = htonl(attrValue);
    psPacket.AddAttr((SPSRequest*)buffer, bufferSize, attrID, reinterpret_cast<void*>(&attrValueN), sizeof(attrValueN));
}

void AddUShortAttrToPacket(CPSPacket& psPacket, u32 attrID, u16 attrValue, u8* buffer, u32 bufferSize)
{
    u16 attrValueN = htons(attrValue);
    psPacket.AddAttr((SPSRequest*)buffer, bufferSize, attrID, reinterpret_cast<void*>(&attrValueN), sizeof(attrValueN));
}


void AddAuthInfoToResponse(CPSPacket& pspResponse, u16 attrID, size_t totalSize, size_t elementsNum, u8** data)
{
    u8* buffer = (u8*) malloc(totalSize);
    u16 oneElementSize = totalSize / elementsNum;
    for (size_t i = 0; i < elementsNum; i++) {
        memcpy(buffer + i*oneElementSize, data[i], oneElementSize);
    }
    pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),
                        attrID, reinterpret_cast<void*>(buffer), totalSize);
    free(buffer);
}


int GetPSAttributeID(AuthAttributeType type, int vectorNum)
{
    switch(type) {
    case RAND:
        switch(vectorNum) {
        case 1:
            return RS_RAND1;
        case 2:
            return RS_RAND2;
        case 3:
            return RS_RAND3;
        case 4:
            return RS_RAND4;
        case 5:
            return RS_RAND5;
        }
    case KC:
        switch(vectorNum) {
        case 1:
            return RS_KC1;
        case 2:
            return RS_KC2;
        case 3:
            return RS_KC3;
        case 4:
            return RS_KC4;
        case 5:
            return RS_KC5;
        }
    case SRES:
        switch(vectorNum) {
        case 1:
            return RS_SRES1;
        case 2:
            return RS_SRES2;
        case 3:
            return RS_SRES3;
        case 4:
            return RS_SRES4;
        case 5:
            return RS_SRES5;
        }
    }
    return -1;
}


void PrintBinaryDumpToString(char* string, size_t stringSize, const unsigned char* data, size_t dataSize)
{
    size_t i = 0;
    for (; i < dataSize; i++) {
        if (2 * (i + 1) >= stringSize - 1)
            break;
        sprintf(&string[2 * i], "%02x", data[i]);
    }
    string[2 * (i + 1)] = '\0';
}


void FillPacketWithTriplets(SS7_REQUEST& ss7Req, CPSPacket& responsePacket)
{
    ss7Req.TranslateVectorsToTriplets();
    int vectorNum = 1;
    char strData[256];
    for(auto vectorIt = ss7Req.authVectors.begin(); vectorIt != ss7Req.authVectors.end(); vectorIt++) {
        auto attrIt = vectorIt->attrs.find(RAND);
        PrintBinaryDumpToString(strData, sizeof(strData), attrIt->second.data.data(), attrIt->second.data.size());
        if(gwOptions.log_options && MTU_TRACE_RX) {
            log("RAND%d: %s", vectorNum, strData);
        }
        responsePacket.AddAttr((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer),
                               GetPSAttributeID(RAND, vectorNum), strData, strlen(strData));

        attrIt = vectorIt->attrs.find(KC);
        PrintBinaryDumpToString(strData, sizeof(strData), attrIt->second.data.data(), attrIt->second.data.size());
        if(gwOptions.log_options && MTU_TRACE_RX) {
            log("KC%d: %s", vectorNum, strData);
        }
        responsePacket.AddAttr((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer),
                                   GetPSAttributeID(KC, vectorNum), strData, strlen(strData));

        attrIt = vectorIt->attrs.find(SRES);
        PrintBinaryDumpToString(strData, sizeof(strData), attrIt->second.data.data(), attrIt->second.data.size());
        if(gwOptions.log_options && MTU_TRACE_RX) {
            log("SRES%d: %s", vectorNum, strData);
        }
        responsePacket.AddAttr((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer),
                                   GetPSAttributeID(SRES, vectorNum), strData, strlen(strData));
        vectorNum++;
    }
}

void LogBinaryDumpIfNeeded(const char* descr, const std::vector<u8>& dump)
{
    if(gwOptions.log_options && MTU_TRACE_RX) {
        char strData[256];
        PrintBinaryDumpToString(strData, sizeof(strData), dump.data(), dump.size());
        log("%s: %s", descr, strData);
    }
}

void FillPacketWithQuintuplets(SS7_REQUEST& ss7Req, CPSPacket& responsePacket)
{
    ss7Req.FillConcatenatedVectors();
    pspResponse.AddAttr((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer),
                        ATTR_RAND, reinterpret_cast<void*>(ss7Req.concatRAND.data()), ss7Req.concatRAND.size());
    LogBinaryDumpIfNeeded("RAND", ss7Req.concatRAND);
    pspResponse.AddAttr((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer),
                        ATTR_XRES, reinterpret_cast<void*>(ss7Req.concatXRES.data()), ss7Req.concatXRES.size());
    LogBinaryDumpIfNeeded("XRES", ss7Req.concatXRES);
    pspResponse.AddAttr((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer),
                        ATTR_CK, reinterpret_cast<void*>(ss7Req.concatCK.data()), ss7Req.concatCK.size());
    LogBinaryDumpIfNeeded("CK", ss7Req.concatCK);
    pspResponse.AddAttr((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer),
                        ATTR_IK, reinterpret_cast<void*>(ss7Req.concatIK.data()), ss7Req.concatIK.size());
    LogBinaryDumpIfNeeded("IK", ss7Req.concatIK);
    pspResponse.AddAttr((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer),
                        ATTR_AUTN, reinterpret_cast<void*>(ss7Req.concatAUTN.data()), ss7Req.concatAUTN.size());
    LogBinaryDumpIfNeeded("AUTN", ss7Req.concatAUTN);
}


bool ValidateAndSetRequestParams(REQUEST_TYPE requestType, const std::multimap<__uint16_t, SPSReqAttrParsed>& mmRequest,
                                        char* imsi, int& vectorsNum, char* errorDescr)
{
    std::multimap<__uint16_t, SPSReqAttrParsed>::const_iterator iter = mmRequest.find(SS7GW_IMSI);
    if(iter == mmRequest.end()) {
        sprintf(errorDescr, "IMSI is not given in auth vectors request");
        return false;
    }
    if(iter->second.m_usDataLen != FIXED_IMSI_LEN) {
        sprintf(errorDescr,"Wrong IMSI %s given. IMSI must be %d symbols long.", imsi, FIXED_IMSI_LEN);
        return false;
    }
    strncpy(imsi, reinterpret_cast<char*>(iter->second.m_pvData), FIXED_IMSI_LEN);
    imsi[iter->second.m_usDataLen] = STR_TERMINATOR;

    vectorsNum = DEFAULT_VECTORS_NUM;
    if (requestType == TRIPLET_REQUEST) {
        iter = mmRequest.find(SS7GW_TRIP_NUM);
        if(iter != mmRequest.end()) {
            char attr_value[256];
            strncpy(attr_value, reinterpret_cast<char*>(iter->second.m_pvData), iter->second.m_usDataLen);
            attr_value[iter->second.m_usDataLen] = STR_TERMINATOR;
            u32  temp_u32;
            if (strtou32(&temp_u32, attr_value) != 0) {
                sprintf(errorDescr,"Wrong number of auth vectors requested (%s)", attr_value);
                return false;
            }
            vectorsNum = (u16)temp_u32;
        }
    }
    else if (requestType == QUINTUPLET_REQUEST) {
        iter = mmRequest.find(ATTR_REQUESTED_VECTORS);
        if(iter != mmRequest.end()) {
            vectorsNum = ntohs(*(u16*)(iter->second.m_pvData));
            log("Num of quintuplets requested: %d", vectorsNum);
        }
    }

    if(vectorsNum < MIN_VECTORS_NUM || vectorsNum > MAX_VECTORS_NUM) {
      sprintf(errorDescr,"Number of requested auth vectors must be from %d to %d. Requested number=%d",
               MIN_VECTORS_NUM, MAX_VECTORS_NUM, vectorsNum);
      return false;
    }
    return true;
}


bool SendRequestConfirmation(REQUEST_TYPE ss7ReqType, bool requestAccepted, u32 clientRequestNum, u32 gwRequestID,
                             const char* errorDescr, int socketIndex)
{
    const int REQUEST_ACCEPTED = 0;
    const int REQUEST_REJECTED = 1;
    CPSPacket pspResponse;
    if(pspResponse.Init((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer), clientRequestNum,
                        ss7ReqType == TRIPLET_REQUEST ? SS7GW_IMSI_RESP : SS7GW_QUINTUPLET_CONF) != 0) {
        // error - buffer too small
        log("Initializing response buffer failed, buffer too small" );
        fprintf(stderr, "%s: Initializing response buffer failed, buffer too small\n", program);
        return false;
    }
    if(requestAccepted) {
        // request accepted, send confirmation to client
        if (ss7ReqType == TRIPLET_REQUEST) {
            // for triplet request send response as string
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer, sizeof(socket_send_buffer), PS_RESULT, "00", 2);
        }
        else {
            // for quintuplet request send response code as number
            AddULongAttrToPacket(pspResponse, PS_RESULT, REQUEST_ACCEPTED, (u8*)socket_send_buffer, sizeof(socket_send_buffer));
            AddULongAttrToPacket(pspResponse, GW_REQUEST_ID, gwRequestID, (u8*)socket_send_buffer, sizeof(socket_send_buffer));
        }
    }
    else {
        // error in request parameters
        if (ss7ReqType == TRIPLET_REQUEST) {
            // for triplet request send response as string
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),
                                PS_RESULT, "01", 2);
        }
        else {
            AddULongAttrToPacket(pspResponse, PS_RESULT, REQUEST_REJECTED, (u8*)socket_send_buffer, sizeof(socket_send_buffer));
        }
        pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),
                            PS_DESCR, errorDescr, strlen(errorDescr));

    }

    if(!send(client_socket[socketIndex], socket_send_buffer, ntohs(((SPSRequest*)socket_send_buffer)->m_usPackLen), 0)) {
        // sending response failed
      log("Sending request accept confirmation to client failed: %d", SOCK_ERR );
      return false;
    }
    return true;
}

void SendRequestResultToClient(int dlg_id)
{
    char address_buffer[64];
    SS7_REQUEST& ss7Req=ss7RequestMap.at(dlg_id);
    const int REQUEST_SUCCESS = 0;
    const int REQUEST_FAILURE = 1;

    int packetInitRes;
    if (ss7Req.request_type == TRIPLET_REQUEST) {
        packetInitRes = pspResponse.Init((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),
                               ss7Req.clientRequestNum, RS_TRIP_REQ);

       }
    else if (ss7Req.request_type == QUINTUPLET_REQUEST) {
        packetInitRes = pspResponse.Init((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),
                               ss7Req.gatewayRequestID, SS7GW_QUINTUPLET_RESP);
        if (!ss7Req.AllVectorsHaveType(QUINTUPLET)) {
            ss7Req.SetSuccess(false);
            ss7Req.error = "Requested for quintuplets but received triplets or some other info";
        }
    }
    if(packetInitRes != 0) {
        // error - buffer too small
        log("Send_Response_to_Client: initializing packet failed, buffer too small" );
        fprintf(stderr, "%s: SendRequestResultToClient: initializing packet failed, buffer too small\n",program );
        return;
    }

    if (ss7Req.successful) {
        if (ss7Req.request_type == TRIPLET_REQUEST) {
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),PS_RESULT, "00", 2);
            FillPacketWithTriplets(ss7Req, pspResponse);
        }
        else  {
            AddULongAttrToPacket(pspResponse, PS_RESULT, REQUEST_SUCCESS,
                                 (u8*)socket_send_buffer, sizeof(socket_send_buffer));
            AddUShortAttrToPacket(pspResponse, ATTR_RECEIVED_VECTORS, ss7Req.receivedVectorsNum,
                                   (u8*)socket_send_buffer, sizeof(socket_send_buffer));
            FillPacketWithQuintuplets(ss7Req, pspResponse);
        }
        log(">>> Request ID %d SUCCESS. Dialogue_id 0x%04x, IMSI %s, vectors requested %d, vectors received %d. "
            "Sending %d bytes to %s.(connection #%d).",
            ss7Req.gatewayRequestID, dlg_id, ss7Req.imsi, ss7Req.requestedVectorsNum, ss7Req.receivedVectorsNum,
            ntohs(((SPSRequest*)socket_send_buffer)->m_usPackLen),
            IPAddr2Text(&client_addr[ss7Req.sockIndex].sin_addr, address_buffer, sizeof(address_buffer)), ss7Req.sockIndex);
    }
    else {
        if (ss7Req.request_type == TRIPLET_REQUEST) {
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),PS_RESULT, "01", 2);
        }
        else {
            AddULongAttrToPacket(pspResponse, PS_RESULT, REQUEST_FAILURE, (u8*)socket_send_buffer, sizeof(socket_send_buffer));
        }
        pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),PS_DESCR,
                            ss7Req.error.c_str(),ss7Req.error.length());
        log(">>> Request #%d FAILURE: %s.",ss7Req.clientRequestNum,ss7Req.error.c_str());
        log(">>> Dialogue_id 0x%04x, IMSI %s. Sending %d bytes to %s (connection #%d).",
            dlg_id,ss7Req.imsi,ntohs(((SPSRequest*)socket_send_buffer)->m_usPackLen),
            IPAddr2Text(&client_addr[ss7Req.sockIndex].sin_addr,address_buffer,sizeof(address_buffer)),
            ss7Req.sockIndex);
    }

    if(!send(client_socket[ss7Req.sockIndex],socket_send_buffer,ntohs(((SPSRequest*)socket_send_buffer)->m_usPackLen),0)) {
        log("sending response failed: %d",SOCK_ERR);
        fprintf(stderr, "%s: sending response failed: %d\n",program,SOCK_ERR);
    }
}


/* This function is called from MTU_smac when close indication arrives and dialogue is finished */
void OnDialogueFinish(u16 dlg_id)
{
    try {
    SS7_REQUEST& ss7Req=ss7RequestMap.at(dlg_id);
    int req_num=ss7Req.clientRequestNum;

    if(ss7Req.successful && ss7Req.receivedVectorsNum<ss7Req.requestedVectorsNum) {
        // some triplets are received, but we need more of them. Initiate new SS7 requests
        log("Request #%d: Requested %d triplets but received %d only. Requesting for extra triplets...",
            ss7Req.clientRequestNum,ss7Req.requestedVectorsNum,ss7Req.receivedVectorsNum);
        if (MTU_open_dlg(dlg_id,ss7Req.imsi) != 0) {
            // sending new SS7 request failed
              ss7Req.state = rs_finished;
              SendRequestResultToClient(dlg_id);
              ss7RequestMap.erase(dlg_id);
        }
        ss7RequestMap.at(dlg_id).state = rs_wait_opn_cnf;
        time(&ss7RequestMap.at(dlg_id).stateChangeTime);
    }
    else {
        SendRequestResultToClient(dlg_id);
        ss7RequestMap.erase(dlg_id);
        log("Processing request #%d finished, dialogue_id 0x%04x closed. Queue size=%d",req_num,dlg_id,ss7RequestMap.size());
     }
    }
    catch(...) {
        log("Exception caught in OnDialogueFinish");
    }
}


/* Function processes next request from socket_recv_buffer.
 * buffer_shift - starting point of next request in buffer.
 * Returned value is the length of processed request.
 */
int ProcessNextRequestFromBuffer(int socketIndex, int buffer_shift, int dataLen) {
    char errorDescr[2048];
    char imsi[20];
    static u32 gwRequestID = 0;
    int requestedVectorsNum = 0;
    u32 clientRequestNum;
    u16 requestType;
    u16 packetLen;
    const int VALIDATE_PACKET = 1;

    imsi[0] = STR_TERMINATOR;
    std::multimap<__uint16_t, SPSReqAttrParsed> mmRequest;
    int parseRes = pspRequest.Parse((SPSRequest *)(socket_recv_buffer+buffer_shift), dataLen,
                      clientRequestNum, requestType, packetLen, mmRequest, VALIDATE_PACKET);
    log("Parse result: %d", parseRes);
    if (parseRes == PARSE_ERROR) {
        log("Unable to parse incoming packet");
        return PARSE_ERROR;
    }
    if(requestType != SS7GW_IMSI_REQ && requestType != SS7GW_QUINTUPLET_REQ) {
        // leave without processing
        return packetLen;
    }

    bool bRequestAccepted = false;
    REQUEST_TYPE ss7ReqType = requestType == SS7GW_IMSI_REQ ? TRIPLET_REQUEST : QUINTUPLET_REQUEST;
    if (ValidateAndSetRequestParams(ss7ReqType, mmRequest, imsi, requestedVectorsNum, errorDescr)) {
         gwRequestID++;
         u32 ss7dialogueID = GetFreeSS7Dialogue();
         log("Dialog allocated: %d", ss7dialogueID);
         if (ss7dialogueID >= 0) {
              SS7_REQUEST ss7Request(ss7ReqType, clientRequestNum, gwRequestID, imsi, requestedVectorsNum,
                                     socketIndex, ss7dialogueID);
              ss7RequestMap.insert( std::pair<u16,SS7_REQUEST>(ss7dialogueID, ss7Request) );
              bRequestAccepted = true;
         }
         else {
              log("GetFreeSS7Dialogue returned %d. All SS7 dialogues are busy, request rejected. Queue size: %d",
                  ss7dialogueID, ss7RequestMap.size());
              sprintf(errorDescr, "All SS7 dialogues are busy, request rejected.");
         }

        if(bRequestAccepted) {
            log("Client request #%d accepted. Gateway request ID %d, dialogue_id 0x%04x allocated. IMSI %s, auth vectors "
                "requested: %d. Queue size: %d",
                clientRequestNum, gwRequestID, ss7dialogueID, imsi, requestedVectorsNum, ss7RequestMap.size());
        }
        else {
            // error in request parameters
            log("Client request #%d for IMSI %s and vectors num %d rejected with reason %s", clientRequestNum,
                imsi, requestedVectorsNum, errorDescr);
        }

        SendRequestConfirmation(ss7ReqType, bRequestAccepted, clientRequestNum, gwRequestID, errorDescr, socketIndex);

        if(bRequestAccepted) {
            if (MTU_open_dlg(ss7dialogueID, imsi) != 0) {
                  ss7RequestMap.at(ss7dialogueID).state = rs_finished;
                  SendRequestResultToClient(ss7dialogueID);
                  ss7RequestMap.erase(ss7dialogueID);
                  return packetLen;
            }
            ss7RequestMap.at(ss7dialogueID).state = rs_wait_opn_cnf;
            time(&ss7RequestMap.at(ss7dialogueID).stateChangeTime);
        }
    }
    return packetLen;
}


void ProcessPendingSS7Messages()
{
    HDR *h;               /* header of received message */
    MSG *m;               /* received message */
    try {
        while ((h = GCT_grab(gwOptions.mtu_mod_id)) != 0) {
            // While we have SS7 messages, process them
            m = (MSG *)h;
            switch (m->hdr.type)
            {
              case MAP_MSG_DLG_IND:
              case MAP_MSG_SRV_IND:
                if (gwOptions.log_options & MTU_TRACE_RX)
                  MTU_display_recvd_msg(m);
                MTU_smac(m);
                break;

              default:
                /*
                 * Under normal operation we don't expect to receive anything
                 * else but if we do report the messages.
                 */
                if (gwOptions.log_options & MTU_TRACE_RX)
                  MTU_display_recvd_msg(m);
                MTU_disp_err(((HDR *)m)->id,"Unexpected message type");
                break;
            }
            /*
             * Once we have finished processing the message
             * it must be released to the pool of messages.
             */
            relm(h);
        }
    }
    catch(...) {
        log("Exception caught while processing SS7 messages loop");
    }
}


// Function CheckRequestQueue checks request queue for hanging dialogues and releases it
void DiscardHangingRequests(time_t tNow)
{
    bool bTimeoutReqFound=false;

    try {
    std::map<u16,SS7_REQUEST>::iterator iter = ss7RequestMap.begin();
    while(iter != ss7RequestMap.end()) {
        if(tNow - iter->second.stateChangeTime > gwOptions.MAP_timeout ) {
            MTU_send_User_Abort(iter->second.ss7dialogueID, iter->second.ss7invokeID);
            iter->second.error="MAP time-out";
            OnDialogueFinish(iter++ -> second.ss7dialogueID); // increment iter here, 'cause OnDialogueFinish call map.erase, after what iterator becomes invalid
            bTimeoutReqFound=true;
         }
         else
            iter++;

    }
    if(bTimeoutReqFound)
        log("Queue size after discarding hanging requests: %d", ss7RequestMap.size());
    }
    catch(...) {
        log("Exception caught in DiscardHangingRequests");
    }
}


void DiscardHangingRequestsSchedule()
{
    static time_t lastQueueCheckTime = time(NULL); // initialise last queue check time
    time_t now;
    time(&now);
    if(now - lastQueueCheckTime > gwOptions.queue_check_period) {
        DiscardHangingRequests(now);
        time(&lastQueueCheckTime);
    }
}

bool IsShutdownSignalSet()
{
    const char* stopFilename = "EAP-SIM-GW.stop";
    FILE* stopFile;
    if((stopFile = fopen(stopFilename, "r"))) {
      log("Stop file found. Shutting gateway down ...");
      fclose(stopFile);
      remove(stopFilename);
      return true;
    }
    else {
        return false;
    }
}

void CloseSocket(int socket)
{
    #ifdef WIN32
        shutdown(socket, SD_BOTH);
        closesocket(socket);
    #else
        shutdown(socket, SHUT_RDWR);
        close(socket);
    #endif
}

void CloseSockets()
{
    for(int i=0; i < gwOptions.max_connections; i++) {
        if(client_socket[i] != 0) {
            CloseSocket(client_socket[i]);
        }
    }
    free(client_socket);
    free(client_addr);
    CloseSocket(listen_socket);
}

bool ProcessIncomingConnection()
{
    #ifdef WIN32
      int clilen;
    #else
      socklen_t clilen;
    #endif
    struct sockaddr_in newclient_addr;
    char address_buffer[64];
    clilen = sizeof(newclient_addr);
    int newsockfd = accept(listen_socket, (struct sockaddr *)&newclient_addr, &clilen);
    if (newsockfd < 0) {
       log("Failed to call accept on socket: %d", SOCK_ERR);
       return false;
    }

    log("Incoming connection from %s",IPAddr2Text(&newclient_addr.sin_addr, address_buffer, sizeof(address_buffer)));

    int freeSocketIndex = NO_FREE_CONNECTIONS;
    for(int i=0; i < gwOptions.max_connections; i++) {
        if(client_socket[i] == 0) {
            freeSocketIndex = i;
            break;
        }
    }
    if (freeSocketIndex == NO_FREE_CONNECTIONS || bShutdownInProgress) {
        if(freeSocketIndex == NO_FREE_CONNECTIONS) {
            log("Maximum allowed connections reached. Rejecting connection...");
        }
        else {
            log("Shutdown in progress. Rejecting connection...");
        }
        CloseSocket(newsockfd);
        return false;
    }
    if(gwOptions.allowed_clients_num > 0) {
        bool bConnectionAllowed = false;
        // check connection for white list of allowed clients
        for(int j=0; j < gwOptions.allowed_clients_num; j++)
            if(!strcmp(gwOptions.allowed_IP[j], address_buffer)) {
                bConnectionAllowed=true;
                break;
            }
        if(!bConnectionAllowed) {
            log("Client address %s not found in white list of allowed IPs. Rejecting connection...",address_buffer);
            CloseSocket(newsockfd);
            return false;
        }
    }

    client_socket[freeSocketIndex] = newsockfd;
    memcpy(&client_addr[freeSocketIndex], &newclient_addr, sizeof(newclient_addr));
    log("Connection #%d accepted.", freeSocketIndex);
    return true;
}

bool ProcessIncomingData(int sockIndex)
{
    int nReceived = recv(client_socket[sockIndex], socket_recv_buffer, sizeof(socket_recv_buffer), 0);
    if(nReceived <= 0) {
        return false;
    }
    char address_buffer[64];
    log("%d bytes received from %s (connection #%d). Processing request(s) ...", nReceived,
        IPAddr2Text(&client_addr[sockIndex].sin_addr, address_buffer, sizeof(address_buffer)), sockIndex);
    int nBytesProcessed = 0;
    while(nBytesProcessed < nReceived) {
      int nRequestLen = ProcessNextRequestFromBuffer(sockIndex, nBytesProcessed, nReceived);
      if(nRequestLen >= 0)
          nBytesProcessed += nRequestLen;
      else
          break;
    }
    return true;
}


bool ProcessSocketEvents()
{
    struct timeval tv;
    tv.tv_sec = 0;  // time-out
    tv.tv_usec = 100;
    fd_set read_set, write_set;
    FD_ZERO( &read_set );
    FD_ZERO( &write_set );
    FD_SET( listen_socket, &read_set );
    int max_socket = listen_socket;
    for(size_t i = 0; i < gwOptions.max_connections; i++) {
        if(client_socket[i] != FREE_SOCKET) {
            FD_SET( client_socket[i], &read_set );
            if(client_socket[i] > max_socket)
                max_socket=client_socket[i];
        }
    }
    const int SELECT_TIMEOUT = 0;
    int socketCount;
    if ( (socketCount = select( max_socket + 1, &read_set, &write_set, NULL, &tv )) != SELECT_TIMEOUT) {
        if(socketCount == SOCKET_ERROR) {
            log("select function returned error: %d", SOCK_ERR);
            return false;
        }
        if(FD_ISSET(listen_socket, &read_set)) {
            ProcessIncomingConnection();
        }
        for(int sockIndex=0; sockIndex < gwOptions.max_connections; sockIndex++) {
          if(FD_ISSET(client_socket[sockIndex], &read_set)) {
              if (!ProcessIncomingData(sockIndex)) {
                  log("Error %d receiving data on connection #%d. Closing connection..." , SOCK_ERR, sockIndex);
                  CloseSocket(client_socket[sockIndex]);
                  client_socket[sockIndex] = FREE_SOCKET;
                  continue;
              }
          }
       }
    }
    return true;
}

int main(int argc, char* argv[])
{
  int cli_error;
  char error_ini_string[1024];

  if(argc<2) {
      printf("Usage: eap-sim-gw ini_file [-debug] [-emul]\n");
      exit(1);
  }

  if ((cli_error = read_initial_settings(argv[1], (char*)error_ini_string)) != 0)  {
    switch (cli_error) {
      case UNABLE_TO_OPEN_INI_FILE :
        fprintf(stderr,"%s: Unable to open ini-file %s. Exiting process.\n",program,argv[1]);
        break;

      case INI_FILE_UNRECON_OPTION :
        fprintf(stderr, "%s: Unrecognised option in ini-file : %s\n", program, error_ini_string);
        break;

      case INI_FILE_RANGE_ERR :
        fprintf(stderr, "%s: Parameter range error in ini-file : %s\n", program, error_ini_string);
        break;
      case TOO_MANY_ALLOWED_IPS :
        fprintf(stderr, "%s: Too many allowed IPs given in ini file : %s\n", program, error_ini_string);
        break;

      default :
        break;
    }

    exit(1);
  }

    if (strlen(gwOptions.service_centre) == 0)
    {
        fprintf(stderr, "%s: SERVICE_CENTRE option missing in ini-file\n", program);
        exit(1);
    }

    if(argc>2) {
        if(!strcmp(argv[2],"-emul") || !strcmp(argv[2],"-EMUL"))
            emulation_mode=1;
    }


      time_t ttToday;
      tm* tmToday;
      time(&ttToday);
      tmToday=localtime(&ttToday);

      if(strlen(gwOptions.log_path)>0) {
#ifdef WIN32
        if(gwOptions.log_path[strlen(gwOptions.log_path)-1] == '\\')
             gwOptions.log_path[strlen(gwOptions.log_path)-1] = '\0';
        sprintf(szLogName,"%s\\eapsimgw_%4.4d%2.2d%2.2d.txt",gwOptions.log_path,
              tmToday->tm_year+1900,tmToday->tm_mon+1,tmToday->tm_mday);
#else
       if(gwOptions.log_path[strlen(gwOptions.log_path)-1] == '/')
             gwOptions.log_path[strlen(gwOptions.log_path)-1]= '\0';
        sprintf(szLogName,"%s/eapsimgw_%4.4d%2.2d%2.2d.txt",gwOptions.log_path,
                tmToday->tm_year+1900,tmToday->tm_mon+1,tmToday->tm_mday);
#endif
      }
      else  {
        sprintf(szLogName,"eapsimgw_%4.4d%2.2d%2.2d.txt", tmToday->tm_year+1900,tmToday->tm_mon+1,tmToday->tm_mday);
      }
      fLog=fopen(szLogName,"a");
      if(!fLog)  {
          fprintf(stderr,"%s: Unable to open log file %s. Terminating...\n",program,szLogName);
          exit(1);
      }
#ifdef WIN32
      WSADATA wsaData;

      if(WSAStartup(MAKEWORD(2,2), &wsaData)) {
          fprintf(stderr,"%s: Error inititialing Winsock: %d. Initialization failed.\n", program,SOCK_ERR);
          exit(1);
      }
#endif

      log("  ----------------------------------------------------------------\n\n"
          "EAP-SIM Gateway start. Initialization parameters read from %s:\nMAP_VERSION %d\nEAPSIMGW_MODULE_ID 0x%x\nMAP_MODULE_ID 0x%x\n"
          "BASE_DIALOGUE 0x%04x\nNUMBER_OF_DIALOGUES %d\nSERVICE_CENTRE %s\nLOG_PATH %s\nMAP_TIMEOUT %d seconds\n"
          "IP_PORT %d\nMAX_CONNECTIONS %d\nLOG_OPTIONS 0x%x\n",
          argv[1],gwOptions.map_version,gwOptions.mtu_mod_id,gwOptions.mtu_map_id,gwOptions.base_dlg_id,gwOptions.num_dlg_ids,
          gwOptions.service_centre,gwOptions.log_path,gwOptions.MAP_timeout,gwOptions.IP_port,gwOptions.max_connections,gwOptions.log_options
          );

      /* First call to socket() function */
      listen_socket = socket(AF_INET, SOCK_STREAM, 0);
      if (listen_socket < 0)
      {
          fprintf(stderr, "%s: Unable to create socket AF_INET,SOCK_STREAM.\n",program);
          exit(1);
      }
      /* Initialize socket structure */
      struct sockaddr_in serv_addr;
      memset((char *) &serv_addr, 0, sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      serv_addr.sin_port = htons(gwOptions.IP_port);

      /* Now bind the host address using bind() call.*/
      if (bind(listen_socket, (struct sockaddr *) &serv_addr,
                            sizeof(serv_addr)) < 0)
      {
           fprintf(stderr, "%s: Failed to call bind on socket.\n",program);
           exit(1);
      }

      listen(listen_socket, gwOptions.max_connections);
      //clilen = sizeof(sockaddr_in);

      client_socket=(int*) malloc(100/*sizeof(int) * gwOptions.max_connections*/);
      memset(client_socket, 0, sizeof(int) * gwOptions.max_connections);
      client_addr=(sockaddr_in*)malloc(sizeof(sockaddr_in) * gwOptions.max_connections);
      time_t tLastQueueCheckTime;
      time(&tLastQueueCheckTime); // initialise last queue check time

      // Main program loop
      while(true) {
        ProcessPendingSS7Messages();
        DiscardHangingRequestsSchedule();
        if (!bShutdownInProgress) {
            bShutdownInProgress = IsShutdownSignalSet();
        }
        if(bShutdownInProgress && ss7RequestMap.size() == 0) {
            // all requests are processed, close connections
            CloseSockets();
            log("Gateway shutdown.");
            fclose(fLog);
            exit(0);
        }
        ProcessSocketEvents();
    }
}


