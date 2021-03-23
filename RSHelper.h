//
// Created by liuze on 2020/11/11.
//

#ifndef UDPSPEEDER_RSHELPER_H
#define UDPSPEEDER_RSHELPER_H
#include <iostream>
#define gwSize 256

class RSHelper {
private:
    static int num2field[gwSize];
    static int field2num[gwSize];
    static int multiTable[gwSize][gwSize];
    int generatorPolynomial[gwSize + 1];
    int generatorPolynomialTemp[gwSize + 1];
    int currentRSCodeLength;
    int polynomialValue[gwSize];
    int solveMatrix[gwSize][gwSize];
    void generateGeneratorPolynomial(int polynomialLength);
    bool getOriginMessage(uint8_t **packets, int originalLength, int rsLength, int offset, int batchLength);

public:

    static void init();

    void GenerateRSPacket(uint8_t **packets, int originalLength, int rsLength, size_t packetSize);

    bool GetOriginMessageFromPackets(uint8_t **packets, int originalLength, int rsLength, size_t packetSize);

};

#endif //UDPSPEEDER_RSHELPER_H
