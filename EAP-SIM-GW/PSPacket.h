
#ifdef	WIN32
#	ifdef	PSPACK_IMPORT
#		define	PSPACK_SPEC	__declspec(dllimport)
#	else
#		define	PSPACK_SPEC __declspec(dllexport)
#	endif
typedef unsigned __int16 __uint16_t;
typedef unsigned __int32 __uint32_t;
#else
#	define	PSPACK_SPEC
#endif

class PSPACK_SPEC CPSPacket {
public:
	/* ������������� ������ */
	int Init (SPSRequest *p_psoBuf, size_t p_stBufSize, __uint32_t p_ui32ReqNum = 0, __uint16_t p_ui16ReqType = 0);
	/* ��������� ������ ������ */
	int SetReqNum (SPSRequest *p_psoBuf, size_t p_stBufSize, __uint32_t p_ui32ReqNum, int p_iValidate = 1);
	/* ��������� ���� ������ */
	int SetReqType (SPSRequest *p_psoBuf, size_t p_stBufSize, __uint16_t p_ui16ReqType, int p_iValidate = 1);
	/* ���������� �������� � ������ */
	int AddAttr (SPSRequest *p_psoBuf, size_t p_stBufSize, __uint16_t p_ui16Type, const void *p_pValue, __uint16_t p_ui16ValueLen, int p_iValidate = 1);
	/* �������� ����� ������ � ����� ���� ��������� */
	int Validate (const SPSRequest *p_psoBuf, size_t p_stBufSize);
	/* ������ ������ */
	void EraseAttrList (std::multimap<__uint16_t,SPSReqAttr*> &p_mmapAttrList);
	int Parse (const SPSRequest *p_psoBuf, size_t p_stBufSize, std::multimap<__uint16_t,SPSReqAttr*> &p_pumapAttrList, int p_iValidate = 1);
    int Parse (const SPSRequest *p_psoBuf, size_t p_stBufSize, __uint32_t &p_ui32ReqNum, __uint16_t &p_ui16ReqType, __uint16_t &p_ui16PackLen, std::multimap<__uint16_t,SPSReqAttr*> &p_pumapAttrList, int p_iValidate=1);
	/* ���������� ���������� ���������� � ����� ��������, � ������ ������ ������������ �������� ����� -1 */
	int Parse (const SPSRequest *p_psoBuf, size_t p_stBufSize, char *p_pmcOutBuf, size_t p_stOutBufSize);
};
