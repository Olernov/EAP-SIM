#include <iostream>
#include <vector>
#include <assert.h>
#include "authinfo.h"


void Assert(bool condition, std::string message)
{
    if (!condition) {
        std::cout << message << std::endl;
    }
}

void PrintBinaryDumpToString(char* string, size_t stringSize, const unsigned char* data, size_t dataSize)
{
    size_t i = 0;
    for (; i < dataSize; i++) {
        if (2 * (i + 1) >= stringSize - 1)
            break;
        sprintf(&string[2 * i], "%02x ", data[i]);
    }
    string[2 * (i + 1)] = '\0';
}

void TranslateQuintupletToTripletTest()
{
    // All zeroes test
    AuthVector authVector;
    std::vector<u8> rand(8, 0);
    authVector.AddAuthAttribute(RAND, AuthInfoAttribute(rand.data(), rand.size()));
    std::vector<u8> xres(8, 0);
    authVector.AddAuthAttribute(XRES, AuthInfoAttribute(xres.data(), xres.size()));
    std::vector<u8> ck(16, 0);
    authVector.AddAuthAttribute(CK, AuthInfoAttribute(ck.data(), ck.size()));
    std::vector<u8> ik(16, 0);
    assert(authVector.getType() == UNKNOWN);
    authVector.AddAuthAttribute(IK, AuthInfoAttribute(ik.data(), ik.size()));
    std::vector<u8> autn(16, 0);
    authVector.AddAuthAttribute(AUTN, AuthInfoAttribute(autn.data(), autn.size()));
    assert(authVector.getType() == QUINTUPLET);
    assert(authVector.TranslateQuintupletToTriplet());
    assert(authVector.getType() == BOTH_TRIPLET_AND_QUINTUPLET);
    auto randIt = authVector.attrs.find(RAND);
    auto sresIt = authVector.attrs.find(SRES);
    auto kcIt = authVector.attrs.find(KC);
    assert(randIt != authVector.attrs.end());
    assert(sresIt != authVector.attrs.end());
    assert(kcIt != authVector.attrs.end());
    assert(randIt->second.data == rand);
    std::vector<u8> expectedSres(4, 0);
    assert(sresIt->second.data == expectedSres);
    std::vector<u8> expectedKc(8, 0);
    assert(kcIt->second.data == expectedKc);

    // Real data test
    AuthVector authVector1;
    u8 randData[] = {0x57, 0xc2, 0x0c, 0x16, 0x91, 0xef, 0xbe, 0xb0,
                     0xa3, 0x2e, 0x2c, 0x76, 0x04, 0x81, 0x33, 0xc9};
    authVector1.AddAuthAttribute(RAND, AuthInfoAttribute(randData, sizeof(randData)));
    u8 xresData[] = {0x60, 0x80, 0xc6, 0xc1, 0xed, 0x0c, 0x28, 0x9e};
    authVector1.AddAuthAttribute(XRES, AuthInfoAttribute(xresData, sizeof(xresData)));

    u8 ckData[] = { 0xb6, 0x93, 0x30, 0xab, 0x11, 0x8a, 0xf0, 0x09,
                    0x38, 0x76, 0x5f, 0x06, 0x74, 0x37, 0xf5, 0x82};
    authVector1.AddAuthAttribute(CK, AuthInfoAttribute(ckData, sizeof(ckData)));

    u8 ikData[] = {0x83, 0x04, 0x4b, 0x3f, 0xd3, 0xd6, 0x14, 0xca,
                   0x3f, 0xa9, 0xe6, 0x1e, 0xe4, 0x57, 0x20, 0xd3};
    authVector1.AddAuthAttribute(IK, AuthInfoAttribute(ikData, sizeof(ikData)));

    u8 autnData[] = {0x41, 0x19, 0x3c, 0x0f, 0x4c, 0xbc, 0x00, 0x00,
                     0x95, 0xf7, 0x4a, 0xef, 0xb1, 0x86, 0x74, 0x13};
    authVector1.AddAuthAttribute(AUTN, AuthInfoAttribute(autnData, sizeof(autnData)));
    assert(authVector1.TranslateQuintupletToTriplet());
    u8 expectedSresData[] = {0x8d, 0x8c, 0xee, 0x5f};
    std::vector<u8> expectedSres1(expectedSresData, expectedSresData + 4);
    u8 expectedKcData[] = {0x32, 0x48, 0xc2, 0x8c, 0x52, 0x3c, 0x31, 0x92};
    std::vector<u8> expectedKc1(expectedKcData, expectedKcData + 8);
    auto sresIt1 = authVector1.attrs.find(SRES);
    auto kcIt1 = authVector1.attrs.find(KC);
    assert(sresIt1->second.data == expectedSres1);
    char resultedKc[50];
    PrintBinaryDumpToString(resultedKc, sizeof(resultedKc), kcIt1->second.data.data(), kcIt1->second.data.size());
    assert(kcIt1->second.data == expectedKc1);
}

int main()
{
    TranslateQuintupletToTripletTest();
    return 0;
}

