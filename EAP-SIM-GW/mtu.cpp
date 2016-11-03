/*
                Copyright (C) Dialogic Corporation 1997-2011. All Rights Reserved.

 Name:          mtu.c

 Description:   Example application program for the MAP user interface.

                The application opens a dialogue, sends a service request,
                and handles the response in a manner designed to exercise
                the MAP functionality and illustrate the programming
                techniques involved.

 Functions:     main

 -----  ---------  ---------------------------------------------
 Issue    Date                       Changes
 -----  ---------  ---------------------------------------------

   A    23-Sep-97   - Initial code.
   B    19-Jan-98   - Prevents IMSI and SMSC address nibbles being swapped.
   C    20-Mar-98   - Added Version 1 functionality.
   D    21-Aug-98   - Temporary fix added so that correct SM RP UI
                      contents may be sent.
   E    11-Nov-98   - Added missing function declarations.
   F    13-Jan-99   - Included additional required parameters in the
                      the MAP-FORWARD-SHORT-MESSAGE-Req SMS-DELIVER.
                    - 'finished' variable made static.
   G    16-Feb-00   - Changed response to receipt of provider errors.
                      Now ignores all but provider error == no response,
                      to which it replies with MAP_CLOSE.  **NOTE** this
                      is different to the 09.02 SDLs.
   1    16-Jul-01   - Removed call to GCT_grab().
   2    10-Aug-01   - Added support for Send-IMSI and Send routing info
                      for GPRS. Also ability to generate multiple
                      dialogues.
   3    19-Dec-01   - Change to ensure dialogue is aborted if no reply
                      to a service invoke is received.
   4    20-Jan-06   - Include reference to Intel Corporation in file header
   5    21-Aug-06   - Operation timeout in MTU_send_imsi() reduced to 15
                      seconds from 60.
   6    13-Dec-06   - Change to use of Dialogic Corporation copyright.   
   7    30-Sep-09   - Support for additional MAP services, e.g. USSD.
   8    02-Dec-09   - Minor fixes to dialogue id's.
   9    05-Apr-11   - Corrected setting of messages to send in SMS tx.
                    - Updated setting of TP-OA parameter.
 */

#define LINT_ARGS
#include "mtu.h"
#include "authinfo.h"




/*
 * Prototypes for local functions:
 */
/*static u16 MTU_alloc_dlg_id(u16 *dlg_id);
static MTU_DLG *MTU_get_dlg_data(u16 dlg_id);
static u8 MTU_alloc_invoke_id(void);
static int MTU_display_msg(char *prefix, MSG *m);
static int MTU_display_recvd_msg(MSG *m);
static int MTU_display_sent_msg(MSG *m);
static int MTU_forward_sm(u16 dlg_id, u8 invoke_id);
static int MTU_MT_forward_sm(u16 dlg_id, u8 invoke_id);
static int MTU_process_uss_req (u16 dlg_id, u8 invoke_id);
static int MTU_open_dlg(u8 service, MTU_BCDSTR *imsi);
static int MTU_other_message(MSG *m);
static void MTU_release_dlg_id(u16 dlg_id);
static void MTU_release_invoke_id(u8 invoke_id, MTU_DLG *dlg);
static int MTU_send_dlg_req(MTU_MSG *dlg_req);
static int MTU_send_imsi(u16 dlg_id, u8 invoke_id);
static int MTU_send_msg(MSG *m);
static int MTU_sri_gprs(u16 dlg_id, u8 invoke_id);
static int MTU_sri_sm(u16 dlg_id, u8 invoke_id);
static int MTU_send_auth_info(u16 dlg_id, u8 invoke_id);
static int MTU_send_srv_req(MTU_MSG *srv_req);
static int MTU_smac(MSG *m);
static int MTU_wait_open_cnf(MTU_MSG *ind, MTU_DLG *dlg);
static int MTU_wait_serv_cnf(MTU_MSG *ind, MTU_DLG *dlg);
static int MTU_wait_close_ind(MTU_MSG *ind, MTU_DLG *dlg);
static int MTU_Send_UnstructuredSSRequestRSP (u16 dlg_id, u8 invoke_id);*/

/*
 * Application context for shortMsgRelayContext-v1
 */
static u8 shortMsgRelayContextv1[AC_LEN] =
{
  06,           /* object identifier */
  07,           /* length */
  04,           /* CCITT */
  00,           /* ETSI */
  00,           /* Mobile domain */
  01,           /* GSM network */
  00,           /* application contexts */
  21,           /* short msg relay */
  01            /* version 1 */
};

/*
 * Application context for shortMsgRelayContext-v2
 */
static u8 shortMsgRelayContextv2[AC_LEN] =
{
  06,           /* object identifier */
  07,           /* length */
  04,           /* CCITT */
  00,           /* ETSI */
  00,           /* Mobile domain */
  01,           /* GSM network */
  00,           /* application contexts */
  25,           /* short msg relay */
  02            /* version 2 */
};

/*
 * Application context for imsiRetrievalContext-v2
 */
static u8 imsiRetrievalContextv2[AC_LEN] =
{
  06,           /* object identifier */
  07,           /* length */
  04,           /* CCITT */
  00,           /* ETSI */
  00,           /* Mobile domain */
  01,           /* GSM network */
  00,           /* application contexts */
  26,           /* imsi retrieval */
  02            /* version 2 */
};

/*
 * Application context for gprsLocationInfoRetrievalContext-v3
 */
static u8 gprsLocationInfoRetrievalContextv3[AC_LEN] =
{
  06,           /* object identifier */
  07,           /* length */
  04,           /* CCITT */
  00,           /* ETSI */
  00,           /* Mobile domain */
  01,           /* GSM network */
  00,           /* application contexts */
  33,           /* gprs location info retrieval */
  03            /* version 3 */
};

/* new application context shortMsgMT-RelayContext */
static u8 shortMsgMTRelayContext[AC_LEN] =
{
  06,           /* object identifier */
  07,           /* length */
  04,           /* CCITT */
  00,           /* ETSI */
  00,           /* Mobile domain */
  01,           /* GSM network */
  00,           /* application contexts */
  25,           /* map-ac shortMsgMT-Relay */
  03            /* version 3 */
};

/* new application context shortMsgMT-GatewayContext */
static u8 shortMsgGatewayContext[AC_LEN] =
{
  06,           /* object identifier */
  07,           /* length */
  04,           /* CCITT */
  00,           /* ETSI */
  00,           /* Mobile domain */
  01,           /* GSM network */
  00,           /* application contexts */
  20,           /* map-ac shortMsgGateway */
  03            /* version 3 */
};

/* new application context networkUnstructuredSsContext-v2 */
static u8 networkUnstructuredSsContext[AC_LEN] =
{
  06,           /* object identifier */
  07,           /* length */
  04,           /* CCITT */
  00,           /* ETSI */
  00,           /* Mobile domain */
  01,           /* GSM network */
  00,           /* application contexts */
  19,           /* map-ac networkUnstructuredSs */
  02            /* version 2 */
};

static u8 sendAuthInfoContext[AC_LEN] =
{
  06,           /* object identifier */
  07,           /* length */
  04,           /* CCITT */
  00,           /* ETSI */
  00,           /* Mobile domain */
  01,           /* GSM network */
  00,           /* application contexts */
  14,           /* map-ac infoRetrieval */
  03            /* MAP version*/
};

/*
 * Default Service Centre address. Note that if there are an odd number of
 * digits, then f should be added to the end of the digits as a filler.
 */
MTU_OASTR DEFAULT_SC_ADDR =
{
  4,                    /* type of address = service centre address */
  5,                    /* number of bytes of digits in the string */
  2,                    /* nature of address = national significant number */
  1,                    /* numbering plan indicator = ISDN/telephony numbering plan */
  {
    0x21, 0x43, 0x65, 0x87, 0xf9
  }                     /* digits, packed in BCD format */
};

/*
 * Default msisdn. Note that if there are an odd number of
 * digits, then f should be added to the end of the digits as a filler.
 */
MTU_ADDSTR DEFAULT_MSISDN =
{
  6,                    /* number of bytes of digits in the string */
  2,                    /* nature of address = national significant number */
  1,                    /* numbering plan indicator = ISDN/telephony numbering plan */
  {
    0x44, 0x83, 0x15, 0x32, 0x54, 0xf6
  }                     /* digits, packed in BCD format */
};

/*
 * Default GGSN number.
 */
MTU_ADDSTR DEFAULT_GGSN_NUMBER =
{
  6,                    /* number of bytes of digits in the string */
  2,                    /* nature of address = national significant number */
  1,                    /* numbering plan indicator = ISDN/telephony numbering plan */
  {
    0x44, 0x79, 0x15, 0x32, 0x54, 0xf6
  }                     /* digits, packed in BCD format */
};


extern GW_OPTIONS gwOptions;
extern GTT_ENTRY gttTable[];
extern std::map<u16,SS7_REQUEST> ss7RequestMap;

/*
 * Static data
 */

static MTU_DLG dlg_data[NUM_OF_DLG_IDS]; /* Dialogue data */
extern SS7_REQUEST ss7Request[];
extern int debug_mode;
//static CL_ARGS mtu_args;                 /* Command line arguments */
static u16 mtu_dlg_id;                   /* current dialogue ID */
static u16 mtu_active;                   /* number of dialogues active */
static u32 mtu_dlg_count;                /* counts number of dialogues opened */

extern void log(const char* szFormat, ...);
extern int Send_Triplets_to_PS(int req_id);
extern int SendRequestResultToClient(int ss7dialogueID);
extern void OnDialogueFinish(u16 ss7dialogueID);

/* construct_GT constructs Global Title according to T-REC Q.713 requirements.
 * It is used for encoding of originating and destination addresses
 */
