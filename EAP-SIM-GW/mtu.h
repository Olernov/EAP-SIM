/*
                Copyright (C) Dialogic Corporation 1997-2011. All Rights Reserved.

 Name:          mtu.h

 Description:   Contains macro and structure definitions used in MTU.

 -----  ---------  ---------------------------------------------
 Issue    Date                      Changes
 -----  ---------  ---------------------------------------------

   A    23-Sep-97  - Initial code.
   B    20-Mar-98  - Added version 1 application functionality.
   C    13-Jan-99  - Added orignating SME address option (-o) to
                     structure SH_MSG
   1    16-Feb-00  - Now includes ss7_inc.h.
   2    10-Aug-01  - Added support for Send-IMSI and Send routing info
                     for GPRS. Also ability to generate multiple
                     dialogues.
   3    20-Jan-06  - Include reference to Intel Corporation in file header
   4    21-Aug-06  - Replaced tabs with spaces.
   5    13-Dec-06  - Change to use of Dialogic Corporation copyright.
   6    30-Sep-09  - Support for additional MAP services, e.g. USSD.
   7    19-Aug-11  - Modify PI_BYTES structure to scale with new 
                     MAP parameters.
 */
#pragma once
#define LINT_ARGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <iostream>
#include <map>
#ifdef WIN32
    #include <Winsock2.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif
extern "C" {
#include "system.h"     /* system type definitions (u8, u16 etc) */
#include "msg.h"        /* basic message type definitions (MSG etc) */
#include "sysgct.h"     /* prototypes for GCT_xxx functions */
#include "bit2byte.h"
#include "asciibin.h"
#include "strtonum.h"
#include "ss7_inc.h"
#include "map_inc.h"
#include "pack.h"
}


#define MAX_ADDR_LEN      (36)    /* Max length of SCCP address */
#define MAX_DATA_LEN      (100)   /* Max length of short message */
#define MAX_BCDSTR_LEN    (10)    /* Max size of BCD octet string */
#define MAX_ADDSTR_LEN    (8)     /* Max size used to store addr digits */
#define BASE_DLG_ID       (0x0)   /* Base MAP dialogue id */
#define NUM_OF_DLG_IDS    (0x1000) /* Number of OG dialogue ids */
#define DEFAULT_INVOKE_ID (1)   /* Invoke id to use in all invokes */
#define AC_LEN            (9)     /* Application context length */
#define PI_BYTES          ((MAPPN_end/8)+1) /* Size of bit field param indicator */

/*
 * MAP versions
 */
#define MTU_MAPV1               (1)
#define MTU_MAPV2               (2)
#define MTU_MAPV3               (3)

/*
 * Maximum parameter length in an MSG:
 */
#define MTU_MAX_PARAM_LEN       (320)

#define NO_RESPONSE             (0)

#define MAX_GTT_TABLE_SIZE      200

#define MAX_ALLOWED_CLIENT_IPS  200

#define SUBSYSTEM_HLR           06
#define SUBSYSTEM_VLR           07

/*
 * Mode. Determines which service requests will be sent in the dialogue
 */
#define MTU_FORWARD_SM          (0)        /* Forward short message */
#define MTU_SEND_IMSI           (1)        /* Send IMSI */
#define MTU_SRI_GPRS            (2)        /* Send routing info for GPRS */
#define MTU_SI_SRIGPRS          (3)        /* alternates: Send IMSI & Send routing info for GPRS */
#define MTU_MT_FORWARD_SM       (4)            /* Forward MT short message */
#define MTU_SRI_SM              (5)        /* Send Routing Information for Short Message */
#define MTU_PROCESS_USS_REQ     (6)        /* Send MAP-ProcessUnstructuredSS-Request */ 
#define MTU_SEND_AUTH_INFO      (7)         /* Send-Authentification-Info */

