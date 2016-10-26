#include <map>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef WIN32
#	include <Winsock2.h>
#	include "CoACommon.h"
#	pragma	comment(lib, "ws2_32.lib")
#else
#	include <arpa/inet.h>
#	include "CoACommon.h"
#endif

#include "PSPacket.h"

int CPSPacket::Init (SPSRequest *p_psoBuf, size_t p_stBufSize, __uint32_t p_ui32ReqNum, __uint16_t p_ui16ReqType) {
	int iRetVal = 0;

	do {
		/* проверяем достаточно ли размера буфера для заголовка пакета */
		if (p_stBufSize < sizeof(SPSRequest)) { iRetVal = -1; break; }
		p_psoBuf->m_uiReqNum = htonl (p_ui32ReqNum);
		p_psoBuf->m_usReqType = htons (p_ui16ReqType);
		p_psoBuf->m_usPackLen = htons (sizeof(SPSRequest));
	} while (0);

	return iRetVal;
}

int CPSPacket::SetReqNum (SPSRequest *p_psoBuf, size_t p_stBufSize, __uint32_t p_ui32ReqNum, int p_iValidate) {
	int iRetVal = 0;

	do {
		if (p_iValidate) { iRetVal = Validate (p_psoBuf, p_stBufSize); }
		if (iRetVal) { break; }
		p_psoBuf->m_uiReqNum = htonl (p_ui32ReqNum);
	} while (0);

	return iRetVal;
}

int CPSPacket::SetReqType (SPSRequest *p_psoBuf, size_t p_stBufSize, __uint16_t p_ui16ReqType, int p_iValidate) {
	int iRetVal = 0;

	do {
		if (p_iValidate) { iRetVal = Validate (p_psoBuf, p_stBufSize); }
		if (iRetVal) { break; }
		p_psoBuf->m_usReqType = htons (p_ui16ReqType);
	} while (0);

	return iRetVal;
}

int CPSPacket::AddAttr (SPSRequest *p_psoBuf, size_t p_stBufSize, __uint16_t p_ui16Type, const void *p_pValue, __uint16_t p_ui16ValueLen, int p_iValidate) {
	int iRetVal = 0;
	SPSReqAttr *psoTmp;
	__uint16_t
		ui16PackLen,
		ui16AttrLen;

	do {
		/* валидация пакета */
		if (p_iValidate) { iRetVal = Validate (p_psoBuf, p_stBufSize); }
		if (iRetVal) { break; }
		/* проверка размера буфера */
		ui16PackLen = ntohs (p_psoBuf->m_usPackLen);
		ui16AttrLen = p_ui16ValueLen + sizeof(SPSReqAttr);
		if ((size_t)(ui16PackLen + ui16AttrLen) > p_stBufSize) { iRetVal = -1; break; }
		psoTmp = (SPSReqAttr*)((char*)p_psoBuf + ui16PackLen);
		psoTmp->m_usAttrType = htons (p_ui16Type);
		psoTmp->m_usAttrLen = htons (ui16AttrLen);
		memcpy ((char*)psoTmp + sizeof(SPSReqAttr), p_pValue, p_ui16ValueLen);
		ui16PackLen += ui16AttrLen;
		p_psoBuf->m_usPackLen = htons (ui16PackLen);
		iRetVal = ui16PackLen;
	} while (0);

	return iRetVal;
}

int CPSPacket::Validate (const SPSRequest *p_psoBuf, size_t p_stBufSize) {
	int iRetVal = 0;
	SPSReqAttr *psoTmp;
	__uint16_t
		ui16TotalLen,
		ui16PackLen,
		ui16AttrLen;

	do {
		/* проверяем достаточен ли размер буфера для заголовка пакета */
		if (p_stBufSize < sizeof(SPSRequest)) { iRetVal = -1; break; }
		/* инициализируем суммарную длину начальным значением */
		ui16TotalLen = sizeof(SPSRequest);
		/* определяем длину пакета по заголовку */
		ui16PackLen = ntohs (p_psoBuf->m_usPackLen);
		/* инициализируем указатель на атрибут начальным значением */
		psoTmp = (SPSReqAttr*)((char*)p_psoBuf + sizeof(SPSRequest));
		/* обходим все атрибуты */
		while (((char*)p_psoBuf + ui16PackLen) > ((char*)psoTmp)) {
			/* определяем длину атрибута */
			ui16AttrLen = ntohs (psoTmp->m_usAttrLen);
			/* увеличиваем суммарную длину пакета */
			ui16TotalLen += ui16AttrLen;
			/* проверяем умещается ли пакет в буфере */
			if (ui16TotalLen > p_stBufSize) { iRetVal = -1; break; }
			/* определяем указатель на следующий атрибут */
			psoTmp = (SPSReqAttr*)((char*)psoTmp + ui16AttrLen);
			/* если суммарная длина больше длины пакета нет смысла проверять дальше */
			if (ui16TotalLen > ui16PackLen) { iRetVal = -1; break; }
		}
		/* если найдена ошибка завершаем проверку */
		if (iRetVal) { break; }
		/* сличаем длину пакета с суммой длин атрибутов */
		if (ui16TotalLen != ui16PackLen) { iRetVal = -1; break; }
	} while (0);

	return iRetVal;
}