int construct_GT(char* address,u8 numplan,u8 subsystem_num,char* gt)
{
    u8 address_indicator=12; // bits mean: SSN included in GT, global title includes translation type, numbering plan,
                            // encoding scheme and nature of address indicator. Routing indicator=0 (route on GT)

    u8 translation_type=0;
    u8 encoding_scheme= (strlen(address) & 2) ? 1 : 2;
    u8 nature_of_address=04;

    unsigned int i=0;
    char tbcdAddr[MAX_ADDR_LEN];    // converted to TBCD

    try {
    while(i<strlen(address)+1) {
        tbcdAddr[i+1]=address[i];
        i++;
        if(i<strlen(address))
            tbcdAddr[i-1]=address[i];
        else
            tbcdAddr[i-1]='0';
        i++;
    }
    tbcdAddr[i]=0;

    //u16 ssn_encoded=subsystem_num << 8;
    snprintf(gt,255,"%02d%02d%02d%d%d%02d%s",address_indicator,subsystem_num,translation_type,numplan,encoding_scheme,nature_of_address,tbcdAddr);

    return 0;
    }
    catch(...) {
        log("Exception caught in construct_GT");
        return -999;
    }
}

int translate_IMSI(char* imsi,u8 gtt_size,char* dest_address)
{
    int i;
    dest_address[0]=0;

    try {
    for (i=0;i<gtt_size;i++) {
        // loop for non-wildcard imsi prefixes
        if(!strncmp(imsi,gttTable[i].imsi_prefix,strlen(gttTable[i].imsi_prefix)) && strcmp(gttTable[i].imsi_prefix,"+")) {
            // appropriate entry. Analyze translation pattern
            if(!strcmp(gttTable[i].ptn_replace,"+")) {
                // replace pattern is wildcard - no translation needed, just use IMSI
                snprintf(dest_address,255,"%s",imsi);
                log("IMSI %s translated using \"%s\" entry to GT %s (numplan %s)",imsi,gttTable[i].name,dest_address,gttTable[i].str_numplan);
                return gttTable[i].numplan;
            }
            // replacement of digits needed
            if(gttTable[i].ptn_keep_remove)
                snprintf(dest_address,255,"%s%s",gttTable[i].ptn_replace,imsi+strlen(gttTable[i].imsi_prefix));
            else
                snprintf(dest_address,255,"%s",gttTable[i].ptn_replace);
            log("IMSI %s translated using \"%s\" entry to GT %s (numplan %s)",imsi,gttTable[i].name,dest_address,gttTable[i].str_numplan);
            return gttTable[i].numplan;
        }
    }

    // could not find IMSI prefix in non-wildcard pattern, try wildcard
    for (i=0;i<gtt_size;i++) {
        // loop for wildcard patterns
        if(!strcmp(gttTable[i].imsi_prefix,"+")) {
            if(!strcmp(gttTable[i].ptn_replace,"+")) {
                // replace pattern is wildcard - no translation needed, just use IMSI
                snprintf(dest_address,255,"%s",imsi);
                log("IMSI %s translated using \"%s\" entry to GT %s (numplan %s)",imsi,gttTable[i].name,dest_address,gttTable[i].str_numplan);
                return gttTable[i].numplan;
            }
            if(gttTable[i].ptn_keep_remove)
                snprintf(dest_address,255,"%s%s",gttTable[i].ptn_replace,imsi);
            else
                snprintf(dest_address,255,"%s",gttTable[i].ptn_replace);
            log("IMSI %s translated using \"%s\" wildcard entry to GT %s (numplan %d)\n",gttTable[i].name,dest_address,gttTable[i].str_numplan);
            return gttTable[i].numplan;
        }
    }
    log("IMSI translation to GT failed. No appropriate entry in GTT table found\n");
    return -1;
    }
    catch(...) {
        log("Exception caught in translate_IMSI");
        return -999;
    }
}


/*
 * MTU_display_msg - displays sent and received messages
 *
 * Always returns zero.
 */
int MTU_display_msg(const char* prefix, MSG* m)
  /*char * prefix;         string prefix to command line display */
  /*MSG *m;                received message */
{
  HDR *h;               /* header of message to trace */
  int instance;         /* instance of module message is sent from */
  u16 mlen;             /* length of traced message */
  u8 *pptr;             /* parameter are of trace message */
  char str[2048];

  try {
  h = (HDR *)m;
  instance = GCT_get_instance(h);
  sprintf(str,"%s I%04x M t%04x i%04x f%02x d%02x s%02x" , prefix, instance,
         h->type, h->id, h->src, h->dst, h->status);

  if ((mlen = m->len) > 0)
  {
    if (mlen > MTU_MAX_PARAM_LEN)
      mlen = MTU_MAX_PARAM_LEN;
    sprintf(str,"%s p",str);
    pptr = get_param(m);
    while (mlen--)
    {
      sprintf(str,"%s%c%c", str, BIN2CH(*pptr/16), BIN2CH(*pptr%16));
      pptr++;
    }
  }
  //printf("\n");
  log("%s",str);

  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_display_msg");
      return -999;
  }
} /* end of MTU_display_msg() */


 /*
 * MTU_display_recvd_message - displays received messages
 *
 * Always returns zero.
 */
int MTU_display_recvd_msg(MSG* m)
  /*MSG *m;                received message */
{
    HDR *h;               /* header of message to trace */

    h = (HDR *)m;
  try {
  switch (m->hdr.type)
  {
    case MAP_MSG_DLG_IND:
      switch (*(get_param(m)))
      {
        case MAPDT_OPEN_IND:
          log("Rx: 0x%04x received Open Indication",h->id);
          break;
        case MAPDT_CLOSE_IND:
          log("Rx: 0x%04x received Close Indication",h->id);
          break;
        case MAPDT_DELIMITER_IND:
          log("Rx: 0x%04x received Delimiter Indication",h->id);
          break;
        case MAPDT_U_ABORT_IND:
          log("Rx: 0x%04x received U Abort Indication",h->id);
          break;
        case MAPDT_P_ABORT_IND:
          log("Rx: 0x%04x received P Abort Indication",h->id);
          break;
        case MAPDT_NOTICE_IND:
          log("Rx: 0x%04x received Notice Indication",h->id);
          break;
        case MAPDT_OPEN_CNF:
          log("Rx: 0x%04x received Open Confirmation",h->id);
          break;
        default:
          log("Rx: 0x%04x received dialogue indication",h->id);
          break;
      }
      break;

    case MAP_MSG_SRV_IND:
      switch (*(get_param(m)))
      {
        case MAPST_SND_RTIGPRS_CNF:
          log("Rx: 0x%04x received Send Routing Info for GPRS Confirmation",h->id);
          break;
        case MAPST_SEND_IMSI_CNF:
          log("Rx: 0x%04x received Send IMSI Confirmation",h->id);
          break;
        case MAPST_FWD_SM_CNF:
          log("Rx: 0x%04x received Forward Short Message Confirmation",h->id);
          break;
        case MAPST_MT_FWD_SM_CNF:
          log("Rx: 0x%04x received MT Forward Short Message Confirmation",h->id);
          break;
        case MAPST_SND_RTISM_CNF:
          log("Rx: 0x%04x received Send Routing Info for SMS Confirmation",h->id);
          break;
        case MAPST_UNSTR_SS_REQ_IND:
          log("Rx: 0x%04x received UnstructuredSS-Request Indication",h->id);
          break;
        case MAPST_PRO_UNSTR_SS_REQ_CNF:
          log("Rx: 0x%04x received ProcessUnstructured-Request-Confirmation",h->id);
          break;
        case MAPST_SEND_AUTH_INFO_CNF:
          log("Rx: 0x%04x received Send-Authentication-Info-Confirmation",h->id);
          break;
        default:
          log("Rx: 0x%04x received service indication",h->id);
          break;
      }
      break;
  }

  MTU_display_msg("Rx:", m);
  return(0);
    }
    catch(...) {
        log("Exception caught in MTU_display_recvd_msg");
        return -999;
    }
}

/*
 * MTU_display_sent_message - displays sent messages
 *
 * Always returns zero.
 */
int MTU_display_sent_msg(MSG* m)
  /*MSG *m;                received message */
{
  HDR* h = (HDR *)m;
  try {
  switch (m->hdr.type)
  {
    case MAP_MSG_DLG_REQ:
      switch (*(get_param(m)))
      {
        case MAPDT_OPEN_REQ:
          log("Tx: 0x%04x sending Open Request",h->id);
          break;
        case MAPDT_OPEN_RSP:
          log("Tx: 0x%04x sending Open Response",h->id);
          break;
        case MAPDT_CLOSE_REQ:
          log("Tx: 0x%04x sending Close Request",h->id);
          break;
        case MAPDT_DELIMITER_REQ:
          log("Tx: 0x%04x sending Delimiter Request",h->id);
          break;
        case MAPDT_U_ABORT_REQ:
          log("Tx: 0x%04x sending U Abort Request",h->id);
          break;
        default:
          log("Tx: 0x%04x sending dialogue request",h->id);
          break;
      }
      break;

    case MAP_MSG_SRV_REQ:
      switch (*(get_param(m)))
     {
        case MAPST_SND_RTIGPRS_REQ:
          log("Tx: 0x%04x sending Send Routing Info for GPRS Request",h->id);
          break;
        case MAPST_SEND_IMSI_REQ:
          log("Tx: 0x%04x sending Send IMSI Request",h->id);
          break;
        case MAPST_FWD_SM_REQ:
          log("Tx: 0x%04x sending Forward Short Message Request",h->id);
          break;
        case MAPST_MT_FWD_SM_REQ:
          log("Tx: 0x%04x sending Forward Short Message Request",h->id);
          break;
        case MAPST_PRO_UNSTR_SS_REQ_REQ:
          log("Tx: 0x%04x sending ProcessUnstructured-Request Req",h->id);
          break;
        case MAPST_UNSTR_SS_REQ_RSP:
          log("Tx: 0x%04x sending UnstructuredSSRequestRSP",h->id);
          break;
        case MAPST_SND_RTISM_REQ:
          log("Tx: 0x%04x sending Send Routing Info for SMS Request",h->id);
          break;                  
        default:
          log("Tx: 0x%04x sending service request",h->id);
          break;
     }
     break;
  }

  MTU_display_msg("Tx:", m);
  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_display_sent_msg");
      return -1;
  }

}

