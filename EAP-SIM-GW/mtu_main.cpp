/*
                Copyright (C) Dialogic Corporation 1997-2009. All Rights Reserved.

 Name:          mtu_main.c

 Description:   Console command line interface to mtu.

 Functions:     main()


 */

#include "mtu.h"
#include "CoACommon.h"
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
#define DEFAULT_TRIPLETS_NUM    (3)

#ifdef WIN32
    #define SOCK_ERR WSAGetLastError()
    #define sleep(x) Sleep(x)
#else
    #define SOCK_ERR errno
#endif

/*
 *
 * Structure that stores the data entered on the command line
 */
GW_OPTIONS gwOptions;
GTT_ENTRY gttTable[MAX_GTT_TABLE_SIZE];
//SS7_REQUEST ss7Request[NUM_OF_DLG_IDS];

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

int debug_mode=0;
int preset_mode=0;
bool bShutdownInProgress=false;

std::multimap<__uint16_t,SPSReqAttr*> mmRequest;

char socket_recv_buffer[65536];
char socket_send_buffer[65536];

CPSPacket pspRequest;
CPSPacket pspResponse;



extern int MTU_smac(MSG *m);
extern int MTU_open_dlg(u16 dlg_id,char* imsi);
extern int MTU_display_recvd_msg(MSG *m);
extern int MTU_send_User_Abort(u16 dlg_id,u16 invoke_id);

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
//#else
//    timeval curTime;
//    gettimeofday(&curTime, NULL);
//    strftime(timBuf,19,"%H:%M:%S",localtime(&curTime.tv_sec));
//    if(fLog) {
//        sprintf(szCurrentLogName,"%s/eapsimgw_%4.4d%2.2d%2.2d.txt",gwOptions.log_path,
//                tmToday->tm_year+1900,tmToday->tm_mon+1,tmToday->tm_mday);
//        fprintf(fLog, "%s.%d %s\n",timBuf, curTime.tv_usec / 1000,szBuffer);
//        fflush(fLog);
//    }
//#endif
    }
    catch(...) {
        fprintf(stderr,"EAP-SIM-GW: Exception caught in log function\n");
    }
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
//      if(p==(char*)option_value) {
//          sprintf(addinfo,"IMSI mask cannot start from delimiter");
//          return(INI_FILE_RANGE_ERR);
//      }
//      else {
//          p=strchr(p+1,'/');
//          if(p) {
//              sprintf(addinfo,"Only one delimiter is allowed at IMSI mask");
//              return(INI_FILE_RANGE_ERR);
//          }
//      }
//      p=strrchr(option_value,'/');
//      if(p==(char*)option_value[strlen(option_value-1)]) {
//          sprintf(addinfo,"IMSI mask cannot end with delimiter");
//          return(INI_FILE_RANGE_ERR);
//      }


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
int read_initial_settings(char* ini_file_name,char* error_str)
{
  int error;
  char inistr[1024];
  char* p;
  char addinfo[1024];

  try {
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
        strcpy(error_str,p);
        if(strlen(addinfo))
            strcat(error_str,addinfo);
        return(error);
    }
  }
  fclose(fSettings);
  return(0);
  }
  catch(...) {
      log("Exception caught in read_initial_settings");
      return -999;
  }
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


int Create_Request(int PS_req_num,char* imsi,int triplets_num,int socket_index)
{
    SS7_REQUEST ss7Request;
    u16 dlg_id=gwOptions.base_dlg_id;

    try {
    std::map<u16,SS7_REQUEST>::iterator iter=ss7RequestMap.begin();
    while(iter != ss7RequestMap.end()) {
        if(iter->first > dlg_id)
            break;
        iter++;
        dlg_id++;
        if(dlg_id >= gwOptions.base_dlg_id + gwOptions.num_dlg_ids) {
                // overrun of open dialogues
                return -1;
        }
    }

    ss7Request.PS_request_num=PS_req_num;
    strcpy(ss7Request.imsi,imsi);
    ss7Request.triplets_num_req=triplets_num;
    ss7Request.triplets_num_recv=0;
    ss7Request.socket_index=socket_index;
    ss7Request.dlg_id=dlg_id;
    ss7Request.invoke_id= 1;        // Use the same invoke_id

    ss7Request.rand[0][0]=0;
    ss7Request.rand[1][0]=0;
    ss7Request.rand[2][0]=0;
    ss7Request.rand[3][0]=0;
    ss7Request.rand[4][0]=0;
    ss7Request.kc[0][0]=0;
    ss7Request.kc[1][0]=0;
    ss7Request.kc[2][0]=0;
    ss7Request.kc[3][0]=0;
    ss7Request.kc[4][0]=0;
    ss7Request.sres[0][0]=0;
    ss7Request.sres[1][0]=0;
    ss7Request.sres[2][0]=0;
    ss7Request.sres[3][0]=0;
    ss7Request.sres[4][0]=0;
    ss7Request.successful=false;

    time(&ss7Request.state_change_time);

    ss7RequestMap.insert( std::pair<u16,SS7_REQUEST>(dlg_id,ss7Request) );

//    printf("\n----- New request allocated. IMSI=%s, triplets requested=%d. SS7 dialogue ID=0x%0000x. Number of active requests=%d ----------\n",
//           imsi,triplets_num,dlg_id,ss7RequestMap.size());
    return dlg_id;
    }
    catch(...) {
        log("Exception caught in Create_Request");
        return -999;
    }
}

