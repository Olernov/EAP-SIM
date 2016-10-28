/*
                Copyright (C) Dialogic Corporation 1997-2011. All Rights Reserved.

 Name:          mtu_fmt.c

 Description:   Contains functions used in formatting and recovery of
                messages.

 Functions:     MTU_dlg_req_to_msg      MTU_srv_req_to_msg
                MTU_msg_to_ind          MTU_str_to_def_alph
                MTU_USSD_str_to_def_alph

 -----  ---------   ---------------------------------------------
 Issue    Date                      Changes
 -----  ---------   ---------------------------------------------

   A    23-Sep-97   - Initial code.
   B    11-Nov-98   - Removed MAP_PAR_NAME to match map_inc.h file.
   C    13-Jan-99   - Added support for ASCII to Default Alphabet handling
   1    16-Feb-00   - Default Alphabet to ASCII handling deleted (now in
                      MTR).
   2    16-Jul-01   - Added timeout parameter to MAP-FORWARD-SM-Req.
   3    10-Aug-01   - Added support for Send-IMSI and Send routing info
                      for GPRS. Also ability to generate multiple
                      dialogues.
   4    19-Dec-01   - Addition of MAPPN_user_rsn to MTU_dlg_req_to_msg.
                      Correction of MAPPN_release_method handling in
                      the same procedure.
   5    20-Jan-06   - Include reference to Intel Corporation in file header.
   6    21-Aug-06   - Replaced tabs with spaces.
   7    13-Dec-06   - Change to use of Dialogic Corporation copyright.   
   8    30-Sep-09   - Support for additional MAP services, e.g. USSD.
   9    19-Aug-11   - Modify to reject code-shifted parameters.
 */

#include "mtu.h"

/*
 * Special conversions...
 *      ascii  def_alph
 * 0x09 tab -> space
 * 0x24 $   -> $ (=2)
 * 0x27 '   -> ' (=39)
 *
 * Any default alphabet symbols without ascii equivalents are replaced with
 * zero. This will appear as '@' when converted back to ascii.
 */
static u8 ascii_to_default_alphabet[0x80]= {
/*0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 32, 10, 0 , 13, 0 , 0 , 0 , /* 0x00 */
  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 32, 0 , 0 , 0 , 0 , /* 0x10 */
 ' ','!','"','#', 2 ,'%','&', 39,'(',')','*','+',',','-','.','/', /* 0x20 */
 '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?', /* 0x30 */
  0 ,'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O', /* 0x40 */
 'P','Q','R','S','T','U','V','W','X','Y','Z', 0 , 0 , 0 , 0 , 0 , /* 0x50 */
  0 ,'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o', /* 0x60 */
 'p','q','r','s','t','u','v','w','x','y','z', 0 , 0 , 0 , 0 , 0   /* 0x70 */
};

/*
 * Prototypes for local functions
 */
#ifdef LINT_ARGS
static int MTU_recover_param(u8 pname, u8 plen, u8 *pptr, MTU_MSG *ind);
#else
static int MTU_recover_param();
#endif

#define ASCII2DEF(ascii)   (ascii_to_default_alphabet[ascii])
#define DEF2ASCII(def_alp) (default_alphabet_to_ascii[def_alp])

/*
 * SEMIOCTS
 * This macro takes an integer value and converts it into two semi-octets
 * encoded in a u8.
 * N.B. Each semi-octet represents a single decimal digit [0..9].
 *      The most significant semi-octet is encoded in bits 0 to 3 of the u8.
 *      The least significant semi-octet is encoded in bits 4 to 7 of the u8.
 *
 * Taking the remainder after dividing the number by 100 keeps the value in the
 * range [0..99]. So (x%100)/10  gives a value in the range 0x0 to 0x9.
 *
 * Taking the remainder after dividing the number by 10 keeps the value in the
 * range [0..9]. So (x%10) << 4 gives a value in the range 0x00 to 0x90.
 *
 *
 * Example:
 *
 * 98(dec) = 0x62 -> 0x9 + 0x80 = 0x89 (hex, semi-octet encoded)
 */
#define SEMIOCTS(x)        (((x)%100)/10  + ((x)%10 << 4))

extern int MTU_disp_err(char *text);