/*
 * MTU_send_msg sends a MSG. On failure the
 * message is released and the user notified.
 *
 * Always returns zero.
 */
int MTU_send_msg(MSG* m)
  /*MSG   *m;              MSG to send */
{
  try {
  if (GCT_send(m->hdr.dst, (HDR *)m) != 0)
  {
    MTU_disp_err(m->hdr.id,"*** failed to send message");
    relm((HDR *)m);
  }
  return(0);

    }
    catch(...) {
        log("Exception caught in MTU_send_msg");
        return -999;
    }
} /* end of MTU_send_msg()*/


  /*
   * MTU_alloc_dlg_id allocates a dialogue ID to be used when opening a dialogue.
   *
   * Returns zero if a dialogue ID was allocated.
   *         -1   if no dialogue ID could be allocated.
   */
u16 MTU_alloc_dlg_id(u16 *dlg_id_ptr)
  /*u16 *dlg_id_ptr;         updated to point to a free dialogue id */
  {
    u16 i;                /* dialogue ID loop counter */
    int found;            /* has an idle dialogue been found? */

    found = 0;

    /*
     * Look for an idle dialogue id starting at mtu_dlg_id
     */
    for (i = mtu_dlg_id; i < (gwOptions.base_dlg_id + gwOptions.num_dlg_ids); i++)
    {
      if (dlg_data[i - gwOptions.base_dlg_id].state == rs_idle)
      {
        found = 1;
        break;
      }
    }

    /*
     * If we haven't found one yet, start looking again, this time from the
     * base id.
     */
    if (found == 0)
    {
      for (i = gwOptions.base_dlg_id; i < mtu_dlg_id; i++)
      {
        if (dlg_data[i - gwOptions.base_dlg_id].state == rs_idle)
        {
          found = 1;
          break;
        }
      }
    }

    if (found)
    {
      /*
       * Update the dialogue id to return and increment the active dialogue count
       */
      *dlg_id_ptr = i;
      mtu_active++;

      /*
       * Select the next dialogue id to start looking for an idle one.
       * If we've reached the end of the range then start from the base id.
       */
      if (mtu_dlg_id == (gwOptions.base_dlg_id + gwOptions.num_dlg_ids - 1))
        mtu_dlg_id = gwOptions.base_dlg_id;
      else
        mtu_dlg_id++;

      return (0);
    }
    else
    {
      /*
       * No idle dialogue id found
       */
      return (-1);
    }
  } /* end of MTU_alloc_dlg_id() */


/*
 * MTU_get_dlg_data returns the dlg data structure.
 */
MTU_DLG *MTU_get_dlg_data(u16 dlg_id)
{
   return(&(dlg_data[dlg_id - gwOptions.base_dlg_id]));
}

/*
 * MTU_alloc_invoke_id allocates an invoke ID to be used for a MAP request.
 */
u8 MTU_alloc_invoke_id()
{
  /*
   * This function always uses the same invoke ID (because only one invocation
   * can be in progress at one time in this example program). In a real
   * application this function would have to search for a free invoke ID and
   * allocate that.
   */
  return(DEFAULT_INVOKE_ID);
};

/*
 * MTU_send_dlg_req allocates a message (using the
 * getm() function) then converts the primitive parameters
 * from the 'C' structured representation into the correct
 * format for passing to the MAP module.
 * The formatted message is then sent.
 *
 * Always returns zero.
 */
int MTU_send_dlg_req(MTU_MSG* req)
  /*MTU_MSG  *req;         structured primitive request to send */
{
  MSG *m;               /* message sent to MAP */

  req->dlg_prim = 1;

  try {
  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, req->ss7dialogueID, NO_RESPONSE,
                                      MTU_MAX_PARAM_LEN)) != 0)
  {
    m->hdr.src = gwOptions.mtu_mod_id;
    m->hdr.dst = gwOptions.mtu_map_id;

    /*
     * Format the parameter area of the message and
     * (if successful) send it
     */
    if (MTU_dlg_req_to_msg(m, req) != 0)
    {
      MTU_disp_err(m->hdr.id,"failed to format dialogue primitive request");
      relm(&m->hdr);
    }
    else
    {
      if (gwOptions.log_options & MTU_TRACE_TX)
        MTU_display_sent_msg(m);

      /*
       * and send to the call processing module:
       */
      MTU_send_msg(m);
    }
  }
  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_send_dlg_req");
      return -999;
  }
} /* end of MTU_send_dlg_req() */


/*
 * MTU_send_srv_req allocates a message (using the
 * getm() function) then converts the primitive parameters
 * from the 'C' structured representation into the correct
 * format for passing to the MAP module.
 * The formatted message is then sent.
 *
 * Always returns zero.
 */
int MTU_send_srv_req(MTU_MSG* req)
  /*MTU_MSG  *req;         structured primitive request to send */
{
  MSG *m;               /* message sent to MAP */

  req->dlg_prim = 0;

  try {
  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, req->ss7dialogueID, NO_RESPONSE,
                                      MTU_MAX_PARAM_LEN)) != 0)
  {
    m->hdr.src = gwOptions.mtu_mod_id;
    m->hdr.dst = gwOptions.mtu_map_id;

    /*
     * Format the parameter area of the message and
     * (if successful) send it
     */
    if (MTU_srv_req_to_msg(m, req) != 0)
    {
      MTU_disp_err(m->hdr.id,"failed to format service primitive request");
      relm(&m->hdr);
    }
    else
    {
      if (gwOptions.log_options & MTU_TRACE_TX)
        MTU_display_sent_msg(m);
      /*
       * and send to the call processing module:
       */
      MTU_send_msg(m);
    }
  }
  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_send_srv_req");
      return -999;
  }
}

int MTU_send_close_request(u16 dlg_id)
{
    MTU_MSG req;          /* structured form of request message */

    SS7_REQUEST& ss7Req=ss7RequestMap.at(dlg_id);


    req.ss7dialogueID = dlg_id;
    req.type = MAPDT_CLOSE_REQ;
    memset((void *)req.pi, 0, PI_BYTES);
    req.ss7invokeID = ss7Req.ss7invokeID;
    bit_set(req.pi, MAPPN_invoke_id);
    req.release_method = MAPRM_normal_release;
    bit_set(req.pi, MAPPN_release_method);

    return MTU_send_dlg_req(&req);
}

/*
 * MTU_send_Delimit
 *
 * Sends a Delimit message to MAP.
 *
 * Always returns zero.
 */
int MTU_send_Delimit(u16 dlg_id)
{
  MSG  *m;              /* Pointer to message to transmit */
  u8   *pptr;           /* Pointer to a parameter */

  try {
  if (gwOptions.log_options & MTU_TRACE_TX)
    log("Tx: Sending Delimiter");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = gwOptions.mtu_mod_id;
    m->hdr.dst = gwOptions.mtu_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Delimit Request
     * Parameter name   = user reason tag
     * Parameter length = 1
     * Parameter value  = reason
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_DELIMITER_REQ;
    pptr[1] = 0x00;

    /*
     * Now send the message
     */
    MTU_send_msg( m);
  }
  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_send_Delimit");
      return -999;
  }
}

int MTU_send_Auth_Info_Response(u16 dlg_id,u16 invoke_id)
{
    MTU_MSG req;          /* structured form of request message */
    u8 da_len;           /* length of formatted u-data */
    u8 num_da_chars;     /* number of formatted*/

    try {
    memset((void *)req.pi, 0, PI_BYTES);
    req.ss7dialogueID = dlg_id;
    req.type = MAPST_SEND_AUTH_INFO_RSP;
    req.ss7invokeID = invoke_id;
    bit_set(req.pi, MAPPN_invoke_id);

    //bit_set(req.pi, MAPPN_timeout);
    //req.timeout = 15;

    MTU_send_srv_req(&req);

    return(0);
    }
    catch(...) {
        log("Exception caught in MTU_send_Auth_Info_Response");
        return -999;
    }
}

int MTU_send_User_Abort(u16 dlg_id,u16 invoke_id)
{
    MTU_MSG msg;

    try {
    msg.type = MAPDT_U_ABORT_REQ;
    msg.ss7dialogueID=dlg_id;
    msg.ss7invokeID=invoke_id;
    memset((void *)msg.pi, 0, PI_BYTES);

    bit_set(msg.pi, MAPPN_user_rsn);
    msg.user_reason = MAPUR_unspecified_reason;
    return MTU_send_dlg_req(&msg);
    }
    catch(...) {
        log("Exception caught in MTU_send_User_Abort");
        return -999;
    }
}

int MTU_send_auth_info(u16 dlg_id,bool bInitial)
    /*u16 dlg_id;            dialogue ID */
    /*bool bInitial         true for first request, false for subsequent  */
  {
//    MTU_DLG *dlg;         /* dialogue data structure */
    MTU_MSG req;          /* structured form of request message */
    int i,j;              /* loop counters */


    /*
     * The following parameters are set in the
     * MAP-SEND-AUTHENTIFICATION-INFO:
     *    IMSI -
     *    Number of Requested Vectors
     */

    try {
    SS7_REQUEST& ss7Req=ss7RequestMap.at(dlg_id);

    memset((void *)req.pi, 0, PI_BYTES);
    req.ss7dialogueID = dlg_id;
    req.type = MAPST_SEND_AUTH_INFO_REQ;

    req.ss7invokeID = ss7Req.ss7invokeID;

    bit_set(req.pi, MAPPN_invoke_id);

    if(bInitial) {
        // Fill these parameters for initial request only, not for subsequent

        /*
         * IMSI
         */
        bit_set(req.pi, MAPPN_imsi);

        i=0;
        j=0;

          while ((ss7Req.imsi != '\0') && (i < MAX_BCDSTR_LEN))
          {
            req.imsi.data[i] = hextobin(ss7Req.imsi[j]);

            if (ss7Req.imsi[++j] != '\0')
            {
              req.imsi.data[i++] |= (hextobin(ss7Req.imsi[j])) << 4;
              j++;
            }
            else
            {
              /*
               * pack the spare 4 bits with 1111
               */
              req.imsi.data[i++] |= 0xf0;
              break;
            }
          }
          req.imsi.num_bytes = i;
    //    }

        /*
         * Number of requested vectors
         */
        bit_set(req.pi, MAPPN_nb_req_vect);
        req.nb_req_vect = ss7Req.requestedVectorsNum;


        /*
         * Operation timeout - 5 seconds
         */
        bit_set(req.pi, MAPPN_timeout);
        req.timeout = gwOptions.MAP_timeout;
    }

    MTU_send_srv_req(&req);

    return(0);
    }
    catch(...) {
        log("Exception caught in MTU_send_auth_info");
        return -999;
    }

  } /* end of MTU_send_auth)_info() */

