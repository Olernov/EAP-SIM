#pragma once

#include <set>
#include <vector>
#include <map>
#include "mtu.h"
#include "system.h"

typedef unsigned long long u64;

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
    QUINTUPLET,
    BOTH_TRIPLET_AND_QUINTUPLET
};


struct AuthInfoAttribute
{
    AuthInfoAttribute(const u8* buffer, u16 size) :
        data(buffer, buffer + size)
    {}

    std::vector<u8> data;
};


class AuthVector
{
public:
    void AddAuthAttribute(AuthAttributeType type, const AuthInfoAttribute& attr) {
        if (type == XRES && attr.data.size() < MAXIMUM_XRES_LEN) {
            // fill XRES to maximum allowed length (as a requirement of OpenHSS).
            // See also TranslateQuintupletToTriplet methods that operates full size XRES.
            AuthInfoAttribute xres(attr.data.data(), attr.data.size());
            xres.data.insert(xres.data.end(), MAXIMUM_XRES_LEN - attr.data.size(), 0);
            attrs.insert(std::make_pair(XRES, xres));
        }
        else {
            attrs.insert(std::make_pair(type, attr));
        }
    }

    bool IsComplete() {
        return (getType() == QUINTUPLET || getType() == TRIPLET);
    }

    AuthVectorType getType() {
        if (Collected(RAND) && Collected(XRES) && Collected(CK) && Collected(IK) && Collected(AUTN)) {
            if (Collected(KC) && Collected(SRES)) {
                return BOTH_TRIPLET_AND_QUINTUPLET;
            }
            else {
                return QUINTUPLET;
            }
        }
        else if (Collected(RAND) && Collected(KC) && Collected(SRES)) {
            return TRIPLET;
        }
        else {
            return VECTOR_UNKNOWN;
        }
    }

    bool IsAttributeMissing(AuthAttributeType type) {
        return !Collected(type);
    }

    bool TranslateQuintupletToTriplet() {
        // Translate quintuplet to triplet according to 3GPP TS 33.102, change request CR 24r2
        // TS: http://www.3gpp.org/DynaReport/33102.htm
        // Change request: https://www.google.ru/url?sa=t&rct=j&q=&esrc=s&source=web&cd=7&ved=0CF8QFjAG&url=http%3A%2F%2Fwww.3gpp.org%2Fftp%2Ftsg_sa%2Fwg3_security%2FTSGS3_08%2FDocs%2FS3-99451_CR_24r2%2520UMTS%2520GSM%2520interop.rtf&ei=tFIhU67EDMrk4QSG-IDICA&usg=AFQjCNGGnDg3SqX_IEz4gtYBybVG7GgvRw&sig2=eSfKkWaHkBQ9zDAx3lYGgw&bvm=bv.62922401,d.bGE&cad=rja

        if (getType() != QUINTUPLET && getType() != BOTH_TRIPLET_AND_QUINTUPLET) {
            return false;
        }
        auto xresIt = attrs.find(XRES);
        if (xresIt == attrs.end()) {
            return false;
        }
        u32 xres[4];
        u32 sres = 0;
        for (int i = 0; i < 4; i++) {
            if (xresIt->second.data.size() >= 4*(i+1)) {
                xres[i] = *(reinterpret_cast<u32*>(xresIt->second.data.data() + 4*i));
            }
            else {
                xres[i] = 0;
            }
            sres ^= xres[i];
        }
        attrs.insert(std::make_pair(SRES, AuthInfoAttribute(reinterpret_cast<u8*>(&sres), sizeof(sres))));

        auto ckIt = attrs.find(CK);
        auto ikIt = attrs.find(IK);
        if (ckIt == attrs.end() || ikIt == attrs.end() ||
                ckIt->second.data.size() != FIXED_CK_LEN || ikIt->second.data.size() != FIXED_IK_LEN) {
            return false;
        }
        u64 ck1, ck2, ik1, ik2;
        ck1 = *(reinterpret_cast<u64*>(ckIt->second.data.data()));
        ck2 = *(reinterpret_cast<u64*>(ckIt->second.data.data() + 8));
        ik1 = *(reinterpret_cast<u64*>(ikIt->second.data.data()));
        ik2 = *(reinterpret_cast<u64*>(ikIt->second.data.data() + 8));
        u64 kc = ck1 ^ ck2 ^ ik1 ^ ik2;
        attrs.insert(std::make_pair(KC, AuthInfoAttribute(reinterpret_cast<u8*>(&kc), sizeof(kc))));
        return true;
    }

    std::map<AuthAttributeType, AuthInfoAttribute> attrs;
private:
    const static int MAXIMUM_XRES_LEN = 16;
    const static int FIXED_CK_LEN = 16;
    const static int FIXED_IK_LEN = 16;
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

    bool TranslateVectorsToTriplets() {
        for(auto vectorIt = authVectors.begin(); vectorIt != authVectors.end(); vectorIt++) {
            switch (vectorIt->getType()) {
            case TRIPLET:
                break;
            case QUINTUPLET:
                if (!vectorIt->TranslateQuintupletToTriplet()) {
                    return false;
                }
                break;
             default:
                return false;
            }
        }
    }

    bool AllVectorsHaveType(AuthVectorType type) {
        for(auto vectorIt = authVectors.begin(); vectorIt != authVectors.end(); vectorIt++) {
            if (vectorIt->getType() != type) {
                return false;
            }
        }
        return true;
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
    bool successful;
    std::string error;

    std::vector<AuthVector> authVectors;

    std::vector<u8> concatRAND;
    std::vector<u8> concatAUTN;
    std::vector<u8> concatXRES;
    std::vector<u8> concatCK;
    std::vector<u8> concatIK;
};


