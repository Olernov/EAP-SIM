#ifndef _COACOMMON_H_
#define _COACOMMON_H_

#define	NAS_ID		"NASId"
#define	NAS_IP		"NASIp"
#define	NAS_PORT	"NASPort"
#define USER_NAME	"UserName"
#define USER_PSWD	"UserPswd"
#define SESSION_ID	"SessionId"
#define	ACCNT_INF	"AccountInfo"
#define	COMMAND		"Command"
#define	CMD_ACCNT_LOGON		"AccountLogon"
#define	CMD_ACCNT_LOGOFF	"AccountLogoff"
#define CMD_SESSION_QUERY	"SessionQuery"
#define CMD_SRV_ACTIVATE	"ServiceActivate"
#define CMD_SRV_DEACTIVATE	"ServiceDeActivate"
#define CMD_OPTION_FORCE	"Force"
#define CMD_ERX_ACTIVATE	"ERX-Service-Activate"
#define CMD_ERX_DEACTIVATE	"ERX-Service-Deactivate"

#define	CMD_VAL_SEP			"="
#define	CMD_PARAM_SEP		"\n"

/*	IRBiS-PS request types
 *	0x00xx
 */
#define	MONIT_REQ	(unsigned short)0x0000
#define	MONIT_RSP	(unsigned short)0x0001
#define	ADMIN_REQ	(unsigned short)0x0002
#define	ADMIN_RSP	(unsigned short)0x0003

/*	IRBiS-PS request types
 *	0x03xx
 */
#define	COMMAND_REQ	(unsigned short)0x0302
#define	COMMAND_RSP	(unsigned short)0x0303

/*	IRBiS-PS common attribute types
 *	0x00xx
 */
#define	PS_RESULT		(unsigned short)0x0000
#define	PS_DESCR		(unsigned short)0x0001
#define	PS_SUBSCR		(unsigned short)0x0002
#define	PS_LASTOK		(unsigned short)0x0005
#define	PS_LASTER		(unsigned short)0x0006
#define	PS_STATUS		(unsigned short)0x0007
#define	PS_MDATTR		(unsigned short)0x0008
#define	PS_ADMCMD		(unsigned short)0x0010

/*	IRBiS-PS CoA module attribute types
 *	0x03xx
 */
#define	PS_NASIP		(unsigned short)0x0300
#define	PS_NASPORT	(unsigned short)0x0301
#define PS_USERNAME	(unsigned short)0x0302
#define	PS_USERPSWD	(unsigned short)0x0303
#define PS_SESSID		(unsigned short)0x0304
#define	PS_ACCINFO	(unsigned short)0x0305
#define PS_COMMAND	(unsigned short)0x0306
#define PS_FRAMEDIP	(unsigned short)0x0307
#define PS_SESSTATUS	(unsigned short)0x0308

/* IRBiS-PS SS7 gateway request types
  */
#define	SS7GW_IMSI_REQ  (unsigned short)0x0602 // request-IMSI
#define	SS7GW_IMSI_RESP (unsigned short)0x0603 // response-IMSI
#define	RS_TRIP_REQ     (unsigned short)0x0104 // request-triplet
#define	RS_TRIP_RESP    (unsigned short)0x0105 // response-triplet

/* IRBiS-PS SS7 gateway attribute types
  */
#define	SS7GW_IMSI      (unsigned short)0x0602 // IMSI
#define	SS7GW_TRIP_NUM  (unsigned short)0x0603 // Number of triplets requested
#define	RS_RAND1        (unsigned short)0x0103 // RAND1
#define	RS_SRES1        (unsigned short)0x0104 // SRES1
#define	RS_KC1          (unsigned short)0x0105 // KC1
#define	RS_RAND2        (unsigned short)0x0106 // RAND2
#define	RS_SRES2        (unsigned short)0x0107 // SRES2
#define	RS_KC2          (unsigned short)0x0108 // KC2
#define	RS_RAND3        (unsigned short)0x0109 // RAND3
#define	RS_SRES3        (unsigned short)0x010a // SRES3
#define	RS_KC3          (unsigned short)0x010b // KC3
#define	RS_RAND4        (unsigned short)0x010c // RAND4
#define	RS_SRES4        (unsigned short)0x010d // SRES4
#define	RS_KC4          (unsigned short)0x010e // KC4
#define	RS_RAND5        (unsigned short)0x010f // RAND5
#define	RS_SRES5        (unsigned short)0x0110 // SRES5
#define	RS_KC5          (unsigned short)0x0111 // KC5

#pragma pack(push,1)
/*	Атрибут запроса в формате IRBiS-PS
 */
struct SPSReqAttr {
	unsigned short m_usAttrType;
	unsigned short m_usAttrLen;
};

/*	Запрос в формате IRBiS-PS
 */
struct SPSRequest {
	unsigned int m_uiReqNum;
	unsigned short m_usReqType;
	unsigned short m_usPackLen;
};
#pragma pack(pop)

#endif /*_COACOMMON_H_*/