/* MTU_Send_UnstructuredSSRequestRSP
 * Formats and sends an UnstructuredSS-RequestRSP message
 * in response to a received ProcessUnstructuredSS-Request.
 *
 *   MAP-OPEN-RSP
 *   service primitive 'UnstructuredSSRequestRSP'
 *   MAP-DELIMITER-REQ
 *
 */
 int MTU_Send_UnstructuredSSRequestRSP(u16 dlg_id, u8 invoke_id)
  /*u16 dlg_id;           Dialogue id */
  /*u8  invoke_id;        Invoke_id */
{
  MTU_MSG req;          /* structured form of request message */
  u8 da_len;           /* length of formatted u-data */
  u8 num_da_chars;     /* number of formatted*/

  /*
   * The following parameters are set in the
   * MTU_Send_UnstructuredSSRequestRSP:
   *    ussd-DataCodingScheme
   *    ussd-string - this will be entered by the user e.g. *88#
   */
   
  memset((void *)req.pi, 0, PI_BYTES);
  req.ss7dialogueID = dlg_id;
  req.type = MAPST_UNSTR_SS_REQ_RSP;
  req.ss7invokeID = invoke_id;
  bit_set(req.pi, MAPPN_invoke_id);

  /* 
   * USSD coding parameter 
   */ 
  bit_set(req.pi, MAPPN_USSD_coding);

  /*
  * USSD coding set to 'GSM default alphabet' 00001111
  * see GSM 03.38 'Cell Broadcast Data Coding Scheme'
  * for further detail
  */
  bit_to_byte(req.ussd_coding.data, 0x1, 0);
  bit_to_byte(req.ussd_coding.data, 0x1, 1);
  bit_to_byte(req.ussd_coding.data, 0x1, 2);
  bit_to_byte(req.ussd_coding.data, 0x1, 3);
  bit_to_byte(req.ussd_coding.data, 0x0, 4);
  bit_to_byte(req.ussd_coding.data, 0x0, 5);
  bit_to_byte(req.ussd_coding.data, 0x0, 6);
  bit_to_byte(req.ussd_coding.data, 0x0, 7);
 
  req.ussd_coding.num_bytes = 1;

  /* USSD string parameter */ 
  bit_set(req.pi, MAPPN_USSD_string);  

  /*
   * USSD string 
   */
  /* 
   * Use the following line to check USSD string formatting ..
   * mtu_args.message="XY Telecom\n 1. Balance\n 2. Texts Remaining";   

  
  req.ussd_string.num_bytes = 1; /* USSD string, allow byte for data length

  num_da_chars = MTU_USSD_str_to_def_alph(mtu_args.message,
                                     &req.ussd_string.data[req.ussd_string.num_bytes],
                                     &da_len, 
                                     MAX_DATA_LEN - req.ussd_string.num_bytes);*/
  /*
   * fill in the ussd_string, the number of formated default alphabet characters
   */
  req.ussd_string.data[req.ussd_string.num_bytes - 1] = num_da_chars;   
  
  req.ussd_string.num_bytes += da_len; 

  /*
   * Operation timeout - 15 seconds
   */
  bit_set(req.pi, MAPPN_timeout);
  req.timeout = 15;

  MTU_send_srv_req(&req);

  req.type = MAPDT_DELIMITER_REQ;
  memset((void *)req.pi, 0, PI_BYTES);

  MTU_send_dlg_req(&req);
  return(0);
           
} /* end of MTU_process_uss_rsp() */

/*
 * MTU_process_uss_req sends the Process USS service request.
 *
 * Always returns zero.
 */
int MTU_process_uss_req (u16 dlg_id,u8 invoke_id)   /* USSD */
  /*u16 dlg_id;            dialogue ID */
  /*u8 invoke_id;         invoke ID */
{
  MTU_MSG req;          /* structured form of request message */
  u8 da_len;           /* length of formatted u-data */
  u8 num_da_chars;     /* number of formatted*/
  
  /*
   * The following parameters are set in the
   * MAP-ProcessUnstructuredSS-Request:
   *    ussd-DataCodingScheme
   *    ussd-string - this will be entered by the user e.g. *55#
   */
   
  memset((void *)req.pi, 0, PI_BYTES);
  req.ss7dialogueID = dlg_id;
  req.type = MAPST_PRO_UNSTR_SS_REQ_REQ;
  req.ss7invokeID = invoke_id;
  bit_set(req.pi, MAPPN_invoke_id);

  /* USSD coding parameter */ 
  bit_set(req.pi, MAPPN_USSD_coding);

  /*
  * USSD coding set to 'GSM default alphabet' 00001111
  * see GSM 03.38 'Cell Broadcast Data Coding Scheme'
  * for further detail
  */

  bit_to_byte(req.ussd_coding.data, 0x1, 0);
  bit_to_byte(req.ussd_coding.data, 0x1, 1);
  bit_to_byte(req.ussd_coding.data, 0x1, 2);
  bit_to_byte(req.ussd_coding.data, 0x1, 3);
  bit_to_byte(req.ussd_coding.data, 0x0, 4);
  bit_to_byte(req.ussd_coding.data, 0x0, 5);
  bit_to_byte(req.ussd_coding.data, 0x0, 6);
  bit_to_byte(req.ussd_coding.data, 0x0, 7);
 
  req.ussd_coding.num_bytes = 1;

  /* 
   * USSD string parameter 
   */ 
  bit_set(req.pi, MAPPN_USSD_string);  

  /*
   * USSD string 

  req.ussd_string.num_bytes = 1; /* USSD string, allow byte for data length

  num_da_chars = MTU_USSD_str_to_def_alph(mtu_args.ussd_string,
                                     &req.ussd_string.data[req.ussd_string.num_bytes],
                                     &da_len, 
                                     MAX_DATA_LEN - req.ussd_string.num_bytes);*/
  /*
   * fill in the ussd_string, the number of formated default alphabet characters
   */
  req.ussd_string.data[req.ussd_string.num_bytes - 1] = num_da_chars;   
  
  req.ussd_string.num_bytes += da_len;

  /*
   * Operation timeout - 15 seconds
   */
  bit_set(req.pi, MAPPN_timeout);
  req.timeout = 15;

  MTU_send_srv_req(&req);

  return(0);
           
} /* end of MTU_process_uss_req() */

const char* GetMAPUserErrorDescr(MAP_USER_ERR err)
{
    switch(err) {
    case MAPUE_unknown_subscriber:
        return "MAPUE_unknown_subscriber";
    case MAPUE_unknown_MSC:
        return "MAPUE_unknown_MSC";
    case MAPUE_unidentified_subscriber:
        return "MAPUE_unidentified_subscriber";
    case MAPUE_absentsubscriber_SM:
        return "MAPUE_absentsubscriber_SM";
    case MAPUE_unknown_equipment:
        return "MAPUE_unknown_equipment";
    case MAPUE_roaming_not_allowed:
        return "MAPUE_roaming_not_allowed";
    case MAPUE_illegal_subscriber:
        return "MAPUE_illegal_subscriber";
    case MAPUE_bearer_service_not_provisioned:
        return "MAPUE_bearer_service_not_provisioned";
    case MAPUE_teleservice_not_provisioned:
        return "MAPUE_teleservice_not_provisioned";
    case MAPUE_illegal_equipment:
        return "MAPUE_illegal_equipment";
    case MAPUE_call_barred:
        return "MAPUE_call_barred";
    case MAPUE_forwarding_violation:
        return "MAPUE_forwarding_violation";
    case MAPUE_cug_reject:
        return "MAPUE_cug_reject";
    case MAPUE_illegal_ss_operation:
        return "MAPUE_illegal_ss_operation";
    case MAPUE_ss_error_status:
        return "MAPUE_ss_error_status";
    case MAPUE_ss_not_available:
        return "MAPUE_ss_not_available";
    case MAPUE_ss_subscription_violation:
        return "MAPUE_ss_subscription_violation";
    case MAPUE_ss_incompatibility:
        return "MAPUE_ss_incompatibility";
    case MAPUE_facility_not_supported:
        return "MAPUE_facility_not_supported";
    case MAPUE_pw_registration_failure:
        return "MAPUE_pw_registration_failure";
    case MAPUE_negative_pw_check:
        return "MAPUE_negative_pw_check";
    case MAPUE_no_handover_number_available:
        return "MAPUE_no_handover_number_available";
    case MAPUE_subsequent_handover_failure:
        return "MAPUE_subsequent_handover_failure";
    case MAPUE_absent_subscriber:
        return "MAPUE_absent_subscriber";
    case MAPUE_subscriber_busy_for_MT_SMS:
        return "MAPUE_subscriber_busy_for_MT_SMS";
    case MAPUE_SM_delivery_failure:
        return "MAPUE_SM_delivery_failure";
    case MAPUE_message_waiting_list_full:
        return "MAPUE_message_waiting_list_full";
    case MAPUE_system_failure:
        return "MAPUE_system_failure";
    case MAPUE_data_missing:
        return "MAPUE_data_missing";
    case MAPUE_unexpected_data_value:
        return "MAPUE_unexpected_data_value";
    case MAPUE_resource_limitation:
        return "MAPUE_resource_limitation";
    case MAPUE_initiating_release:
        return "MAPUE_initiating_release";
    case MAPUE_no_roaming_number_available:
        return "MAPUE_no_roaming_number_available";
    case MAPUE_tracing_buffer_full:
        return "MAPUE_tracing_buffer_full";
    case MAPUE_number_of_pw_attempts_violation:
        return "MAPUE_number_of_pw_attempts_violation";
    case MAPUE_number_changed:
        return "MAPUE_number_changed";
    case MAPUE_busy_subscriber:
        return "MAPUE_busy_subscriber";
    case MAPUE_no_subscriber_reply:
        return "MAPUE_no_subscriber_reply";
    case MAPUE_forwarding_failed:
        return "MAPUE_forwarding_failed";
    case MAPUE_or_not_allowed:
        return "MAPUE_or_not_allowed";
    case MAPUE_ATI_not_allowed:
        return "MAPUE_ATI_not_allowed";
    case MAPUE_unauthorised_requesting_network:
        return "MAPUE_unauthorised_requesting_network";
    case MAPUE_unauthorised_LCS_client:
        return "MAPUE_unauthorised_LCS_client";
    case MAPUE_position_method_failure:
        return "MAPUE_position_method_failure";
    case MAPUE_unknown_or_unreachable_LCS_client:
        return "MAPUE_unknown_or_unreachable_LCS_client";
    case MAPUE_mm_event_not_supported:
        return "MAPUE_mm_event_not_supported";
    case MAPUE_atsi_not_allowed:
        return "MAPUE_atsi_not_allowed";
    case MAPUE_atm_not_allowed:
        return "MAPUE_atm_not_allowed";
    case MAPUE_information_not_available:
        return "MAPUE_information_not_available";
    case MAPUE_unknown_alphabet:
        return "MAPUE_unknown_alphabet";
    case MAPUE_ussd_busy:
        return "MAPUE_ussd_busy";
    default:
        return "";
    }
}