void Send_Response_to_Client(int dlg_id)
{
    char address_buffer[64];

    try {
    SS7_REQUEST& ss7Req=ss7RequestMap.at(dlg_id);

    if(pspResponse.Init((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),ss7Req.PS_request_num,RS_TRIP_REQ)) {
        // error - buffer too small
        log("Send_Response_to_Client: initializing packet failed, buffer too small" );
        fprintf(stderr, "%s: Send_Response_to_Client: initializing packet failed, buffer too small\n",program );
        return;
    }

    pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),SS7GW_IMSI,ss7Req.imsi,strlen(ss7Req.imsi));

    if(ss7Req.successful) {
        pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),PS_RESULT,"00",2);

        if(ss7Req.triplets_num_recv>0) {
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_RAND1,ss7Req.rand[0],strlen(ss7Req.rand[0]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_KC1,ss7Req.kc[0],strlen(ss7Req.kc[0]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_SRES1,ss7Req.sres[0],strlen(ss7Req.sres[0]));
        }
        if(ss7Req.triplets_num_recv>1) {
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_RAND2,ss7Req.rand[1],strlen(ss7Req.rand[1]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_KC2,ss7Req.kc[1],strlen(ss7Req.kc[1]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_SRES2,ss7Req.sres[1],strlen(ss7Req.sres[1]));
        }
        if(ss7Req.triplets_num_recv>2) {
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_RAND3,ss7Req.rand[2],strlen(ss7Req.rand[2]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_KC3,ss7Req.kc[2],strlen(ss7Req.kc[2]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_SRES3,ss7Req.sres[2],strlen(ss7Req.sres[2]));
        }
        if(ss7Req.triplets_num_recv>3) {
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_RAND4,ss7Req.rand[3],strlen(ss7Req.rand[3]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_KC4,ss7Req.kc[3],strlen(ss7Req.kc[3]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_SRES4,ss7Req.sres[3],strlen(ss7Req.sres[3]));
        }
        if(ss7Req.triplets_num_recv>4) {
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_RAND5,ss7Req.rand[4],strlen(ss7Req.rand[4]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_KC5,ss7Req.kc[4],strlen(ss7Req.kc[4]));
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),RS_SRES5,ss7Req.sres[4],strlen(ss7Req.sres[4]));
        }
        log(">>> Request #%d SUCCESS. Dialogue_id 0x%04x, IMSI %s, triplets requested %d, triplets received %d. Sending %d bytes to %s.(connection #%d).",
            ss7Req.PS_request_num,dlg_id,ss7Req.imsi,ss7Req.triplets_num_req,ss7Req.triplets_num_recv,ntohs(((SPSRequest*)socket_send_buffer)->m_usPackLen),
            IPAddr2Text(&client_addr[ss7Req.socket_index].sin_addr,address_buffer,sizeof(address_buffer)), ss7Req.socket_index);
    }
    else {
        pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),PS_RESULT,"01",2);
        pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),PS_DESCR,ss7Req.error.c_str(),ss7Req.error.length());
        log(">>> Request #%d FAILURE: %s.",ss7Req.PS_request_num,ss7Req.error.c_str());
        log(">>> Dialogue_id 0x%04x, IMSI %s. Sending %d bytes to %s (connection #%d).",
            dlg_id,ss7Req.imsi,ntohs(((SPSRequest*)socket_send_buffer)->m_usPackLen),
            IPAddr2Text(&client_addr[ss7Req.socket_index].sin_addr,address_buffer,sizeof(address_buffer)),ss7Req.socket_index);
    }

    if(!debug_mode) {
        if(!send(client_socket[ss7Req.socket_index],socket_send_buffer,ntohs(((SPSRequest*)socket_send_buffer)->m_usPackLen),0)) {
            // sending response failed
        log("sending response failed: %d",SOCK_ERR);
        fprintf(stderr, "%s: sending response failed: %d\n",program,SOCK_ERR);
        }
    }
    }
    catch(...) {
        log("Exception caught in Send_Response_to_Client");
    }
}