#define MTU_SEND_AUTH_INFO_RSP  0xc2
#define MAPPN_INVOKE_ID     0x0e
#define MAPPN_RAND1         0x54
#define MAPPN_RAND2         0x55
#define MAPPN_RAND3         0x56
#define MAPPN_RAND4         0x57
#define MAPPN_RAND5         0x58
#define MAPPN_KC1           0x5e
#define MAPPN_KC2           0x5f
#define MAPPN_KC3           0x60
#define MAPPN_KC4           0x61
#define MAPPN_KC5           0x62
#define MAPPN_SRES1         0x59
#define MAPPN_SRES2         0x5a
#define MAPPN_SRES3         0x5b
#define MAPPN_SRES4         0x5c
#define MAPPN_SRES5         0x5d
#define MAPPN_XRES1         0x129
#define MAPPN_XRES2         0x12a
#define MAPPN_XRES3         0x12b
#define MAPPN_XRES4         0x12c
#define MAPPN_XRES5         0x12d
#define MAPPN_CK1           0x12e
#define MAPPN_CK2           0x12f
#define MAPPN_CK3           0x130
#define MAPPN_CK4           0x131
#define MAPPN_CK5           0x132
#define MAPPN_IK1           0x133
#define MAPPN_IK2           0x134
#define MAPPN_IK3           0x135
#define MAPPN_IK4           0x136
#define MAPPN_IK5           0x137
#define MAPPN_AUTN1         0x138
#define MAPPN_AUTN2         0x139
#define MAPPN_AUTN3         0x13a
#define MAPPN_AUTN4         0x13b
#define MAPPN_AUTN5         0x13c
/*
 * Display options
 */
#define MTU_TRACE_TX            (0x0001)        /* Trace messages sent */
#define MTU_TRACE_RX            (0x0002)        /* Trace messages received */
#define MTU_TRACE_ERROR         (0x0004)        /* Trace errors */
#define MTU_TRACE_STATS         (0x0008)        /* Trace statistics */

/*
 * Macro for message display procedures
 */
#define BIN2CH(b)               ((char)(((b)<10)?'0'+(b):'a'-10+(b)))


#define MIN_VECTORS_NUM  (1)
#define MAX_VECTORS_NUM  (5)

/*
 * Structure used for origination and destination addresses
 */
typedef struct
{
  u8 num_bytes;                 /* number of digits */
  u8 data[MAX_ADDR_LEN];        /* contains the digits */
} MTU_ADDR;

/*
 * Structure used for parameters of format TBCD_STRING
 */
typedef struct
{
  u8 num_bytes;                 /* number of bytes of data */
  u8 data[MAX_BCDSTR_LEN];      /* contains the data */
} MTU_BCDSTR;

/*
 * Structure used for parameters of format AddressString
 */
typedef struct
{
  u8 num_dig_bytes;             /* number of bytes of digits */
  u8 noa;                       /* nature of address */
  u8 npi;                       /* numbering plan indicator */
  u8 digits[MAX_ADDSTR_LEN];    /* contains the digits */
} MTU_ADDSTR;

/*
 * Structure used for parameters of format SM-RP-OA
 */
typedef struct
{
  u8 toa;                       /* type of address */
  u8 num_dig_bytes;             /* number of bytes of digits */
  u8 noa;                       /* nature of address */
  u8 npi;                       /* numbering plan indicator */
  u8 digits[MAX_ADDSTR_LEN];    /* contains the digits */
} MTU_OASTR;

/*
 * Structure used for parameters of format SignalInfo
 */
typedef struct
{
  u8 num_bytes;                 /* number of digits in the string */
  u8 data[MAX_DATA_LEN];        /* contains the digits */
} MTU_SIGINFO;


/*
 * Structure containing all command-line parameters
 */
typedef struct
{
  u8 mtu_mod_id;                /* the module ID of this module */
  u8 mtu_map_id;                /* the module ID of the MAP module */
  u8 map_version;               /* MAP version */
  u8 mode;                      /* type of service primitives to send */
  u16 log_options;                  /* display options */
  u16 base_dlg_id;              /* the base dlg id to use */
  u16 num_dlg_ids;              /* the number of dlg ids */
  u16 max_active;               /* number of active dialogues to maintain */
  u8 MAP_timeout;
  u16 IP_port;
  u16 max_connections;
  u8 allowed_clients_num;
  char allowed_IP[MAX_ALLOWED_CLIENT_IPS][40];
  char service_centre[40];         /* service centre */
  char log_path[255];           /*log files path*/
  u8 gtt_table_size;            /*size of array gttTable - global title translation from ini-file*/
  int queue_check_period;
} GW_OPTIONS;