const char* GetMAPProviderErrorDescr(MAP_PROV_ERR err)
{
    switch(err) {
    case MAPPE_duplicated_invoke_id:
        return "MAPPE_resource_limitation";
    case MAPPE_not_supported_service:
        return "MAPPE_resource_limitation";
    case MAPPE_mistyped_parameter:
        return "MAPPE_mistyped_parameter";
    case MAPPE_resource_limitation:
        return "MAPPE_resource_limitation";
    case MAPPE_initiating_release:
        return "MAPPE_initiating_release";
    case MAPPE_unexpected_response_from_peer:
        return "MAPPE_unexpected_response_from_peer";
    case MAPPE_service_completion_failure:
        return "MAPPE_service_completion_failure";
    case MAPPE_no_response_from_peer:
        return "MAPPE_no_response_from_peer";
    case MAPPE_invalid_response_received:
        return "MAPPE_invalid_response_received";
    default:
        return "";
    }
}

const char* GetMAPDiagnosticInfoDescr(MAP_DIAG_INF inf)
{
    switch(inf) {
    case MAPPDI_short_term_resource_limitation:
        return "MAPPDI_short_term_resource_limitation";
    case MAPPDI_long_term_resource_limitation:
        return "MAPPDI_long_term_resource_limitation";
    case MAPPDI_handover_cancellation:
        return "MAPPDI_handover_cancellation";
    case MAPPDI_radio_channel_release:
        return "MAPPDI_radio_channel_release";
    case MAPPDI_network_path_release:
        return "MAPPDI_network_path_release";
    case MAPPDI_call_release:
        return "MAPPDI_call_release";
    case MAPPDI_associated_procedure_release:
        return "MAPPDI_associated_procedure_release";
    case MAPPDI_tandem_dialogue_release:
        return "MAPPDI_tandem_dialogue_release";
    case MAPPDI_remote_operations_failure:
        return "MAPPDI_remote_operations_failure";
    default:
        return "";
    }
}

const char* GetMAPProblemDiagnosticDescr(MAP_PROB_DIAG inf)
{
    switch(inf) {
    case MAPPD_abnormal_event_detected_by_peer:
        return "MAPPD_abnormal_event_detected_by_peer";
    case MAPPD_response_rejected_by_peer:
        return "MAPPD_response_rejected_by_peer";
    case MAPPD_abnormal_event_rx_from_peer:
        return "MAPPD_abnormal_event_rx_from_peer";
    case MAPPD_message_not_delivered:
        return "MAPPD_message_not_delivered";
    }
    return "";
}

const char* GetMAPUserReasonDescr(MAP_USER_RSN rsn)
{
    switch(rsn) {
    case MAPUR_user_specific:
        return "User specific reason";
    case MAPUR_user_resource_limitation:
        return "User resource limitation";
    case MAPUR_resource_unavail:
        return "Resource unavailable";
    case MAPUR_app_proc_cancelled:
        return "Application procedure cancelled";
    case MAPUR_procedure_error:
        return "Procedure Error";
    case MAPUR_unspecified_reason:
        return "Unspecified Reason";
    case MAPUR_version_not_supported:
        return "Version Not Supported";
    }
    return "";
}
const char* GetMAPProviderReasonDescr(MAP_PROV_RSN rsn)
{
    switch(rsn) {
    case MAPPR_prov_malfct:
        return "Provider malfunction";
    case MAPPR_dlg_rlsd:
        return "Supporting dialogue/transaction released";
    case MAPPR_rsrc_limit:
        return "Resource limitation";
    case MAPPR_mnt_act:
        return "Maintenance activity";
    case MAPPR_ver_incomp:
        return "Version incompatibility";
    case MAPPR_ab_dlg:
        return "Abnormal MAP dialogue";
    case MAPPR_invalid_PDU:
        return "Invalid PDU (obsolete - no longer used)";
    case MAPPR_idle_timeout:
        return "Idle Timeout";
    }
    return "";
}

const char* GetMAPRefuseReasonDescr(MAP_REF_RSN rsn)
{
    switch(rsn) {
    case MAPRR_no_reason:
        return "No reason given";
    case MAPRR_inv_dest_ref:
        return "Invalid destination reference";
    case MAPRR_inv_orig_ref:
        return "Invalid origination reference";
    case MAPRR_appl_context:
        return "Application context not supported";
    case MAPRR_ver_incomp:
        return "Potential version incompatibility";
    case MAPRR_node_notreach:
        return "Remote node not reachable";
    }
    return "";
}
/*
 * MTU_open_dlg opens a dialogue by sending:
 *
 *   MAP-OPEN-REQ
 *   service primitive request
 *   MAP-DELIMITER-REQ
 *
 * Returns zero if the dialogue was successfully opened.
 *         -1   if the dialogue could not be opened.
 */
int MTU_open_dlg(/*u8 service, MTU_BCDSTR *imsi*/u16 dlg_id,char* imsi)
  /*u8 service;            service to use for this dialogue */
  /*MTU_BCDSTR *imsi;      IMSI */
{
  MTU_MSG req;          /* structured form of request message */
  //MTU_DLG *dlg;         /* dialogue data structure */
  int i,j;              /* loop counters */
  char orig_address[256];
  char dest_address[256];

  try {
  /*
   * Open the dialogue by sending MAP-OPEN-REQ.
   */
  memset((void *)req.pi, 0, PI_BYTES);
  req.ss7dialogueID = dlg_id;
  req.type = MAPDT_OPEN_REQ;

  /*
   * Copy the appropriate application context into the message structure
   */
  bit_set(req.pi, MAPPN_applic_context);

   // Set Application Context
   for (i=0; i<AC_LEN; i++)
       req.applic_context[i] = sendAuthInfoContext[i];

  /*
   * Copy the destination address parameter into the MAP-OPEN-REQ,
   * converting from ASCII to hex, and packing into BCD format.
   */
  bit_set(req.pi, MAPPN_dest_address);
  char address[MAX_ADDR_LEN];
  int numplan=translate_IMSI(imsi,gwOptions.gtt_table_size,address);
  if(numplan<0) {
      ss7RequestMap.at(dlg_id).error="IMSI translation to GT failed. No appropriate entry in GTT table found";
      return -1;
  }

  construct_GT(address,numplan,SUBSYSTEM_HLR,dest_address);

  i = 0;
  j = 0;
  while ((dest_address[j] != '\0') && (i < MAX_ADDR_LEN))
  {
    req.dest_address.data[i] = (hextobin(dest_address[j])) << 4;

    if (dest_address[j+1] != '\0')
    {
      req.dest_address.data[i++] |= hextobin(dest_address[j+1]);
      j += 2;
    }
    else
    {
      i++;
      break;
    }
  }
  req.dest_address.num_bytes = i;

  /*
   * Copy the origination address parameter into the MAP-OPEN-REQ, converting
   * from ASCII to hex, and packing into BCD format.
   */
  bit_set(req.pi, MAPPN_orig_address);

  construct_GT(gwOptions.service_centre,1 /*numplan=E.164*/,SUBSYSTEM_VLR,orig_address);

  i = 0;
  j = 0;
  while ((orig_address[j] != '\0') && (i < MAX_ADDR_LEN))
  {

    req.orig_address.data[i] = (hextobin(orig_address[j])) << 4;

    if (orig_address[j+1] != '\0')
    {
      req.orig_address.data[i++] |= hextobin(orig_address[j+1]);
      j += 2;
    }
    else
    {
      i++;
      break;
    }
  }
  req.orig_address.num_bytes = i;

    MTU_send_dlg_req(&req);

  /*
   * Send the appropriate service primitive request. First, allocate an invoke
   * ID. This invoke ID is used in all messages to and from MAP associated with
   * this request.
   */
  //dlg->invoke_id = MTU_alloc_invoke_id();

  MTU_send_auth_info(dlg_id,true);

  /*
   * Now send MAP-DELIMITER-REQ to indicate that no more requests will be sent
   * (for now).
   */
  req.type = MAPDT_DELIMITER_REQ;
  memset((void *)req.pi, 0, PI_BYTES);

  MTU_send_dlg_req(&req);

  /*
   * Set state to wait for the MAP-OPEN-CNF which indicates whether or not
   * the dialogue has been accepted.
   */


  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_open_dlg");
      return -999;
  }
} /* end of MTU_open_dlg() */

