#pragma once

#include <set>
#include <vector>
#include <map>
#include "mtu.h"
#include "system.h"

enum AuthAttributeType
{
    UNKNOWN,
    RAND,
    SRES,
    KC,
    XRES,
    CK,
    IK,
    AUTN
};


enum AuthVectorType
{
    VECTOR_UNKNOWN,
    TRIPLET,
    QUINTUPLET
};


struct AuthInfoAttribute
{
    AuthInfoAttribute(/*AuthAttributeType type, */u8* buffer, u16 size) :
       // type(type),
        data(buffer, buffer + size)
    {}

    //AuthAttributeType type;
    std::vector<u8> data;
};


class AuthVector
{
public:
    void AddAuthAttribute(AuthAttributeType type, const AuthInfoAttribute& attr) {
        attrs.insert(std::make_pair(type, attr));
    }

    bool IsComplete() {
        return (getType() == QUINTUPLET || getType() == TRIPLET);
    }

    AuthVectorType getType() {
        if (Collected(RAND) && Collected(XRES) && Collected(CK) && Collected(IK) && Collected(AUTN))
            return QUINTUPLET;
        else if (Collected(RAND) && Collected(KC) && Collected(SRES))
            return TRIPLET;
        else {
            return VECTOR_UNKNOWN;
        }
    }

    bool IsAttributeMissing(AuthAttributeType type) {
        return !Collected(type);
    }


    std::map<AuthAttributeType, AuthInfoAttribute> attrs;
private:
    std::set<AuthAttributeType> attrTypesCollected;

    bool Collected(AuthAttributeType type) {
        return attrs.find(type) != attrs.end();
    }
};


class AuthInfoRequest
{
public:
    AuthInfoRequest();
};


enum REQUEST_TYPE {
    TRIPLET_REQUEST,
    QUINTUPLET_REQUEST
} ;


struct SS7_REQUEST
{
    SS7_REQUEST(REQUEST_TYPE rt, u16 cltReqNum, u32 gwID, const char* reqIMSI, u8 reqVectors,
                int sock, u16 dlgID) :
        request_type(rt),
        clientRequestNum(cltReqNum),
        gatewayRequestID(gwID),
        requestedVectorsNum(reqVectors),
        receivedVectorsNum(0),
        sockIndex(sock),
        ss7dialogueID(dlgID)
    {
        ss7invokeID = 1;        // Use the same invoke_id
        strncpy(imsi, reqIMSI, sizeof(imsi));
        for (int i = 0; i < MAX_VECTORS_NUM; i++) {
            binRAND[i] = NULL;
            binXRES[i] = NULL;
            binCK[i] = NULL;
            binIK[i] = NULL;
            binAUTN[i] = NULL;
            binKC[i] = NULL;
            binSRES[i] = NULL;
        }
        binRANDnum = 0;
        binKCnum = 0;
        binSRESnum = 0;
        binXRESnum = 0;
        binCKnum = 0;
        binIKnum = 0;
        binAUTNnum = 0;

        binRANDsize = 0;
        binSRESsize = 0;
        binKCsize = 0;
        binXRESsize = 0;
        binCKsize = 0;
        binIKsize = 0;
        binAUTNsize = 0;

        rand[0][0]=0;
        rand[1][0]=0;
        rand[2][0]=0;
        rand[3][0]=0;
        rand[4][0]=0;
        kc[0][0]=0;
        kc[1][0]=0;
        kc[2][0]=0;
        kc[3][0]=0;
        kc[4][0]=0;
        sres[0][0]=0;
        sres[1][0]=0;
        sres[2][0]=0;
        sres[3][0]=0;
        sres[4][0]=0;
        successful=false;
        time(&stateChangeTime);
    }

    void AddAuthAttribute(AuthAttributeType type, const AuthInfoAttribute& attr) {
        for(auto it = authVectors.begin(); it != authVectors.end(); it++) {
            if (it->IsAttributeMissing(type)) {
                it->AddAuthAttribute(type, attr);
                return;
            }
        }
        AuthVector newAuthVector;
        newAuthVector.AddAuthAttribute(type, attr);
        authVectors.push_back(newAuthVector);
    }