/*
 * MTU_dlg_req_to_msg formats a Dialogue Request message to send to MAP.
 *
 * Always returns zero.
 */
int MTU_dlg_req_to_msg(MSG* m, MTU_MSG* dlg_req)
{
  int mlen;             /* total param length */
  int i;                /* loop counter */
  u8  *pptr;            /* pointer to parameter area of message */

  pptr = get_param(m);

  /*
   * for type and terminator octet
   */
  mlen = 2;
  *pptr++ = (u8)dlg_req->type;

  /*
   * Result
   */
  if (bit_test(dlg_req->pi, MAPPN_result))
  {
    *pptr++ = MAPPN_result;
    *pptr++ = 1;
    *pptr++ = dlg_req->result;
    mlen += 3;
  }

  /*
   * application context
   */
  if (bit_test(dlg_req->pi, MAPPN_applic_context))
  {
    *pptr++ = MAPPN_applic_context;
    *pptr++ = AC_LEN;
    for (i=0; i<AC_LEN; i++)
    {
      *pptr++ = dlg_req->applic_context[i];
    }
    mlen += AC_LEN + 2;
  }

  if (bit_test(dlg_req->pi, MAPPN_dest_address))
  {
    /*
     * destination address
     */
    *pptr++ = MAPPN_dest_address;
    *pptr++ = dlg_req->dest_address.num_bytes;
    for (i=0; i<dlg_req->dest_address.num_bytes; i++)
    {
      *pptr++ = dlg_req->dest_address.data[i];
    }
    mlen += dlg_req->dest_address.num_bytes + 2;
  }

  if (bit_test(dlg_req->pi, MAPPN_orig_address))
  {
    /*
     * origination address
     */
    *pptr++ = MAPPN_orig_address;
    *pptr++ = dlg_req->orig_address.num_bytes;
    for (i=0; i<dlg_req->orig_address.num_bytes; i++)
    {
      *pptr++ = dlg_req->orig_address.data[i];
    }
    mlen += dlg_req->orig_address.num_bytes + 2;
  }

  /*
   * release method
   */
  if (bit_test(dlg_req->pi, MAPPN_release_method))
  {
    *pptr++ = MAPPN_release_method;
    *pptr++ = 1;
    *pptr++ = dlg_req->release_method;
    mlen += 3;
  }

  /*
   * problem diagnostic
   */
  if (bit_test(dlg_req->pi, MAPPN_prob_diag))
  {
    *pptr++ = MAPPN_prob_diag;
    *pptr++ = 1;
    *pptr++ = dlg_req->prob_diag;
    mlen += 3;
  }
  
  /*
   * User reason
   */
  if (bit_test(dlg_req->pi, MAPPN_user_rsn))
  {
    *pptr++ = MAPPN_user_rsn;
    *pptr++ = 1;
    *pptr++ = dlg_req->user_reason;
    mlen += 3;
  }
  
  /*
   * Set last byte to zero, indicating no more parameters.
   */
  *pptr = 0;

  m->len = (unsigned short)mlen;

  return(0);
} /* end of MTU_dlg_req_to_msg() */

/*
 * MTU_srv_req_to_msg formats a Service Request message to send to MAP.
 *
 * Always returns zero.
 */