void MTU_release_invoke_id(u8 invoke_id,MTU_DLG* dlg)
  /*u8 invoke_id;          invoke ID */
  /*MTU_DLG *dlg;          dialogue data structure */
{
  /*
   * Since only one invoke ID is ever used, it is not necessary to release
   * it. In a real application, the invoke ID would have to be marked as
   * free in this function.
   */

  dlg->ss7invokeID = 0;

  return;
}


/*
 * MTU_release_dlg_id is used to release the dialogue ID at the end of the
 * dialogue so that it may be used for another dialogue.
 */
void MTU_release_dlg_id(u16 dlg_id)
  /*u16 dlg_id;            Dialogue id of dialogue being released */
{
  MTU_DLG *dlg;
  /*
   * Decrement the active count.
   */
  mtu_active--;

  dlg = MTU_get_dlg_data(dlg_id);

  /*
   * Set dialogue state to idle.
   */
  dlg->state = rs_idle;


  /*
   * Clear any stored IMSI by setting the number of bytes to zero.
   */
  dlg->imsi.num_bytes = 0;

  return;
}

/*
 * MTU_release_request is used to release the SS7 request at the end of the
 * dialogue so that it may be used for another request.
 * Uses dlg_id from SS7 message
 */
void MTU_release_request(u16 dlg_id)
{

  mtu_active--;

  ss7RequestMap.at(dlg_id).state = rs_finished;
  ss7RequestMap.at(dlg_id).ss7invokeID = 0;
  time(&ss7RequestMap.at(dlg_id).stateChangeTime);
  /*
   * Clear any stored IMSI by setting the number of bytes to zero.
   */
  ss7RequestMap.at(dlg_id).imsi[0]= 0;

  return;
}


/*
 * MTU_wait_open_cnf handles messages received in the waiting for open
 * confirmation state.
 *
 * Always returns zero.
 */
int MTU_wait_open_cnf(MTU_MSG* msg/*MTU_DLG* dlg*/)
  /*MTU_MSG *msg;          received message */
  /*MTU_DLG *dlg;          dialogue data structure */
{
  char buf[1024];

  try {
  if (msg->dlg_prim)
  {
    switch (msg->type)
    {
      case MAPDT_U_ABORT_IND:
      case MAPDT_P_ABORT_IND:
        /*
         * Dialogue aborted. Release the invoke ID, release the dialogue
         * ID, and idle the state machine.
         */
        if (msg->type == MAPDT_U_ABORT_IND)
        {
          if (bit_test(msg->pi, MAPPN_user_rsn)) {
            MTU_disp_err_val(msg->ss7dialogueID, "MAP-U-ABORT-Ind received with user reason = ",
                              msg->user_reason,GetMAPUserReasonDescr(msg->user_reason));
            sprintf(buf,"MAP-U-ABORT-Ind received with user reason =0x%04x (%s)",msg->user_reason,
                    GetMAPUserReasonDescr(msg->user_reason));
            ss7RequestMap.at(msg->ss7dialogueID).error=buf;
          }
          else {
            MTU_disp_err(msg->ss7dialogueID,"MAP-U-ABORT-Ind received");
            ss7RequestMap.at(msg->ss7dialogueID).error="MAP-U-ABORT-Ind received";
           }
        }
        else
        {
          if (bit_test(msg->pi, MAPPN_prov_rsn)) {
            MTU_disp_err_val(msg->ss7dialogueID,"MAP-P-ABORT-Ind received with provider reason = ",
                             msg->prov_reason,GetMAPProviderReasonDescr(msg->prov_reason));
            sprintf(buf,"MAP-P-ABORT-Ind received with provider reason =0x%04x (%s)",msg->prov_reason,
                    GetMAPProviderReasonDescr(msg->prov_reason));
            ss7RequestMap.at(msg->ss7dialogueID).error=buf;
          }
          else {
            MTU_disp_err(msg->ss7dialogueID,"MAP-P-ABORT-Ind received");
            ss7RequestMap.at(msg->ss7dialogueID).error="MAP-P-ABORT-Ind received";
          }
        }
//        MTU_release_invoke_id(dlg->invoke_id, dlg);
//        MTU_release_request(msg->dlg_id);
        ss7RequestMap.at(msg->ss7dialogueID).state=rs_finished;
        time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);
        break;

      case MAPDT_OPEN_CNF:
        if (msg->result == MAPRS_DLG_ACC)
        {
          /*
           * Dialogue has been accepted. Change state to wait for the
           * MAP-FORWARD-SHORT-MESSAGE-Cnf which indicates whether or
           * not the short message was delivered successfully.
           */
          ss7RequestMap.at(msg->ss7dialogueID).state = rs_wait_serv_cnf;
          time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);
        }
        else
        {
          /*
           * Report the error, release the invoke ID, release the dialogue ID,
           * and idle the state machine.
           */
          if (bit_test(msg->pi, MAPPN_refuse_rsn)) {
            MTU_disp_err_val(msg->ss7dialogueID,"dialogue refused with reason: %x ",
                             msg->refuse_reason, GetMAPRefuseReasonDescr(msg->refuse_reason));
            sprintf(buf,"Dialogue refused with reason: 0x%04x (%s)",msg->refuse_reason,GetMAPRefuseReasonDescr(msg->refuse_reason));
            ss7RequestMap.at(msg->ss7dialogueID).error=buf;
          }
          else {
            MTU_disp_err(msg->ss7dialogueID,"dialogue refused");
            ss7RequestMap.at(msg->ss7dialogueID).error="Dialogue refused";
          }
//          MTU_release_invoke_id(dlg->invoke_id, dlg);
//          MTU_release_dlg_id(msg->dlg_id);
            MTU_release_request(msg->ss7dialogueID);
          ss7RequestMap.at(msg->ss7dialogueID).state=rs_finished;
          time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);
        }
        break;

      default:
        MTU_disp_err_val(msg->ss7dialogueID,"Unexpected dialogue primitive received: ",msg->type, "");
        break;
    }
  }
  else
  {
      MTU_disp_err_val(msg->ss7dialogueID,"Unexpected service primitive received: ", msg->type, "");
  }

  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_wait_open_cnf");
      return -999;
  }
} /* end of MTU_wait_open_cnf() */

void Get32bitPartsOfStr(char* value,u32* part1,u32* part2,u32* part3,u32* part4)
{
    try {
    char temp_char=value[8];
    value[8]='\0';  // temporarily end string to process conversion to u32 number
    *part1=strtoul(value,NULL,16);
    value[8]=temp_char;  // recover XRES string

    temp_char=value[16];
    value[16]='\0';
    *part2=strtoul(value+8,NULL,16);
    value[16]=temp_char;

    temp_char=value[24];
    value[24]='\0';
    *part3=strtoul(value+16,NULL,16);
    value[24]=temp_char;

    *part4=strtoul(value+24,NULL,16);
    }
    catch(...) {
        log("Exception caught in Get32bitPartsOfStr");
    }
}


AuthAttributeType TranslateMessageParam(u16 param)
{
    AuthAttributeType authInfoType = UNKNOWN;
    switch(param) {
    case MAPPN_RAND1:
    case MAPPN_RAND2:
    case MAPPN_RAND3:
    case MAPPN_RAND4:
    case MAPPN_RAND5:
        authInfoType = RAND;
        break;
    case MAPPN_KC1:
    case MAPPN_KC2:
    case MAPPN_KC3:
    case MAPPN_KC4:
    case MAPPN_KC5:
        authInfoType = KC;
        break;
    case MAPPN_SRES1:
    case MAPPN_SRES2:
    case MAPPN_SRES3:
    case MAPPN_SRES4:
    case MAPPN_SRES5:
        authInfoType = SRES;
        break;
    case MAPPN_XRES1:
    case MAPPN_XRES2:
    case MAPPN_XRES3:
    case MAPPN_XRES4:
    case MAPPN_XRES5:
        authInfoType = XRES;
        break;
    case MAPPN_CK1:
    case MAPPN_CK2:
    case MAPPN_CK3:
    case MAPPN_CK4:
    case MAPPN_CK5:
        authInfoType = CK;
        break;
    case MAPPN_IK1:
    case MAPPN_IK2:
    case MAPPN_IK3:
    case MAPPN_IK4:
    case MAPPN_IK5:
        authInfoType = IK;
        break;
    case MAPPN_AUTN1:
    case MAPPN_AUTN2:
    case MAPPN_AUTN3:
    case MAPPN_AUTN4:
    case MAPPN_AUTN5:
        authInfoType = AUTN;
        break;
    }
    return authInfoType;
}


int ParseAuthInfo(u16 dlg_id,MSG* msg)
{
    u8 *pptr;
    u16 param;
    u8 *p1;
    int mlen,plen;
    char value[100];

    try {
        if ((mlen = msg->len) == 0) {
            return 0;
        }
        if (mlen > MTU_MAX_PARAM_LEN)
          mlen = MTU_MAX_PARAM_LEN;
        pptr = get_param(msg);
        p1=pptr;
        SS7_REQUEST& ss7Req = ss7RequestMap.at(dlg_id);

        if(*p1 != MTU_SEND_AUTH_INFO_RSP)
            return -1;
        p1++;
        if(*p1 == MAPPN_INVOKE_ID)
            p1 += *(p1+1)+2;
        if(p1>=pptr+mlen) return -1;

        while(p1 < pptr+mlen-1) {
            param=*p1;
            if(param==0xf0) {
                // param name is 2-byte
                p1+=2;
                param=*p1<<8 | *(p1+1);
                p1++;
            }
            value[0]=0;

            AuthAttributeType authInfoType = TranslateMessageParam(param);
            if (authInfoType != UNKNOWN) {
               plen = *(p1+1);
               p1 += 2;
               ss7Req.AddAuthAttribute(authInfoType, AuthInfoAttribute(p1, plen));
               if (ss7Req.EnoughVectorsReceived())  {
                   break;
               }

               for(int i=0; i<plen; i++, p1++)
                   sprintf(value,"%s%c%c", value, BIN2CH(*p1/16), BIN2CH(*p1%16));
           }
           else {
               // skip unknown params
                p1 += *(p1+1)+2;
                continue;
           }
        }

        if (ss7Req.EnoughVectorsReceived())  {
            ss7Req.SetSuccess(true);
        }
        log("Requested %d vectors, received %d", ss7Req.requestedVectorsNum, ss7Req.receivedVectorsNum);
        return 0;
    }
    catch(...) {
        log("Exception caught in ParseTriplets");
        return -999;
    }
}