/* This function is called from MTU_smac when close indication arrives and dialoguse is finished */
void OnDialogueFinish(u16 dlg_id)
{
    try {
    SS7_REQUEST& ss7Req=ss7RequestMap.at(dlg_id);
    int req_num=ss7Req.PS_request_num;

    if(ss7Req.successful && ss7Req.triplets_num_recv<ss7Req.triplets_num_req) {
        // some triplets are received, but we need more of them. Initiate new SS7 requests
        log("Request #%d: Requested %d triplets but received %d only. Requesting for extra triplets...",
            ss7Req.PS_request_num,ss7Req.triplets_num_req,ss7Req.triplets_num_recv);
        if (MTU_open_dlg(dlg_id,ss7Req.imsi) != 0) {
            // sending new SS7 request failed
              ss7Req.state = rs_finished;
              Send_Response_to_Client(dlg_id);
              ss7RequestMap.erase(dlg_id);
        }
        ss7RequestMap.at(dlg_id).state = rs_wait_opn_cnf;
        time(&ss7RequestMap.at(dlg_id).state_change_time);
    }
    else {
        Send_Response_to_Client(dlg_id);
        ss7RequestMap.erase(dlg_id);
        log("Processing request #%d finished, dialogue_id 0x%04x closed. Queue size=%d",req_num,dlg_id,ss7RequestMap.size());
     }
    }
    catch(...) {
        log("Exception caught in OnDialogueFinish");
    }
}


/* Function consistently processes requests from socket_recv_buffer.
 * buffer_shift - starting point of next request in buffer.
 * Return value is the length of processed request.
 */