int MTU_srv_req_to_msg(MSG* m, MTU_MSG* srv_req)
{
  int   mlen;           /* total param length */
  int   i;              /* loop counter */
  u8    *pptr;          /* pointer to parameter area of message */

  pptr = get_param(m);

  /*
   * Message length is at least two bytes (accounts for primitive type
   * and terminator octet)
   */
  mlen = 2;
  *pptr++ = (u8)srv_req->type;

  /*
   * Invoke ID
   */
  if (bit_test(srv_req->pi, MAPPN_invoke_id))
  {
    *pptr++ = MAPPN_invoke_id;
    *pptr++ = 1;
    *pptr++ = srv_req->ss7invokeID;
    mlen += 3;
  }

  /*
   * SM RP DA
   */
  if (bit_test(srv_req->pi, MAPPN_sm_rp_da))
  {
    *pptr++ = MAPPN_sm_rp_da;
    *pptr++ = srv_req->sm_rp_da.num_bytes;
    for (i=0; i<srv_req->sm_rp_da.num_bytes; i++)
    {
      *pptr++ = srv_req->sm_rp_da.data[i];
    }
    mlen += srv_req->sm_rp_da.num_bytes + 2;
  }

  /*
   * SM RP OA
   */
  if (bit_test(srv_req->pi, MAPPN_sm_rp_oa))
  {
    *pptr++ = MAPPN_sm_rp_oa;
    *pptr++ = srv_req->sm_rp_oa.num_dig_bytes + 3;
    *pptr++ = srv_req->sm_rp_oa.toa;
    *pptr++ = srv_req->sm_rp_oa.num_dig_bytes + 1;
    /*
     * bit 7 is the extension indicator (set to 1 to indicate last octet)
     * bits 4-6 are the type of number 
     * bits 0-4 are the numbering plan indicator
     */
    bit_to_byte(pptr, 1, 7);
    bits_to_byte(pptr, srv_req->sm_rp_oa.noa, 4, 3);
    bits_to_byte(pptr++, srv_req->sm_rp_oa.npi, 0, 4);
    for (i=0; i<srv_req->sm_rp_oa.num_dig_bytes; i++)
    {
      *pptr++ = srv_req->sm_rp_oa.digits[i];
    }
    mlen += srv_req->sm_rp_oa.num_dig_bytes + 5;
  }

  /*
   * SM RP UI
   */
  if (bit_test(srv_req->pi, MAPPN_sm_rp_ui))
  {
    *pptr++ = MAPPN_sm_rp_ui;
    *pptr++ = srv_req->sm_rp_ui.num_bytes;
    for (i=0; i<srv_req->sm_rp_ui.num_bytes; i++)
    {
      *pptr++ = srv_req->sm_rp_ui.data[i];
    }
    mlen += srv_req->sm_rp_ui.num_bytes + 2;
  }

  /*
   * MSISDN
   */
  if (bit_test(srv_req->pi, MAPPN_msisdn))
  {
    *pptr++ = MAPPN_msisdn;
    *pptr++ = srv_req->msisdn.num_dig_bytes + 1;
    /*
     * bit 7 is the extension indicator (set to 1 to indicate last octet)
     * bits 4-6 are the nature of address
     * bits 0-4 are the numbering plan indicator
     */
    bit_to_byte(pptr, 1, 7);
    bits_to_byte(pptr, srv_req->msisdn.noa, 4, 3);
    bits_to_byte(pptr++, srv_req->msisdn.npi, 0, 4);
    for (i=0; i<srv_req->msisdn.num_dig_bytes; i++)
    {
      *pptr++ = srv_req->msisdn.digits[i];
    }
    mlen += srv_req->msisdn.num_dig_bytes + 3;
  }


  /*
   * IMSI
   */
  if (bit_test(srv_req->pi, MAPPN_imsi))
  {
    *pptr++ = MAPPN_imsi;
    *pptr++ = srv_req->imsi.num_bytes;
    for (i=0; i<srv_req->imsi.num_bytes; i++)
    {
      *pptr++ = srv_req->imsi.data[i];
    }
    mlen += srv_req->imsi.num_bytes + 2;
  }

  /*
   * GGSN Number
   */
  if (bit_test(srv_req->pi, MAPPN_ggsn_number))
  {
    *pptr++ = MAPPN_ggsn_number;
    *pptr++ = srv_req->ggsn_num.num_dig_bytes + 1;
    /*
     * bit 7 is the extension indicator (set to 1 to indicate last octet)
     * bits 4-6 are the nature of address
     * bits 0-4 are the numbering plan indicator
     */
    bit_to_byte(pptr, 1, 7);
    bits_to_byte(pptr, srv_req->ggsn_num.noa, 4, 3);
    bits_to_byte(pptr++, srv_req->ggsn_num.npi, 0, 4);
    for (i=0; i<srv_req->ggsn_num.num_dig_bytes; i++)
    {
      *pptr++ = srv_req->ggsn_num.digits[i];
    }
    mlen += srv_req->ggsn_num.num_dig_bytes + 3;
  }
  /* 
   * MoreMessagesToSend 
   */
   
   
  if (bit_test(srv_req->pi, MAPPN_more_msgs))
  {
    *pptr++ = MAPPN_more_msgs;
        *pptr++ = 0;
        mlen += 2;
  }  
  
  /* Short Message delivery priority */
   
   
  if (bit_test(srv_req->pi, MAPPN_sm_rp_pri))
  {
    *pptr++ = MAPPN_sm_rp_pri;
        *pptr++ = 1;
    *pptr++ = 1;
        mlen += 3;
  }  
  
  /*
   * USSD Coding
   */
   if (bit_test(srv_req->pi, MAPPN_USSD_coding))
  {
    *pptr++ = MAPPN_USSD_coding;
    *pptr++ = srv_req->ussd_coding.num_bytes;
    for (i=0; i<srv_req->ussd_coding.num_bytes; i++)
    {
      *pptr++ = srv_req->ussd_coding.data[i];
    }
    mlen += srv_req->ussd_coding.num_bytes + 2;
  }  
  
  /*
   * USSD String
   */
  if (bit_test(srv_req->pi, MAPPN_USSD_string))
  {
    *pptr++ = MAPPN_USSD_string;
    *pptr++ = srv_req->ussd_string.num_bytes - 1; 
        /*  USSD string bytes without preceding length information */  
    for (i=1; i<srv_req->ussd_string.num_bytes; i++)
    {
      *pptr++ = srv_req->ussd_string.data[i];
    }
    mlen += srv_req->ussd_string.num_bytes + 2;
  }
    
  /*
   * Service Centre Address 2-12oct
   */
  if (bit_test(srv_req->pi, MAPPN_sc_addr))
  {
    *pptr++ = MAPPN_sc_addr;
    *pptr++ = srv_req->sc_addr.num_dig_bytes + 1;
    /*
     * bit 7 is the extension indicator (set to 1 to indicate last octet)
     * bits 4-6 are the nature of address
     * bits 0-4 are the numbering plan indicator
     */
    bit_to_byte(pptr, 1, 7);
    bits_to_byte(pptr, srv_req->sc_addr.noa, 4, 3);
    bits_to_byte(pptr++, srv_req->sc_addr.npi, 0, 4);
    for (i=0; i<srv_req->sc_addr.num_dig_bytes; i++)
    {
      *pptr++ = srv_req->sc_addr.digits[i];
    }
    mlen += srv_req->sc_addr.num_dig_bytes + 3;
  }

  /*
   * Timeout
   */
  if (bit_test(srv_req->pi, MAPPN_timeout))
  {
    *pptr++ = MAPPN_timeout;
    *pptr++ = 2;
    *pptr++ = srv_req->timeout & 0xff;
    *pptr++ = (srv_req->timeout & 0xff00) >> 8;
    mlen += 4;
  }

  if (bit_test(srv_req->pi, MAPPN_nb_req_vect))
    {
      *pptr++ = 0xF0;   // 'cause MAPPN_nb_req_vect > 0xff
      *pptr++ = 4;
      *pptr++ = (MAPPN_nb_req_vect & 0xFF00) >> 8;
      *pptr++ = MAPPN_nb_req_vect & 0xff;
      *pptr++ = 1;
      *pptr++ = srv_req->nb_req_vect;
      mlen += 6;
    }

  /*
   * Set last byte to zero, indicating no more parameters.
   */
  *pptr = 0;

  m->len = (unsigned short)mlen;

  return(0);
};