/*
 * MTU_wait_serv_cnf handles messages received in the wait for mobile
 * terminated MTU confirmation state.
 *
 * Always returns zero.
 */
int MTU_wait_serv_cnf(MSG* m,MTU_MSG* msg/*,MTU_DLG* dlg*/)
  /*MTU_MSG *msg;          received message */
  /*MTU_DLG *dlg;          dialogue data structure */
{
  char buf[1024];

  try {
  if (msg->dlg_prim)
  {
    switch (msg->type)
    {
      case MAPDT_U_ABORT_IND:
      case MAPDT_P_ABORT_IND:
        /*
         * Dialogue aborted. Release the invoke ID, release the dialogue
         * ID, and idle the state machine.
         */
        if (msg->type == MAPDT_U_ABORT_IND)
        {
          if (bit_test(msg->pi, MAPPN_user_rsn)) {
            MTU_disp_err_val(msg->ss7dialogueID,"MAP-U-ABORT-Ind received with user reason = ",
                             msg->user_reason, GetMAPUserReasonDescr(msg->user_reason));
            sprintf(buf,"MAP-U-ABORT-Ind received with user reason =0x%04x (%s)",msg->user_reason,GetMAPUserReasonDescr(msg->user_reason));
            ss7RequestMap.at(msg->ss7dialogueID).error=buf;
          }
          else {
            MTU_disp_err(msg->ss7dialogueID,"MAP-U-ABORT-Ind received");
            ss7RequestMap.at(msg->ss7dialogueID).error="MAP-U-ABORT-Ind received";
          }
        }
        else
        {
          if (bit_test(msg->pi, MAPPN_prov_rsn)) {
            MTU_disp_err_val(msg->ss7dialogueID,"MAP-P-ABORT-Ind received with provider reason = ",
                             msg->prov_reason, GetMAPProviderReasonDescr(msg->prov_reason));
            sprintf(buf,"MAP-P-ABORT-Ind received with provider reason =0x%04x (%s)",msg->prov_reason,GetMAPProviderReasonDescr(msg->prov_reason));
            ss7RequestMap.at(msg->ss7dialogueID).error=buf;
          }
          else {
            MTU_disp_err(msg->ss7dialogueID,"MAP-P-ABORT-Ind received");
            ss7RequestMap.at(msg->ss7dialogueID).error="MAP-U-ABORT-Ind received";
          }

        }
//        MTU_release_invoke_id(dlg->invoke_id, dlg);
//        MTU_release_dlg_id(msg->dlg_id);
//        MTU_release_request(msg->dlg_id);
        ss7RequestMap.at(msg->ss7dialogueID).state=rs_finished;
        time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);

        break;

      case MAPDT_NOTICE_IND:
        /*
         * MAP-NOTICE-IND indicates some kind of error. Close the dialogue
         * using MAP-U-ABORT-REQ, release the invoke ID, release the dialogue
         * ID, and idle the state machine.
         */
        if (bit_test(msg->pi, MAPPN_prob_diag)) {
          MTU_disp_err_val(msg->ss7dialogueID,"MAP-NOTICE-Ind received with problem diagnostic = ",
                           msg->prob_diag,GetMAPProblemDiagnosticDescr(msg->prob_diag));
          sprintf(buf,"MAP-NOTICE-Ind received with problem diagnostic =0x%04x (%s)",msg->prob_diag,GetMAPProblemDiagnosticDescr(msg->prob_diag));
          ss7RequestMap.at(msg->ss7dialogueID).error=buf;
        }
        else {
          MTU_disp_err(msg->ss7dialogueID,"MAP-NOTICE-Ind received");
          ss7RequestMap.at(msg->ss7dialogueID).error="MAP-NOTICE-Ind received";
        }


        /*
         * Send MAP-U_ABORT-Req containing the following parameters:
         *      release method
         */
        msg->type = MAPDT_U_ABORT_REQ;
        memset((void *)msg->pi, 0, PI_BYTES);

        bit_set(msg->pi, MAPPN_user_rsn);
        msg->user_reason = MAPUR_unspecified_reason;
        MTU_send_dlg_req(msg);

        ss7RequestMap.at(msg->ss7dialogueID).state=rs_finished;
        time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);

        break;
      case MAPDT_DELIMITER_IND:
        /* 
         * USSD response - once the delimiter is received we can send the UnstructuredSSRequestRSP message 
         */ 
 //       MTU_Send_UnstructuredSSRequestRSP (msg->dlg_id, dlg->invoke_id);

// EXPERIMENTAL        MTU_send_close_request(msg->dlg_id);
        MTU_send_Delimit(msg->ss7dialogueID);

        break;
    default:
        MTU_disp_err_val(msg->ss7dialogueID,"Unexpected dialogue primitive received: ", msg->type, "");
        break;
    }
  }
  else
  {
    /*
     * Check that this message is related to the current invocation.
     */
    if (msg->ss7invokeID == ss7RequestMap.at(msg->ss7dialogueID).ss7invokeID)
    {
      switch (msg->type)
      {
        case MAPST_FWD_SM_CNF:
        case MAPST_SEND_IMSI_CNF:
        case MAPST_SND_RTIGPRS_CNF:
        case MAPST_MT_FWD_SM_CNF:
        case MAPST_SND_RTISM_CNF:
        case MAPST_PRO_UNSTR_SS_REQ_CNF:
        case MAPST_SEND_AUTH_INFO_CNF:
          /*
           * Confirmation to service request received. Release the invoke ID.
           */
//          MTU_release_invoke_id(dlg->invoke_id, dlg);

          if (bit_test(msg->pi, MAPPN_prov_err))
          {
            /*
             * A provider error parameter is included indicating an error.
             * Send a MAP-U_ABORT-Req and release the dialogue ID.
             */
//            MTU_disp_err("Service primitive cnf received with");
            MTU_disp_err_val(msg->ss7dialogueID,"Service primitive cnf received with provider error = ", msg->prov_err,GetMAPProviderErrorDescr(msg->prov_err));
            sprintf(buf,"MAP provider error=0x%04x (%s)",msg->prov_err,GetMAPProviderErrorDescr(msg->prov_err));
            ss7RequestMap.at(msg->ss7dialogueID).error=buf;

            msg->type = MAPDT_U_ABORT_REQ;
            memset((void *)msg->pi, 0, PI_BYTES);

            bit_set(msg->pi, MAPPN_user_rsn);
            msg->user_reason = MAPUR_unspecified_reason;
            MTU_send_dlg_req(msg);

            ss7RequestMap.at(msg->ss7dialogueID).state=rs_finished;
            time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);
          }
          else
          {
            if (bit_test(msg->pi, MAPPN_user_err))
            {
              // MTU_disp_err("Service primitive cnf received with");
              MTU_disp_err_val(msg->ss7dialogueID,"Service primitive cnf received with user error = ", msg->user_err,
                               GetMAPUserErrorDescr(msg->user_err));
              sprintf(buf,"MAP user error=0x%04x (%s)",msg->user_err,GetMAPUserErrorDescr(msg->user_err));
              ss7RequestMap.at(msg->ss7dialogueID).error=buf;
            }
            else
                ParseAuthInfo(msg->ss7dialogueID,m);

            /*
             * If the IMSI is present, save it in case it is needed for a
             * subsequent dialogue.
             */
            if (bit_test(msg->pi, MAPPN_imsi))
              ss7RequestMap.at(msg->ss7dialogueID).bcdIMSI = msg->imsi;

            //dlg->state = MTU_WAIT_CLOSE_IND;
            ss7RequestMap.at(msg->ss7dialogueID).state = rs_wait_close_ind;
            time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);
          }
          break;
        case MAPST_UNSTR_SS_REQ_IND:
          {
             ss7RequestMap.at(msg->ss7dialogueID).state = rs_wait_serv_cnf;
             time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);
          }
          break;
        default:
          MTU_disp_err_val(msg->ss7dialogueID,"Unexpected service primitive received: ", msg->type, "");
          break;
      }
    }
    else
    {
      MTU_disp_err_val(msg->ss7dialogueID,"Received service primitive with unexpected invoke ID: ",
                       msg->ss7invokeID, "");
    }
  }

  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_wait_serv_cnf");
      return -999;
  }
} /* end of MTU_wait_serv_cnf() */

/*
 * MTU_wait_close_ind handles messages received in the wait for close indication
 * state.
 *
 * Always returns zero.
 */