/*
 * Structure containing a dialogue message to be sent or received.
 */
typedef struct
{
  u8 pi[PI_BYTES];              /* Bit field indicating parameters present */
  u16 ss7dialogueID;                   /* dialogue ID */
  u8 ss7invokeID;                 /* invoke ID */
  u8 type;                      /* primitive type */
  u8 dlg_prim;                  /* indicates this is a dialogue primitive */
  u16 timeout;                  /* operaton timeout */
  MAP_REL_METHOD release_method;  /* release method */
  MAP_RESULT result;            /* result */
  MAP_REF_RSN refuse_reason;    /* refuse reason */
  MAP_USER_RSN user_reason;     /* user reason */
  MAP_PROV_RSN prov_reason;     /* provider reason */
  MAP_PROB_DIAG prob_diag;      /* problem diagnostic */
  MAP_PROV_ERR prov_err;        /* provider error */
  MAP_USER_ERR user_err;        /* user error */
  u8 applic_context[AC_LEN];    /* application context */
  MTU_ADDR dest_address;        /* destination address */
  MTU_ADDR orig_address;        /* origination address */
  MTU_BCDSTR sm_rp_da;          /* relay protocol destination address */
  MTU_OASTR sm_rp_oa;           /* relay protocol origination address */
  MTU_SIGINFO sm_rp_ui;         /* user information - contains the short message */
  MTU_ADDSTR msisdn;            /* msisdn */
  MTU_BCDSTR imsi;              /* IMSI */
  MTU_ADDSTR ggsn_num;          /* GGSN number */
  MAP_MORE_MSGS more_msgs;      /* MAP more messages */
  MTU_ADDSTR sc_addr;           /* service centre address */
  MAP_SM_RP_PRI sm_rp_pri;      /* Short Message delivery priority */
  MTU_SIGINFO ussd_coding;      /* USSD coding */
  MTU_SIGINFO ussd_string;      /* USSD string */ 
  u8 nb_req_vect;               /* number of requested vectors */
} MTU_MSG;

/*
 * Dialogue data structure
 */
typedef struct
{
  u8 ss7invokeID;                 /* Invoke ID for service request */
  u8 state;                     /* state machine state */
  u8 service;                   /* service used in this dialogue */
  MTU_BCDSTR imsi;              /* IMSI */
} MTU_DLG;




enum REQUEST_STATE {
    rs_idle,
    rs_wait_opn_cnf,
    rs_wait_serv_cnf,
    rs_wait_close_ind,
    rs_send_response,
    rs_finished
} ;


typedef struct
{
    char name[80];
    char imsi_prefix[MAX_ADDR_LEN];
    u8 numplan;
    char str_numplan[6];
    char ptn_replace[MAX_ADDR_LEN];
    u8 ptn_keep_remove;
    u8 reserve_numplan;
    char str_reserve_numplan[6];
    char reserve_ptn_replace[MAX_ADDR_LEN];
    char reserve_ptn_keep_remove;
} GTT_ENTRY;

/*
 * Function prototypes
 */
int mtu_ent(char*,u16 ss7dialogueID);
int MTU_disp_err(u16 ss7dialogueID, const char *text);
int MTU_disp_err_val(u16 ss7dialogueID,const char *text, u16 value, const char* descr);
int MTU_dlg_req_to_msg(MSG *m, MTU_MSG *dlg_req);
int MTU_msg_to_ind(MTU_MSG *ind, MSG *m);
int MTU_srv_req_to_msg(MSG *m, MTU_MSG *srv_req);
int MTU_get_scts(u8 *time_stamp);
u8  MTU_str_to_def_alph(char *ascii_str, u8 *da_octs, u8 *da_len, u8 max_octs);
u8  MTU_USSD_str_to_def_alph(char *ascii_str, u8 *da_octs, u8 *da_len, u8 max_octs);