/*
 * MTU_msg_to_ind converts primitive indications
 * from the MAP module into the 'C' structured
 * representation of a primitive indication.
 *
 * Returns zero on success or an error code otherwise.
 */
int MTU_msg_to_ind(MTU_MSG* ind, MSG* m)
  /*MTU_MSG  *ind;         structure to recover data to */
  /*MSG* m     message to recover data from */
{
  u16   mlen;
  int   ret;
  u8    plen;           /* current parameter length */
  u8    *pptr;
  u8    *eop;
  u8    pname;          /* parameter name */

  ret = -1;
  ind->ss7dialogueID = m->hdr.id;
  if (m->hdr.type == MAP_MSG_DLG_IND)
    ind->dlg_prim = 1;
  else
    ind->dlg_prim = 0;
  mlen = m->len;

  /*
   * Clear the parameter indicator field
   */
  memset((void *)ind->pi, 0, PI_BYTES);

  /*
   * recover the parameters
   */
  if (mlen)
  {
    pptr = get_param(m);
    eop = pptr + mlen;
    ind->type = *pptr++;

    while (pptr < eop)
    {
      if (*pptr == 0)
      {
        if (++pptr == eop)
          ret = 0;
        break;
      }
      else
      {
        pname = *pptr++;
                
        if (pname != MAPPN_CODE_SHIFT)
        {             
          bit_set(ind->pi, pname);
          plen = *pptr++;
          ret = MTU_recover_param(pname, plen, pptr, ind);
          pptr += plen;
        }
        else
        {
          /*
           * Code shift parameters not yet supported
           */
          return(ret);
        }        
      }
    } /* end while */
  } /* end if */
  return(ret);
} /* end of MTU_msg_to_ind() */