int ProcessRequest(int conn_index,int buffer_shift) {
    char szError[2048];
    u32  temp_u32;
    char attr_value[256];
    char imsi[20];
    int nDialogueID;
    bool bRequestAccept=false;
    int triplets_num=0;
    __uint32_t ui32ReqNum;
    __uint16_t ui16ReqType;
    __uint16_t ui16PackLen;

    try {
    imsi[0]=0;
    //SPSRequest *pspRequest=(SPSRequest *)socket_recv_buffer;

    mmRequest.clear();
    pspRequest.Parse((SPSRequest *)(socket_recv_buffer+buffer_shift), sizeof(socket_recv_buffer), ui32ReqNum,ui16ReqType,ui16PackLen,mmRequest, 1);
    if(ui16ReqType==SS7GW_IMSI_REQ) {
      // Request for IMSI triplets
      if(!bShutdownInProgress)
        do {
          std::multimap<__uint16_t,SPSReqAttr*>::iterator iter=mmRequest.find(SS7GW_IMSI);
          if(iter==mmRequest.end()) {
              sprintf(szError,"IMSI is not given in triplets request");
              break;
          }
          strncpy(imsi,reinterpret_cast<char*>(iter->second)+4,15);
          imsi[ntohs(iter->second->m_usAttrLen)-4]=0;
          if(ntohs(iter->second->m_usAttrLen)-4 != 15) {
              sprintf(szError,"Wrong IMSI=%s given. IMSI must be 15 symbols long.",imsi);
              break;
          }

          iter=mmRequest.find(SS7GW_TRIP_NUM);
          if(iter!=mmRequest.end()) {
              strncpy(attr_value,reinterpret_cast<char*>(iter->second)+4,ntohs(iter->second->m_usAttrLen)-4 );
              attr_value[ntohs(iter->second->m_usAttrLen)-4]=0;
              if (strtou32(&temp_u32, attr_value) != 0) {
                  sprintf(szError,"Wrong number of triplets given (%s)",attr_value);
                  break;
              }
              triplets_num=(u16)temp_u32;
              if(triplets_num<1 ||triplets_num>5) {
                sprintf(szError,"Number of triplets must be from 1 to 5. Given value=%d",triplets_num);
                break;
              }

          }
          else {
              triplets_num=DEFAULT_TRIPLETS_NUM;  // by default request AuC for 3 triplets
          }

          if((nDialogueID=Create_Request(ui32ReqNum,imsi,triplets_num,conn_index))<0) {
              sprintf(szError, "All SS7 dialogues are busy, request rejected. Queue size=%d",ss7RequestMap.size());
              break;
          }
          log("Request #%d, dialogue_id 0x%04x allocated. IMSI %s, triplets requested %d. Queue size=%d",
                 ui32ReqNum,nDialogueID,imsi,triplets_num,ss7RequestMap.size());

          bRequestAccept=true;
         }
         while(0);
       else {
         // Shutdown in progress, no new requests accepted
         bRequestAccept=false;
         strcpy(szError,"Shutdown in progress. Unable to accept new requests.");
       }

        // send response to client
        if(pspResponse.Init((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),ui32ReqNum,SS7GW_IMSI_RESP)) {
            // error - buffer too small
            log("Initializing response buffer failed, buffer too small" );
            fprintf(stderr, "%s: Initializing response buffer failed, buffer too small\n",program );
            return ui16PackLen;
        }
        if(bRequestAccept) {
            // request accepted, send confirmation to client
            log("Request #%d for IMSI %s and triplet num %d accepted.",ui32ReqNum,imsi,triplets_num);
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),PS_RESULT,"00",2);
        }
        else {
            // error in request parameters
            log("Request #%d for IMSI %s and triplet num %d rejected with reason %s",ui32ReqNum,imsi,triplets_num,szError);
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),PS_RESULT,"01",2);
            pspResponse.AddAttr((SPSRequest*)socket_send_buffer,sizeof(socket_send_buffer),PS_DESCR,szError,strlen(szError));
        }

        if(!send(client_socket[conn_index],socket_send_buffer,ntohs(((SPSRequest*)socket_send_buffer)->m_usPackLen),0)) {
            // sending response failed
          log("Sending request accept confirmation to client failed: %d",SOCK_ERR );
        }

        if(!bRequestAccept)
            return ui16PackLen;

        // request accepted
        if(!preset_mode) {
            if (MTU_open_dlg(nDialogueID,imsi) != 0) {
              ss7RequestMap.at(nDialogueID).state = rs_finished;
              Send_Response_to_Client(nDialogueID);
              ss7RequestMap.erase(nDialogueID);
              return ui16PackLen;
            }
            ss7RequestMap.at(nDialogueID).state = rs_wait_opn_cnf;
            time(&ss7RequestMap.at(nDialogueID).state_change_time);
        }
        else {
            // preset mode - used when SS7 link is down
            strcpy(ss7RequestMap.at(nDialogueID).rand[0],"e6486dcc5ebc94e3c989a97bd0909f2a");
            strcpy(ss7RequestMap.at(nDialogueID).rand[1],"1b5366727f83d8029db95b0eb2a22f02");
            strcpy(ss7RequestMap.at(nDialogueID).rand[2],"e9e41353b37341ee63f38b7c5a41efef");
            strcpy(ss7RequestMap.at(nDialogueID).sres[0],"e61752eb");
            strcpy(ss7RequestMap.at(nDialogueID).sres[1],"58ecd4a2");
            strcpy(ss7RequestMap.at(nDialogueID).sres[2],"f6dbfa9b");
            strcpy(ss7RequestMap.at(nDialogueID).kc[0],"bc1d4d2b909c1800");
            strcpy(ss7RequestMap.at(nDialogueID).kc[1],"2562db2c4af9f000");
            strcpy(ss7RequestMap.at(nDialogueID).kc[2],"c64cc11999b4b000");

            log("Preset mode: triplets filled out with constant values");
            ss7RequestMap.at(nDialogueID).successful = true;
            ss7RequestMap.at(nDialogueID).triplets_num_recv=ss7RequestMap.at(nDialogueID).triplets_num_req;
            ss7RequestMap.at(nDialogueID).state = rs_finished;
            OnDialogueFinish(nDialogueID);
        }
    }
    return ui16PackLen;
    }
    catch(...) {
        log("Exception caught in ProcessRequest");
        return -999;
    }
}