void CPSPacket::EraseAttrList (std::multimap<__uint16_t,SPSReqAttr*> &p_mmapAttrList) {
	SPSReqAttr *psoTmp;
	std::multimap<__uint16_t,SPSReqAttr*>::iterator iterList;

	for (iterList = p_mmapAttrList.begin(); iterList != p_mmapAttrList.end(); ) {
		psoTmp = iterList->second;
		free (psoTmp);
#ifdef WIN32
//        iterList =
#endif
			p_mmapAttrList.erase (iterList);
	}
}

int CPSPacket::Parse (const SPSRequest *p_psoBuf, size_t p_stBufSize, std::multimap<__uint16_t,SPSReqAttr*> &p_pumapAttrList, int p_iValidate) {
	int iRetVal = 0;
	SPSReqAttr *psoTmp;
	SPSReqAttr *psoPSReqAttr;
	__uint16_t
		ui16PackLen,
		ui16AttrLen;

	do {
		/* валидация пакета */
		if (p_iValidate) { iRetVal = Validate (p_psoBuf, p_stBufSize); }
		if (iRetVal) { break; }
		/* определяем длину пакета */
		ui16PackLen = ntohs (p_psoBuf->m_usPackLen);
		/* инициализируем указатель на атрибут начальным значением */
		psoTmp = (SPSReqAttr*)((char*)p_psoBuf + sizeof(SPSRequest));
		/* обходим все атрибуты */
		while (((char*)p_psoBuf + ui16PackLen) > ((char*)psoTmp)) {
			/* определяем длину атрибута */
			ui16AttrLen = ntohs (psoTmp->m_usAttrLen);
			/* добавляем атрибут в список */
			psoPSReqAttr = (SPSReqAttr*) malloc (ui16AttrLen);
			memcpy (psoPSReqAttr, psoTmp, ui16AttrLen);
			p_pumapAttrList.insert (std::make_pair (ntohs (psoTmp->m_usAttrType), (SPSReqAttr*)psoPSReqAttr));
			/* определяем указатель на следующий атрибут */
			psoTmp = (SPSReqAttr*)((char*)psoTmp + ui16AttrLen);
		}
	} while (0);

	return iRetVal;
}

int CPSPacket::Parse (const SPSRequest *p_psoBuf, size_t p_stBufSize, __uint32_t &p_ui32ReqNum, __uint16_t &p_ui16ReqType, __uint16_t &p_ui16PackLen, std::multimap<__uint16_t,SPSReqAttr*> &p_pumapAttrList, int p_iValidate) {
    int iRetVal = 0;

    /* валидация пакета */
    if (p_iValidate) { iRetVal = Validate (p_psoBuf, p_stBufSize); }
    if (iRetVal) { return iRetVal; }
    p_ui32ReqNum=ntohl (p_psoBuf->m_uiReqNum);
    p_ui16ReqType=ntohs (p_psoBuf->m_usReqType);
    p_ui16PackLen=ntohs (p_psoBuf->m_usPackLen);

    return Parse(p_psoBuf,p_stBufSize,p_pumapAttrList,0);
}