/*
 * MTU_recover_param() recovers a parameter from a received message into
 * the C structure for that message.
 *
 * Always returns zero (success).
 */
static int MTU_recover_param(u8 pname,u8 plen,u8* pptr,MTU_MSG* ind)
  /*u8 pname;              parameter name */
  /*u8 plen;               parameter length */
  /*u8 *pptr;              parameter data */
  /*MTU_MSG *ind;          structure to map parameter into */
{
  u8 i;                 /* loop counter */

  switch(pname)
  {
    case MAPPN_imsi:
      if (plen <= MAX_BCDSTR_LEN)
      {
        for (i=0; i<plen; i++)
        {
          ind->imsi.data[i] = *pptr;
          pptr++;
        }
        ind->imsi.num_bytes = plen;
      }
      else
        MTU_disp_err(ind->ss7dialogueID,"IMSI too long");
      break;

    case MAPPN_invoke_id:
      ind->ss7invokeID = *pptr;
      break;

    case MAPPN_prob_diag:
      ind->prob_diag = (MAP_PROB_DIAG)*pptr;
      break;

    case MAPPN_prov_rsn:
      ind->prov_reason = (MAP_PROV_RSN)*pptr;
      break;

    case MAPPN_prov_err:
      ind->prov_err = (MAP_PROV_ERR)*pptr;
      break;

    case MAPPN_refuse_rsn:
      ind->refuse_reason = (MAP_REF_RSN)*pptr;
      break;

    case MAPPN_result:
      ind->result = (MAP_RESULT)*pptr;
      break;

    case MAPPN_user_err:
      ind->user_err = (MAP_USER_ERR)*pptr;
      break;

    case MAPPN_user_rsn:
      ind->user_reason = (MAP_USER_RSN)*pptr;
      break;
  }

  return(0);
} /* end of MTU_recover_param() */

/*
 * MTU_get_scts()
 * Uses the time.h library to generate a timestamp which is then formatted
 * into semi-octets in a u8 array (time_stamp).
 *
 * Returns 0 if the time stamp was formatted correctly
 *         -1 if it was not.
 */
int MTU_get_scts(u8* time_stamp)
{
  struct tm *utc_time;  /* Coordinated Univeral Time (UTC) */
  time_t calender_time;

  time(&calender_time);

  utc_time = gmtime(&calender_time);

  if (utc_time != NULL)
  {
    /*
     * year, month, day, hour, minute, second
     * The tm_mon value is the number of months since january, so add one
     * to get the month number (jan=1,feb=2, ..,dec=12).
     */
    time_stamp[0] = SEMIOCTS(utc_time->tm_year);
    time_stamp[1] = SEMIOCTS(utc_time->tm_mon + 1);
    time_stamp[2] = SEMIOCTS(utc_time->tm_mday);
    time_stamp[3] = SEMIOCTS(utc_time->tm_hour);
    time_stamp[4] = SEMIOCTS(utc_time->tm_min);
    time_stamp[5] = SEMIOCTS(utc_time->tm_sec);
    /*
     * We use calander time so time zone is zero
     */
    time_stamp[6] = 4; // Changed by Olegin, было 0, поставил зону +4, лучше здесь как-то получить временную зону из настроек машины
    return(0);
  }
  else
  {
    printf("*** Unable to determine time stamp ***\n");
    return(-1);
  }
}