    bool EnoughVectorsReceived() {
        receivedVectorsNum = 0;
        for(auto it = authVectors.begin(); it != authVectors.end(); it++) {
            if (it->IsComplete()) {
                receivedVectorsNum++;
            }
        }
        if (receivedVectorsNum >= requestedVectorsNum) {
            return true;
        }
        else {
            return false;
        }
    }

    void SetSuccess(bool success) {
        successful = success;
    }

    void FillConcatenatedVectors( ) {
        for(auto vectorIt = authVectors.begin(); vectorIt != authVectors.end(); vectorIt++) {
            auto attrIt = vectorIt->attrs.find(RAND);
            if (attrIt != vectorIt->attrs.end()) {
                concatRAND.insert(concatRAND.end(), attrIt->second.data.begin(), attrIt->second.data.end());
            }
            attrIt = vectorIt->attrs.find(XRES);
            if (attrIt != vectorIt->attrs.end()) {
                concatXRES.insert(concatXRES.end(), attrIt->second.data.begin(), attrIt->second.data.end());
            }
            attrIt = vectorIt->attrs.find(CK);
            if (attrIt != vectorIt->attrs.end()) {
                concatCK.insert(concatCK.end(), attrIt->second.data.begin(), attrIt->second.data.end());
            }
            attrIt = vectorIt->attrs.find(IK);
            if (attrIt != vectorIt->attrs.end()) {
                concatIK.insert(concatIK.end(), attrIt->second.data.begin(), attrIt->second.data.end());
            }
            attrIt = vectorIt->attrs.find(AUTN);
            if (attrIt != vectorIt->attrs.end()) {
                concatAUTN.insert(concatAUTN.end(), attrIt->second.data.begin(), attrIt->second.data.end());
            }
        }
    }

    REQUEST_TYPE request_type;
    u16 clientRequestNum;
    u32 gatewayRequestID;
    char imsi[30];
    MTU_BCDSTR bcdIMSI;
    u8 requestedVectorsNum;            /*number of triplets requested*/
    u8 receivedVectorsNum;           /*number of triplets received*/
    int sockIndex;
    u16 ss7dialogueID;
    u8 ss7invokeID;                 /* Invoke ID for service request */
    REQUEST_STATE state;
    time_t stateChangeTime;
    char rand[MAX_VECTORS_NUM][33];
    char sres[MAX_VECTORS_NUM][9];
    char kc[MAX_VECTORS_NUM][17];
    char xres[MAX_VECTORS_NUM][33];
    char ck[MAX_VECTORS_NUM][33];
    char ik[MAX_VECTORS_NUM][33];
    char autn[MAX_VECTORS_NUM][33];

    u8* binRAND[MAX_VECTORS_NUM];
    u8* binXRES[MAX_VECTORS_NUM];
    u8* binCK[MAX_VECTORS_NUM];
    u8* binIK[MAX_VECTORS_NUM];
    u8* binAUTN[MAX_VECTORS_NUM];
    u8* binSRES[MAX_VECTORS_NUM];
    u8* binKC[MAX_VECTORS_NUM];

    u8 binRANDnum;
    u8 binSRESnum;
    u8 binKCnum;
    u8 binXRESnum;
    u8 binCKnum;
    u8 binIKnum;
    u8 binAUTNnum;

    u8 binRANDsize;
    u8 binSRESsize;
    u8 binKCsize;
    u8 binXRESsize;
    u8 binCKsize;
    u8 binIKsize;
    u8 binAUTNsize;

    bool successful;
    std::string error;

    std::vector<AuthVector> authVectors;

    std::vector<u8> concatRAND;
    std::vector<u8> concatAUTN;
    std::vector<u8> concatXRES;
    std::vector<u8> concatCK;
    std::vector<u8> concatIK;

    ~SS7_REQUEST() {
        for (int i = 0; i < binRANDnum; i++)    {
            free(binRAND[i]);
        }
        for (int i = 0; i < binSRESnum; i++)    {
            free(binSRES[i]);
        }
        for (int i = 0; i < binXRESnum; i++)    {
            free(binXRES[i]);
        }
        for (int i = 0; i < binCKnum; i++)    {
            free(binCK[i]);
        }
        for (int i = 0; i < binIKnum; i++)    {
            free(binIK[i]);
        }
        for (int i = 0; i < binAUTNnum; i++)    {
            free(binAUTN[i]);
        }
        for (int i = 0; i < binKCnum; i++)    {
            free(binKC[i]);
        }
    }
};