int MTU_wait_close_ind(MTU_MSG* msg/*,MTU_DLG* dlg*/)
  /*MTU_MSG *msg;          received message */
  /*MTU_DLG *dlg;          dialogue data structure */
{
  char buf[1024];

  try {
  if (msg->dlg_prim)
  {
    switch (msg->type)
    {
      case MAPDT_CLOSE_IND:

         /* Save the IMSI if necessary for later use and then release the
         * dialogue ID and idle the state machine.
         */
//        if (gwOptions.mode == MTU_SI_SRIGPRS)
//          imsi = dlg->imsi;
//       MTU_release_dlg_id(msg->dlg_id);
        //MTU_release_request(msg->dlg_id);


        /*
         * For the mode where Send IMSI and Send routing info
         * for GPRS are used alternately, and this dialogue used Send IMSI,
         * open a new dialogue using Send routing info for GPRS. Only do this
         * if the IMSI was received.
         */
//        if ((gwOptions.mode == MTU_SI_SRIGPRS) &&
//            (dlg->service == MTU_SEND_IMSI) &&
//            (imsi.num_bytes > 0))
//        {
//          if (MTU_open_dlg(MTU_SRI_GPRS, &imsi) != 0)
//            MTU_disp_err("Failed to open dialogue");
//        }
//        else

          ss7RequestMap.at(msg->ss7dialogueID).state=rs_finished;
          time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);
        break;

      case MAPDT_U_ABORT_IND:
      case MAPDT_P_ABORT_IND:
        /*
         * Dialogue aborted. Release the invoke ID, release the dialogue
         * ID, and idle the state machine.
         */

        if (msg->type == MAPDT_U_ABORT_IND)
        {
          if (bit_test(msg->pi, MAPPN_user_rsn)) {
            MTU_disp_err_val(msg->ss7dialogueID,"MAP-U-ABORT-Ind received with user reason = ",
                             msg->user_reason,GetMAPUserReasonDescr(msg->user_reason));
            sprintf(buf,"MAP-U-ABORT-Ind received with user reason = 0x%04x (%s)",msg->user_reason,GetMAPUserReasonDescr(msg->user_reason));
            ss7RequestMap.at(msg->ss7dialogueID).error=buf;
          }
          else {
            MTU_disp_err(msg->ss7dialogueID,"MAP-U-ABORT-Ind received");
            ss7RequestMap.at(msg->ss7dialogueID).error="MAP-U-ABORT-Ind received";
          }
        }
        else
        {
          if (bit_test(msg->pi, MAPPN_prov_rsn)) {
            MTU_disp_err_val(msg->ss7dialogueID,"MAP-P-ABORT-Ind received with provider reason = ",
                             msg->prov_reason,GetMAPProviderReasonDescr(msg->prov_reason));
            sprintf(buf,"MAP-P-ABORT-Ind received with provider reason = 0x%04x (%s)",msg->prov_reason,GetMAPProviderReasonDescr(msg->prov_reason));
            ss7RequestMap.at(msg->ss7dialogueID).error=buf;
          }
          else {
            MTU_disp_err(msg->ss7dialogueID,"MAP-P-ABORT-Ind received");
            ss7RequestMap.at(msg->ss7dialogueID).error="MAP-U-ABORT-Ind received";
          }
        }
        ss7RequestMap.at(msg->ss7dialogueID).state = rs_finished;
        time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);
        break;

      case MAPDT_DELIMITER_IND:
        // repeat service request for extra vectors, increasing invoke_id
        ss7RequestMap.at(msg->ss7dialogueID).ss7invokeID++;
        MTU_send_auth_info(msg->ss7dialogueID,false);
        MTU_send_Delimit(msg->ss7dialogueID);
        ss7RequestMap.at(msg->ss7dialogueID).state = rs_wait_serv_cnf;
        time(&ss7RequestMap.at(msg->ss7dialogueID).stateChangeTime);
        break;

      default:
        MTU_disp_err_val(msg->ss7dialogueID,"Unexpected dialogue primitive received: ", msg->type, "");
        break;
    }
  }
  else
  {
      MTU_disp_err_val(msg->ss7dialogueID,"Unexpected service primitive received: ", msg->type, "");
  }

  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_wait_close_ind");
      return -999;
  }
} /* end of MTU_wait_close_ind() */


/*
 * MTU_smac is Short Message Service state machine. It is entered after the
 * dialogue with the servicing MSC has been opened and the short message has
 * been forwarded.
 *
 * Always returns zero.
 */
int MTU_smac(MSG* m)
  /*MSG *m;        received message */
{
  MTU_MSG ind;          /* structured form of received message */

  try {
  /*
   * Recover the parameters from the MSG into a 'C' structure
   */
  if (MTU_msg_to_ind(&ind, m) != 0)
  {
    MTU_disp_err(m->hdr.id,"Primitive indication recovery failure");
    return 0;
  }

    if (ss7RequestMap.find(ind.ss7dialogueID) == ss7RequestMap.end())
    {
        MTU_disp_err_val(m->hdr.id,"Unexpected dialogue ID=0x", ind.ss7dialogueID, "");
      return 0;
    }
//        dlg = MTU_get_dlg_data(ind.dlg_id);

      /*
       * Handle the event according to the current state.
       */
      switch (ss7RequestMap.at(ind.ss7dialogueID).state /*dlg->state*/)
      {
        case rs_wait_opn_cnf:
          MTU_wait_open_cnf(&ind/*, ind.dlg_id*/);
          break;

        case rs_wait_serv_cnf:
          MTU_wait_serv_cnf(m,&ind/*, dlg*/);
          break;

        case rs_wait_close_ind:
          MTU_wait_close_ind(&ind/*, dlg*/);
          break;

        case rs_idle:
        default:
          MTU_disp_err(ind.ss7dialogueID,"Message received for inactive dialogue");
          break;
      }

      if(ss7RequestMap.at(ind.ss7dialogueID).state == rs_finished) {
          OnDialogueFinish(ind.ss7dialogueID);
      }
  return(0);
  }
  catch(...) {
      log("Exception caught in MTU_smac");
      return -999;
  }
} /* end of MTU_smac() */





/*
 * MTU_disp_err
 *
 * Traces internal progress to the console.
 *
 * Always returns zero.
 */
int MTU_disp_err(u16 dlg_id,const char* text)
  /*char *text;    Text for tracing progress of program */
{
  if (gwOptions.log_options & MTU_TRACE_ERROR)
    fprintf(stderr, "*** 0x%04x %s ***", dlg_id, text);

  return(0);
}

/*
 * MTU_disp_err_val
 *
 * Traces internal progress to the console.
 *
 * Always returns zero.
 */
int MTU_disp_err_val(u16 dlg_id,const char* text,u16 value,const char* descr)
  /*char *text;    Text for tracing progress of program */
  /*u16 value;     Value to be displayed */
{
  if (gwOptions.log_options & MTU_TRACE_ERROR)
    log("*** 0x%04x %s%04x (%s)***", dlg_id,text, value, descr);

  return(0);
}


  /*
   * mtu_ent opens a dialogue with the servicing MSC and forwards the short
   * message. It then waits for messages and processes them appropriately
   * until a MAP-CLOSE-IND is received or an error occurs.
   *
   * Always returns zero.
   */
  int mtu_ent(char* imsi,u16 dlg_id)
    /*CL_ARGS *cl_args;      structure containing all command-line arguments */
  {
    HDR *h;               /* received message */
    MSG *m;               /* received message */
    //MTU_BCDSTR imsi;      /* IMSI */

    /*
     * Initialise static data

    mtu_args.mtu_mod_id = cl_args->mtu_mod_id;
    mtu_args.mtu_map_id = cl_args->mtu_map_id;
    mtu_args.map_version = cl_args->map_version;
    mtu_args.mode = cl_args->mode;
    mtu_args.options = cl_args->options;
    mtu_args.base_dlg_id = cl_args->base_dlg_id;
    mtu_args.num_dlg_ids = cl_args->num_dlg_ids;
    mtu_args.max_active = cl_args->max_active;
    strcpy(mtu_args.imsi,cl_args->imsi);
    strcpy(mtu_args.service_centre, cl_args->service_centre);
    strcpy(mtu_args.dest_address, cl_args->dest_address);
    strcpy(mtu_args.orig_address , cl_args->orig_address);
    mtu_args.message = cl_args->message;
    mtu_args.msisdn = cl_args->msisdn;
    mtu_args.ggsn_number = cl_args->ggsn_number;
    mtu_args.ussd_string = cl_args->ussd_string;
    mtu_args.triplets_num=cl_args->triplets_num;
    mtu_args.gtt_table_size = cl_args->gtt_table_size;
    mtu_args.MAP_timeout = cl_args->MAP_timeout;
*/
    //mtu_dlg_id = gwOptions.base_dlg_id;
    mtu_active = 0;
    mtu_dlg_count = 0;


    //imsi.num_bytes = 0;

    /*
     * Make sure all the dialogues are idle to start with
     */
//    for (i=0; i<gwOptions.num_dlg_ids; i++)
//      dlg_data[i].state = MTU_IDLE;
//    int res;
//    if((res=GCT_cong_status(0)) != 0) {
//        // dialogic stack in congestion, return ERROR
//        //....
//        // ....
//        fprintf(stderr,"Dialogic stack in congestion\n");
//        return -1;
//    }


    /*
     * If multiple dialogues have been requested, open dialogues up to the
     * maximum that can be simultaneously active and keep on opening new
     * dialogues as they are closed. Otherwise, just open one dialogue.
     */

    if (gwOptions.max_active == 0)
    {
      /*
       * Only a single dialogue was requested
       */


      if (MTU_open_dlg(dlg_id,imsi) != 0)
        MTU_disp_err(0,"Failed to open dialogue");

      int finished = 0;
      while (!finished)
      {
        /*
         * Receive messages as they become available and
         * process accordingly.
         *
         */
        if ((h = GCT_grab(gwOptions.mtu_mod_id)) != 0)
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
              MTU_disp_err(0,"Unexpected message type");
              break;
          }
          /*
           * Once we have finished processing the message
           * it must be released to the pool of messages.
           */
          relm(h);
        }
      } /* end while */
    }
    else
    {
      /*
       * Multiple dialogues
       */
      while (1)
      {
        if (mtu_active < gwOptions.max_active)
        {
          if (MTU_open_dlg(dlg_id,imsi) != 0)
            MTU_disp_err(0,"Failed to open dialogue");
        }

        /*
         * Receive messages as they become available and
         * process accordingly.
         *
         * GCT_receive will attempt to receive messages
         * from the task's message queue and block until
         * a message is ready.
         */
        if ((h = GCT_grab(gwOptions.mtu_mod_id)) != 0)
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
               * else but if we do, report the messages.
               */
              if (gwOptions.log_options & MTU_TRACE_RX)
                MTU_display_recvd_msg(m);
              MTU_disp_err(0,"Unexpected message type");
              break;
          }

          /*
           * Once we have finished processing the message
           * it must be released to the pool of messages.
           */
          relm(h);
        }
      } /* end while */
    }
    return(0);
  } /* end of mtu_ent() */