/*
 * MTU_str_to_def_alph()
 * Formats the ascii string into an octet array.
 *
 * Returns the number of formatted default alpabet characters, not the number
 * octets formatted.
 * Returns zero if the formatting fails.
 */
u8 MTU_str_to_def_alph(char* ascii_str, u8 *da_octs,u8 *da_len,u8 max_octs)
  /*char *ascii_str;       The source of the ascii chars */
  /*u8   *da_octs;         The u8 array into which the deft alph char are built */
  /*u8  *da_len;          The formated octet length of da_octs, updated */
  /*u8   max_octs;        The max size of da_octs */
{
  u8 buf_index = 0;    /* position in ascii buffer */
  u16 bits_written = 0; /* bits written into default alphabet buffer */
  
  while (ascii_str[buf_index] != '\0')
  {
    /*
     * Is there enough room for the default alphabet octets
     */
    if ((bits_written + 7) > (max_octs * 8)) 
    {
      printf("MTU_str_to_def_alph fails: not enough room!\n");
      return(0);  
    } 

    packbits(da_octs, buf_index * 7, ASCII2DEF(ascii_str[buf_index]), 7);

    bits_written = ++buf_index * 7;
  }

  /*
   * Do we need to pad the rest of the last octet
   */
  if (bits_written % 8 != 0)
  { 
    /*
     * Pad with zeros
     */
    packbits(da_octs, bits_written, 0, 8 - (bits_written % 8));
    /*
     * Update the number of octets used
     */
    *da_len = bits_written / 8 + 1;
  }
  else
  {
    /*
     * Update the number of octets used
     */
    *da_len = bits_written / 8 ;
  }

  return(buf_index);
}

/*
 * MTU_USSD_str_to_def_alph()
 * Formats the ascii string into an octet array.
 *
 * Returns the number of formatted default alpabet characters, not the number
 * octets formatted.
 * Returns zero if the formatting fails.
 *
 * Additional test made for USSD padding when 7 binary pad bits are needed. (GSM 03.38)
 */
u8 MTU_USSD_str_to_def_alph(char* ascii_str,u8 *da_octs,u8 *da_len,u8 max_octs)
  /*char *ascii_str;       The source of the ascii chars */
  /*u8   *da_octs;         The u8 array into which the deft alph char are built */
  /*u8  *da_len;          The formated octet length of da_octs, updated */
  /*u8   max_octs;        The max size of da_octs */
{
  u8 buf_index = 0;    /* position in ascii buffer */
  u16 bits_written = 0; /* bits written into default alphabet buffer */
  int bitcount;         /* USSD test */ 

  while (ascii_str[buf_index] != '\0')
  {
    /*
     * Is there enough room for the default alphabet octets
     */
    if ((bits_written + 7) > (max_octs * 8)) 
    {
      printf("MTU_USSD_str_to_def_alph fails: not enough room!\n");
      return(0);  
    }    
      

    packbits(da_octs, buf_index * 7, ASCII2DEF(ascii_str[buf_index]), 7);

    bits_written = ++buf_index * 7;
  }

  /*
   * Do we need to pad the rest of the last octet
   */
  if (bits_written % 8 != 0)
  { 
    /*
     * Pad with zeros
     */
    bitcount = (bits_written % 8);
    if  (bitcount == 7)  
    { 
      /* For USSD pack with the <CR> character = 0001101
       * see GSM 03.38 'USSD packing' for detail
       */
      packbits(da_octs, bits_written,     1, 1);
      packbits(da_octs, bits_written + 1, 0, 1);
      packbits(da_octs, bits_written + 2, 1, 1);
      packbits(da_octs, bits_written + 3, 1, 1);
      packbits(da_octs, bits_written + 4, 0, 1);
      packbits(da_octs, bits_written + 5, 0, 1);
      packbits(da_octs, bits_written + 6, 0, 1);
      /*
       * Update the number of octets used
       */
      *da_len = bits_written / 8 + 1;
      }
      else 
      {
        packbits(da_octs, bits_written, 0, 8 - (bits_written % 8));
      }
      
      /*
       * Update the number of octets used
       */
      *da_len = bits_written / 8 + 1;
  }
  else
  {
    /*
     * Update the number of octets used
     */
    *da_len = bits_written / 8 ;
  }

  return(buf_index);
}