// Function CheckRequestQueue checks request queue for hanging dialogues and releases it
void CheckRequestQueue(time_t tNow)
{
    bool bTimeoutReqFound=false;

    try {
    std::map<u16,SS7_REQUEST>::iterator iter = ss7RequestMap.begin();
    while(iter != ss7RequestMap.end()) {
        if(tNow - iter->second.state_change_time > gwOptions.MAP_timeout ) {
            MTU_send_User_Abort(iter->second.dlg_id,iter->second.invoke_id);
            iter->second.error="MAP time-out";
            OnDialogueFinish(iter++ -> second.dlg_id); // increment iter here, 'cause OnDialogueFinish call map.erase, after what iterator becomes invalid
            bTimeoutReqFound=true;
         }
         else
            iter++;

    }
    if(bTimeoutReqFound)
        log("Queue size after checking request queue=%d",ss7RequestMap.size());
    }
    catch(...) {
        log("Exception caught in CheckRequestQueue");
    }
}

/*
 * Main function for EAP-SIM Gateway
 */
int main(int argc, char* argv[])
{
  int cli_error;
  char error_ini_string[1024];
  int listen_socket, newsockfd;
#ifdef WIN32
  int clilen;
#else
  socklen_t clilen;
#endif
  struct sockaddr_in serv_addr;
  struct sockaddr_in newclient_addr;
  char address_buffer[64];
  fd_set read_set, write_set;
  struct timeval tv;
  int  i,res,max_socket;
  u32  temp_u32;
  char imsi[20];
  int triplets_num;
  int nDialogueID;
  int nReceived;
  int nBytesProcessed;
  int nRequestLen;
  time_t tQueueCheckTime;
  time_t tNow;
  char dir_slash;
  char szStopFilename[2048];
  FILE* fStopFile;
  bool bConnectionAllowed;

  HDR *h;               /* header of received message */
  MSG *m;               /* received message */

  gwOptions.mtu_mod_id = DEFAULT_MODULE_ID;
  gwOptions.mtu_map_id = DEFAULT_MAP_ID;
  gwOptions.map_version = MTU_MAPV3; //MTU_MAPV2;
  gwOptions.mode = MTU_SEND_AUTH_INFO;
  gwOptions.log_options = DEFAULT_OPTIONS;
  gwOptions.base_dlg_id = BASE_DLG_ID;
  gwOptions.num_dlg_ids = NUM_OF_DLG_IDS;
  gwOptions.max_active = DEFAULT_MAX_ACTIVE;
  //cl_args.orig_address[0] = 0;
  gwOptions.service_centre[0] = 0;
  gwOptions.log_path[0] = 0;
  gwOptions.gtt_table_size = 0;
  gwOptions.MAP_timeout=5;
  gwOptions.IP_port=5100;
  gwOptions.allowed_clients_num=0;
  gwOptions.max_connections=10;
  gwOptions.queue_check_period=10;


  if(argc<2) {
      printf("Usage: eap-sim-gw ini_file\n");
      exit(1);
  }
  if ((cli_error = read_initial_settings(argv[1],(char*)error_ini_string)) != 0)
  {
    switch (cli_error)
    {
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

  /*
   * Check the ini file arguments
   */
  if (strlen(gwOptions.service_centre) == 0)
  {
    fprintf(stderr, "%s: SERVICE_CENTRE option missing in ini-file\n", program);
    exit(1);
  }

  if(argc>2) {
    if(!strcmp(argv[2],"-debug") || !strcmp(argv[2],"-DEBUG"))
        debug_mode=1;

    if(!strcmp(argv[2],"-preset") || !strcmp(argv[2],"-PRESET"))
        preset_mode=1;
  }

  // construct name of stop-file. It must reside in same directory as ini-file
#ifdef WIN32
  dir_slash='\\' ;
#else
  dir_slash='/';
#endif
  char* ds_pos=strrchr(argv[1],dir_slash);    // search for last occurence of slash in ini-file name
  if(ds_pos) {
      strncpy(szStopFilename,argv[1],ds_pos-argv[1]+1);
      szStopFilename[ds_pos-argv[1]+1]=0;
      strcat(szStopFilename,"EAP-SIM-GW.stop");
  }
  else
      strcpy(szStopFilename,"EAP-SIM-GW.stop");


  if(!debug_mode) {


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

      listen(listen_socket,gwOptions.max_connections);
      clilen = sizeof(sockaddr_in);

      client_socket=(int*) malloc(100/*sizeof(int) * gwOptions.max_connections*/);
      memset(client_socket, 0, sizeof(int) * gwOptions.max_connections);
      client_addr=(sockaddr_in*)malloc(sizeof(sockaddr_in) * gwOptions.max_connections);
      time(&tQueueCheckTime); // initialise last queue check time

      // Main program loop
      while(true) {
          try {
          while ((h = GCT_grab(gwOptions.mtu_mod_id)) != 0)
          {
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

          try {
          // No more SS7 messages to process - check our dialogue queue
          time(&tNow);
          if(tNow - tQueueCheckTime > gwOptions.queue_check_period) {
              // once in 30 seconds check requests queue for hanging dialogues
              CheckRequestQueue(tNow);
              time(&tQueueCheckTime);
              if(!bShutdownInProgress) {
                if((fStopFile=fopen(szStopFilename,"r"))) {
                  bShutdownInProgress=true;
                  log("Stop file %s found. Shutting gateway down ...",szStopFilename);
                  fclose(fStopFile);
                }
              }
              if(bShutdownInProgress && ss7RequestMap.size() == 0) {
                // all requests are processed, close connections
                for(i=0; i<gwOptions.max_connections; i++) {
                    if(client_socket[i] != 0) {
                      shutdown(newsockfd,2);
                    #ifdef WIN32
                      closesocket(newsockfd);
                    #else
                      close(newsockfd);
                    #endif
                    }
                }
                free(client_socket);
                free(client_addr);
                // close listening socket
              #ifdef WIN32
                closesocket(listen_socket);
              #else
                close(listen_socket);
              #endif

                ss7RequestMap.clear();

                // delete stop file
                remove(szStopFilename);
                log("Gateway shutdown.");
                fclose(fLog);
                exit(0);
              }
          }
          }
          catch(...) {
              log("Exception caught in main loop: checking queue");
          }


          // Check new client connection and requests

          try {
          tv.tv_sec = 0;  // time-out
          tv.tv_usec = 100;
          FD_ZERO( &read_set );
          FD_ZERO( &write_set );
          FD_SET( listen_socket, &read_set );
          max_socket=listen_socket;
          for(i=0; i<gwOptions.max_connections; i++) {
              if(client_socket[i] != 0) {
                  FD_SET( client_socket[i], &read_set );
                  if(client_socket[i] > max_socket)
                      max_socket=client_socket[i];
              }
          }

          if ( (res=select( max_socket + 1, &read_set, &write_set, NULL, &tv )) !=0  )
          {
              if(res==-1) {
                  fprintf(stderr, "%s: select returned error: %d\n" ,program,SOCK_ERR);
                  sleep(10); // not to overflow log files and stderr
                  continue;
              }

              if(FD_ISSET(listen_socket,&read_set)) {
                // check for free connections


                newsockfd = accept(listen_socket, (struct sockaddr *)&newclient_addr,&clilen);
                if (newsockfd < 0)
                {
                   log("Failed to call accept on socket: %d",SOCK_ERR);
                   fprintf(stderr, "%s: Failed to call accept on socket: %d\n",program,SOCK_ERR);
                   continue;
                }

                log("Incoming connection from %s",IPAddr2Text(&newclient_addr.sin_addr,address_buffer,sizeof(address_buffer)));

                for(i=0; i<gwOptions.max_connections; i++) {
                    if(client_socket[i]==0)
                        break;
                }
                if (i>=gwOptions.max_connections || bShutdownInProgress) {
                    // no new connections available
                    if(i>=gwOptions.max_connections)
                        log("Maximum allowed connections reached. Rejecting connection...");
                    else
                        log("Shutdown in progress. Rejecting connection...");
                    shutdown(newsockfd,2);
                    #ifdef WIN32
                      closesocket(newsockfd);
                    #else
                      close(newsockfd);
                    #endif
                    continue;
                }
                if(gwOptions.allowed_clients_num > 0) {
                    bConnectionAllowed=false;
                    // check connection for white list of allowed clients
                    for(int j=0; j<gwOptions.allowed_clients_num; j++)
                        if(!strcmp(gwOptions.allowed_IP[j],address_buffer)) {
                            bConnectionAllowed=true;
                            break;
                        }
                    if(!bConnectionAllowed) {
                        log("Client address %s not found in white list of allowed IPs. Rejecting connection...",address_buffer);
                        shutdown(newsockfd,2);
                        #ifdef WIN32
                          closesocket(newsockfd);
                        #else
                          close(newsockfd);
                        #endif
                        continue;
                    }
                }


                client_socket[i]=newsockfd;
                memcpy(&client_addr[i],&newclient_addr,sizeof(newclient_addr));

                log("Connection #%d accepted.",i);
                continue;

              }

              for(i=0;i<gwOptions.max_connections;i++) {
                if(FD_ISSET(client_socket[i],&read_set)) {

                  nReceived = recv(client_socket[i], socket_recv_buffer, sizeof(socket_recv_buffer), 0);
                  if(nReceived <= 0) {
                      log("Error %d receiving data on connection #%d. Closing connection..." ,SOCK_ERR,i);
                      // set socket descriptor to 0
                      shutdown(client_socket[i],2);
                      #ifdef WIN32
                        closesocket(newsockfd);
                      #else
                        close(newsockfd);
                      #endif
                      client_socket[i]=0;
                      break;
                  }
//                  log("%d bytes received from %d.%d.%d.%d (connection #%d). Processing request(s) ...",nReceived,
//                         client_addr[i].sin_addr.S_un.S_un_b.s_b1,client_addr[i].sin_addr.S_un.S_un_b.s_b2,
//                         client_addr[i].sin_addr.S_un.S_un_b.s_b3,client_addr[i].sin_addr.S_un.S_un_b.s_b4,i);
                  log("%d bytes received from %s (connection #%d). Processing request(s) ...",nReceived,
                      IPAddr2Text(&client_addr[i].sin_addr,address_buffer,sizeof(address_buffer)),i);
                  nBytesProcessed = 0;
                  while(nBytesProcessed<nReceived) {
                    nRequestLen = ProcessRequest(i,nBytesProcessed);
                    if(nRequestLen >= 0)
                        nBytesProcessed += nRequestLen;
                    else
                        break;
                  }
                }
             }

          }
          }
          catch(...) {
              log("Exception caught in main loop: processing socket events");
              return -999;
          }
    }

//#ifdef WIN32
//      closesocket( newsockfd );
//      closesocket( listen_socket );
//      WSACleanup();
//#else
//      close(sock);
//#endif

}
  else {
    // debug mode - no creating sockets, just get triplets for IMSI given in command line
    if(argc>2) {
        strcpy(imsi,argv[2]);
        if(argc>3) {
            if (strtou32(&temp_u32, argv[3]) != 0) {
                // error
                fprintf(stderr,"Wrong attribute value for number of triplets given");
                exit(1);
            }
            if(temp_u32<1 || temp_u32>5) {
                fprintf(stderr,"Number of triplets must be from 1 to 5. Given value=%ld",temp_u32);
                exit(1);
            }
            triplets_num=(u16)temp_u32;
        }
        else
            triplets_num=DEFAULT_TRIPLETS_NUM;

        if((nDialogueID=Create_Request(1,imsi,triplets_num,-1))==-1) {
            fprintf(stderr, "%s: Create_Request failed, all dialogues are busy",program);
            // send report to Policy Server
            // .....
        }

        if ((res=MTU_open_dlg(nDialogueID,imsi)) != 0) {
            if(res==-1) {
                ss7RequestMap.at(nDialogueID).state = rs_finished;
                Send_Response_to_Client(nDialogueID);
                ss7RequestMap.erase(nDialogueID);
            }

        }

        ss7RequestMap.at(nDialogueID).state = rs_wait_opn_cnf;
        time(&ss7RequestMap.at(nDialogueID).state_change_time);

        HDR *h;               /* received message */
        MSG *m;               /* received message */

        while (true)
        {
          if ((h = GCT_receive(gwOptions.mtu_mod_id)) != 0)
          {
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
                //MTU_disp_err("Unexpected message type");
                break;
            }
            /*
             * Once we have finished processing the message
             * it must be released to the pool of messages.
             */
            relm(h);
          }
          if(ss7RequestMap.find(nDialogueID) == ss7RequestMap.end())
              // processing finished
              break;

        } /* end while */

        return 0;
    }
  }

  return(0);
}


/*
 *        show_syntax()
 */