int CPSPacket::Parse (const SPSRequest *p_psoBuf, size_t p_stBufSize, char *p_pmcOutBuf, size_t p_stOutBufSize) {
	int iRetVal = 0;
	int iFnRes, iStrLen;
	char mcString[0x1000];
	size_t stWrtInd, stStrLen;
	std::multimap<__uint16_t,SPSReqAttr*> mapAttrList;

	do {
		/* разбор атрибутов пакета */
		iRetVal = Parse (p_psoBuf, p_stBufSize, mapAttrList);
		if (iRetVal) { break; }
		/* формируем заголовок пакета */
#ifdef WIN32
		iFnRes = _snprintf (
#else
		iFnRes = snprintf (
#endif
			mcString,
			sizeof (mcString) -1,
			"request number: 0x%08x; type: 0x%04x; length: %u;",
			ntohl (p_psoBuf->m_uiReqNum),
			ntohs (p_psoBuf->m_usReqType),
			ntohs (p_psoBuf->m_usPackLen));
		if (0 >= iFnRes) { iRetVal = -1; break; }
		/* для linux: если строка целиком не уместилась в буфер (под Windows это не работает) */
		if (iFnRes >= sizeof (mcString) - 1) { iFnRes = sizeof (mcString) - 1; }
		mcString[iFnRes] = '\0';
		stWrtInd = 0;
		stStrLen = p_stOutBufSize > iFnRes ? iFnRes : p_stOutBufSize - 1;
		memcpy (p_pmcOutBuf, mcString, stStrLen);
		p_pmcOutBuf[stStrLen] = '\0';
		stWrtInd = stStrLen;
		/* обходим все атрибуты */
		bool bStop = false;
		for (std::multimap<__uint16_t,SPSReqAttr*>::iterator iter = mapAttrList.begin(); iter != mapAttrList.end (); ++iter) {
			/* формируем заголовок атрибута */
#ifdef WIN32
			iFnRes = _snprintf (
#else
			iFnRes = snprintf (
#endif
				mcString,
				sizeof (mcString) - 1,
				" attribute code: 0x%04x; length: %u; value: ",
				ntohs (iter->second->m_usAttrType),
				ntohs (iter->second->m_usAttrLen));
			/* если произошла ошика завершаем формирование атрибутов */
			if (0 >= iFnRes) { iRetVal = -1; break; }
			if (iFnRes >= sizeof (mcString) - 1) { iFnRes = sizeof (mcString) - 1; }
			mcString[iFnRes] = '\0';
			/* выводим значение атрибута по байтам */
			for (int iInd = sizeof (SPSReqAttr); iInd < ntohs (iter->second->m_usAttrLen) && iFnRes < sizeof(mcString) - 1; ++iInd) {
				if (0x20 <= reinterpret_cast<char*>(iter->second)[iInd] && 0x7F > reinterpret_cast<char*>(iter->second)[iInd]) {
					mcString[iFnRes] = reinterpret_cast<char*>(iter->second)[iInd];
					++iFnRes;
				} else {
					/* если следующий символ не уместится в буфер - завершаем формирование атрибута */
					if (iFnRes + 10 > sizeof(mcString) - 1) { bStop =  true; break; }
#ifdef WIN32
					iStrLen = _snprintf (
#else
					iStrLen = snprintf (
#endif
						&mcString[iFnRes],
						sizeof (mcString) - 1 - iFnRes,
						"\\x%02x",
						reinterpret_cast<unsigned char*>(iter->second)[iInd]);
					/* если при выводе очередного байта возникла ошибка прекращаем обработку атрибута */
					if (0 >= iStrLen) { bStop =  true; break; }
					/* для linux: если строка целиком не уместилась в буфер (под Windows это не работает) */
					if (iStrLen >= sizeof (mcString) - 1 - iFnRes) { iStrLen = sizeof (mcString) - 1 - iFnRes; }
					iFnRes += iStrLen;
				}
				/* досрочно завершаем вывод атрибута */
				if (bStop) { break; }
			}
			mcString[iFnRes] = '\0';
			if (iRetVal) {
				break;
			} else {
				stStrLen = p_stOutBufSize - stWrtInd > iFnRes ? iFnRes : p_stOutBufSize - stWrtInd - 1;
				memcpy (&p_pmcOutBuf[stWrtInd], mcString, stStrLen);
				stWrtInd += stStrLen;
				p_pmcOutBuf[stWrtInd] = '\0';
			}
			/* если формирование атрибута прервано завершаем вывод всего пакета */
			if (bStop) { break; }
		}
		if (0 == iRetVal) {
			iRetVal = static_cast<int>(stWrtInd);
		}
	} while (0);

	return iRetVal;
}
