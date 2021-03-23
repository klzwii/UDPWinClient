//
// Created by liuze on 2020/11/11.
//

#include "RSHelper.h"
#include <cstring>
#include <algorithm>

//G(2^8) log2num
int RSHelper::num2field[gwSize] = {1, 2, 4, 8, 16, 32, 64, 128, 29, 58, 116, 232, 205, 135, 19, 38, 76, 152, 45, 90, 180, 117, 234, 201, 143, 3, 6, 12, 24, 48, 96, 192, 157, 39, 78, 156, 37, 74, 148, 53, 106, 212, 181, 119, 238, 193, 159, 35, 70, 140, 5, 10, 20, 40, 80, 160, 93, 186, 105, 210, 185, 111, 222, 161, 95, 190, 97, 194, 153, 47, 94, 188, 101, 202, 137, 15, 30, 60, 120, 240, 253, 231, 211, 187, 107, 214, 177, 127, 254, 225, 223, 163, 91, 182, 113, 226, 217, 175, 67, 134, 17, 34, 68, 136, 13, 26, 52, 104, 208, 189, 103, 206, 129, 31, 62, 124, 248, 237, 199, 147, 59, 118, 236, 197, 151, 51, 102, 204, 133, 23, 46, 92, 184, 109, 218, 169, 79, 158, 33, 66, 132, 21, 42, 84, 168, 77, 154, 41, 82, 164, 85, 170, 73, 146, 57, 114, 228, 213, 183, 115, 230, 209, 191, 99, 198, 145, 63, 126, 252, 229, 215, 179, 123, 246, 241, 255, 227, 219, 171, 75, 150, 49, 98, 196, 149, 55, 110, 220, 165, 87, 174, 65, 130, 25, 50, 100, 200, 141, 7, 14, 28, 56, 112, 224, 221, 167, 83, 166, 81, 162, 89, 178, 121, 242, 249, 239, 195, 155, 43, 86, 172, 69, 138, 9, 18, 36, 72, 144, 61, 122, 244, 245, 247, 243, 251, 235, 203, 139, 11, 22, 44, 88, 176, 125, 250, 233, 207, 131, 27, 54, 108, 216, 173, 71, 142, 1};
//G(2^8) num2log
int RSHelper::field2num[gwSize] = {0, 255, 1, 25, 2, 50, 26, 198, 3, 223, 51, 238, 27, 104, 199, 75, 4, 100, 224, 14, 52, 141, 239, 129, 28, 193, 105, 248, 200, 8, 76, 113, 5, 138, 101, 47, 225, 36, 15, 33, 53, 147, 142, 218, 240, 18, 130, 69, 29, 181, 194, 125, 106, 39, 249, 185, 201, 154, 9, 120, 77, 228, 114, 166, 6, 191, 139, 98, 102, 221, 48, 253, 226, 152, 37, 179, 16, 145, 34, 136, 54, 208, 148, 206, 143, 150, 219, 189, 241, 210, 19, 92, 131, 56, 70, 64, 30, 66, 182, 163, 195, 72, 126, 110, 107, 58, 40, 84, 250, 133, 186, 61, 202, 94, 155, 159, 10, 21, 121, 43, 78, 212, 229, 172, 115, 243, 167, 87, 7, 112, 192, 247, 140, 128, 99, 13, 103, 74, 222, 237, 49, 197, 254, 24, 227, 165, 153, 119, 38, 184, 180, 124, 17, 68, 146, 217, 35, 32, 137, 46, 55, 63, 209, 91, 149, 188, 207, 205, 144, 135, 151, 178, 220, 252, 190, 97, 242, 86, 211, 171, 20, 42, 93, 158, 132, 60, 57, 83, 71, 109, 65, 162, 31, 45, 67, 216, 183, 123, 164, 118, 196, 23, 73, 236, 127, 12, 111, 246, 108, 161, 59, 82, 41, 157, 85, 170, 251, 96, 134, 177, 187, 204, 62, 90, 203, 89, 95, 176, 156, 169, 160, 81, 11, 245, 22, 235, 122, 117, 44, 215, 79, 174, 213, 233, 230, 231, 173, 232, 116, 214, 244, 234, 168, 80, 88, 175};

int RSHelper::multiTable[gwSize][gwSize];

static inline int modnn(int x)
{
    while (x >= 255) {
        x -= 255;
        x = (x >> 8) + (x & 255);
    }
    return x;
}

void RSHelper::generateGeneratorPolynomial(int polynomialLength) {
    memset(generatorPolynomial, 0, sizeof(generatorPolynomial));
    generatorPolynomial[0] = field2num[1];
    generatorPolynomial[1] = field2num[1];
    for (int i = 2; i <= polynomialLength; i++) {
        memset(generatorPolynomialTemp, 0, sizeof(generatorPolynomialTemp));
        generatorPolynomialTemp[0] = generatorPolynomial[0] + (i - 1);
        generatorPolynomialTemp[0] %= (gwSize - 1);
        for (int j = 1; j < i; j++) {
            generatorPolynomialTemp[i - j] = generatorPolynomial[i - j - 1];
        }
        for (int j = 1; j < i; j++) {
            int temp = generatorPolynomial[j] + i - 1;
            temp %= (gwSize - 1);
            temp = num2field[temp];
            temp ^= num2field[generatorPolynomialTemp[j]];
            generatorPolynomialTemp[j] = field2num[temp];
        }
        std::swap(generatorPolynomial, generatorPolynomialTemp);
    }
}

void RSHelper::GenerateRSPacket(uint8_t **packets, int originalLength, int rsLength, size_t packetSize) {
    unsigned char tempPolynomial[gwSize];
    size_t batchLength = (gwSize - 1) / (originalLength + rsLength);
    int batch = (packetSize - 1) / batchLength + 1;
    for (int currentBatch = 0; currentBatch < batch; currentBatch ++) {
        auto copySize = std::min(packetSize - currentBatch * batchLength , batchLength);
        for (int currentPacket = 0; currentPacket < originalLength; currentPacket ++) {
            memset(tempPolynomial, 0, sizeof(tempPolynomial));
            for (int i = rsLength; i < rsLength + originalLength; i ++) {
                memcpy(tempPolynomial + i * copySize, packets[i] + currentBatch * batchLength, copySize);
            }
            auto messageLength = originalLength * copySize;
            auto rsCodeLength = rsLength * copySize;
            if (rsCodeLength != currentRSCodeLength) {
                currentRSCodeLength = rsCodeLength;
                generateGeneratorPolynomial(rsCodeLength);
            }
            for (auto i = messageLength + rsCodeLength; i >= rsCodeLength; i--) {
                if (tempPolynomial[i]) {
                    int addFactor = field2num[tempPolynomial[i]];
                    for (int j = 0; j <= rsCodeLength; j++) {
                        int tempFactor = generatorPolynomial[rsCodeLength - j];
                        tempFactor = (tempFactor + addFactor) % (gwSize - 1);
                        tempFactor = num2field[tempFactor];
                        tempPolynomial[i - j] ^= tempFactor;
                    }
                }
            }
        }
        for (int i = 0; i < rsLength; i ++) {
            memcpy(packets[i] + currentBatch * batchLength, tempPolynomial + i * copySize, copySize);
        }
    }
}

bool RSHelper::GetOriginMessageFromPackets(uint8_t** packets, int originalLength, int rsLength, size_t packetSize) {
    size_t batchLength = (gwSize - 1) / (originalLength + rsLength);
    int batch = (packetSize - 1) / batchLength + 1;
    for (auto currentBatch = 0; currentBatch < batch; currentBatch ++) {
        auto copySize = std::min(packetSize - currentBatch * batchLength, batchLength);
        if (!getOriginMessage(packets, originalLength, rsLength, currentBatch * batchLength, copySize)) {
            return false;
        }
    }
    return true;
}

bool RSHelper::getOriginMessage(uint8_t** packets, int originalLength, int rsLength, int offset, int  batchLength) {
    int isWrong = 0;
    auto rsCodeLength = batchLength * rsLength;
    auto messageLength = batchLength * originalLength;
    for (int i = 0; i < rsCodeLength; i++) {
        int tempAns = 0;
        for (int j = rsCodeLength + messageLength - 1; j >= 0; j --) {
            int word = packets[j / batchLength][j % batchLength + offset];
            if (word == 0) {
                continue;
            }
            int c = modnn(j*i);
            tempAns ^= multiTable[num2field[c]][word];
        }
        polynomialValue[i] = tempAns;
        isWrong |= tempAns;
    }
    if (!isWrong) {
        return true;
    }
    int maxWrongPos = rsCodeLength >> 1;  //初始假设错误有 rsCodeLength>>1 个，检验矩阵是否满秩
    int matrixRank = 0;
    while (true) {
        for (int i = 0; i < maxWrongPos; i++) {
            memcpy(solveMatrix[i], &polynomialValue[0] + i, maxWrongPos * sizeof(int));
        }
        // 开始将矩阵转换为行阶梯形式
        for (int i = 0; i < maxWrongPos; i++) {
            bool done = false;
            bool finished = true;
            for (int j = i; j < maxWrongPos; j++) {
                for (int k = i; k < maxWrongPos; k++) {
                    if (solveMatrix[k][j]) {
                        finished = false;
                        matrixRank++;
                        std::swap(solveMatrix[k], solveMatrix[i]);
                        int tempAlpha = field2num[solveMatrix[i][j]];
                        for (int kk = j; kk < maxWrongPos; kk++) {
                            if (!solveMatrix[i][kk]) {
                                continue;
                            }
                            solveMatrix[i][kk] = multiTable[solveMatrix[i][kk]][num2field[255 - tempAlpha]];
                        }
                        for (int kc = k + 1; kc < maxWrongPos; kc++) {
                            int tempBeta = field2num[solveMatrix[kc][j]];
                            if (solveMatrix[kc][j]) {
                                for (int kk = j; kk < maxWrongPos; kk++) {
                                    if (!solveMatrix[i][kk]) {
                                        continue;
                                    }
                                    solveMatrix[kc][kk] ^= multiTable[solveMatrix[i][kk]][num2field[tempBeta]];
                                }
                            }
                        }
                        done = true;

                        break;
                    }
                }
                if (done) {
                    break;
                }
            }
            if (finished) {
                break;
            }
        }
        if (matrixRank == maxWrongPos) { //当计算出来的矩阵秩和假设的错误数相同时，代表当前矩阵满秩同时错误数与矩阵秩相同
            break;                       //(当错误过多时，矩阵秩为rsCodeLength>>1，且无法恢复错误）
        } else {
            maxWrongPos = matrixRank; //当前矩阵若不是满秩，其错误数最小是当前矩阵秩个，因此在下一次迭代可以直接将矩阵的最大秩设置为当前秩
            matrixRank = 0;
        }
    }
    for (int i = 0; i < matrixRank; i++) {
        memcpy(solveMatrix[i], &polynomialValue[0] + i, (matrixRank + 1) * sizeof(int));
    }
    // 高斯消元法进行定位多项式系数求解
    for (int i = 0; i < matrixRank; i++) {
        if (!solveMatrix[i][i]) {
            for (int j = i + 1; j < matrixRank; j++) {
                if (solveMatrix[j][i]) {
                    std::swap(solveMatrix[i], solveMatrix[j]);
                }
            }
        }
        int tempAlpha = field2num[solveMatrix[i][i]];
        for (int j = i; j <= matrixRank; j++) {
            if (solveMatrix[i][j] == 0)
                continue;
            solveMatrix[i][j] = multiTable[solveMatrix[i][j]][num2field[255 - tempAlpha]];
        }
        for (int j = i + 1; j < matrixRank; j++) {
            if (!solveMatrix[j][i]) {
                continue;
            }
            int tempBeta = solveMatrix[j][i];
            for (int k = i; k <= matrixRank; k++) {
                int tempFactor = 0;
                if (solveMatrix[i][k]) {
                    tempFactor = multiTable[tempBeta][solveMatrix[i][k]];
                }
                solveMatrix[j][k] ^= tempFactor;
            }
        }
    }
    for (int i = matrixRank - 1; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            if (!solveMatrix[j][i]) {
                continue;
            }
            if (!solveMatrix[i][matrixRank]) {
                solveMatrix[j][i] = 0;
                continue;
            }
            solveMatrix[j][matrixRank] ^= multiTable[solveMatrix[i][matrixRank]][solveMatrix[j][i]];
            solveMatrix[j][i] = 0;
        }
    }
    int gaussAns[(gwSize >> 1) + 1];
    for (int i = 0; i < matrixRank; i++) {
        gaussAns[i] = solveMatrix[i][matrixRank];
    }
    // 高斯消元法结束 开始通过求得定位多项式进行错误位置求解
    int sumWrongPos = 0;
    int wrongPos[gwSize >> 1];
    for (int i = 0; i < rsCodeLength + messageLength; i++) {
        int calcAns = 1;
        for (int j = 0; j < matrixRank; j++) {
            if (!gaussAns[matrixRank - j - 1]) {
                continue;
            }
            int tempFactor = field2num[gaussAns[matrixRank - j - 1]];
            tempFactor -= modnn(i * (j + 1));
            tempFactor = modnn(tempFactor + 255);
            calcAns ^= num2field[tempFactor];
        }
        if (!calcAns) {
            wrongPos[sumWrongPos++] = i;
        }
    }

    //如果求出的错误位置和矩阵的秩不相同 代表错误过多无法消除(或者有bug TAT)
    if (sumWrongPos != matrixRank) {
        return false;
    }

    for (int i = 0; i < sumWrongPos; i++) {
        for (int j = 0; j < sumWrongPos; j++) {
            //std::cout << wrongPos[j] << " " << i << " " << j << std::endl;
            solveMatrix[i][j] = num2field[modnn(wrongPos[j] * i)];
        }
        solveMatrix[i][sumWrongPos] = polynomialValue[i];
    }

    // 高斯消元法进行错误恢复
    for (int i = 0; i < matrixRank; i++) {
        if (!solveMatrix[i][i]) {
            for (int j = i + 1; j < matrixRank; j++) {
                if (solveMatrix[j][i]) {
                    std::swap(solveMatrix[i], solveMatrix[j]);
                }
            }
        }
        int tempAlpha = field2num[solveMatrix[i][i]];
        for (int j = i; j <= matrixRank; j++) {
            if (solveMatrix[i][j] == 0)
                continue;
            solveMatrix[i][j] = multiTable[solveMatrix[i][j]][num2field[255 - tempAlpha]];
        }
        for (int j = i + 1; j < matrixRank; j++) {
            if (!solveMatrix[j][i]) {
                continue;
            }
            int tempBeta = solveMatrix[j][i];
            for (int k = i; k <= matrixRank; k++) {
                int tempFactor = 0;
                if (solveMatrix[i][k]) {
                    tempFactor = multiTable[tempBeta][solveMatrix[i][k]];
                }
                solveMatrix[j][k] ^= tempFactor;
            }
        }
    }
    for (int i = matrixRank - 1; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            if (!solveMatrix[j][i]) {
                continue;
            }
            if (!solveMatrix[i][matrixRank]) {
                solveMatrix[j][i] = 0;
                continue;
            }
            solveMatrix[j][matrixRank] ^= multiTable[solveMatrix[j][i]][solveMatrix[i][matrixRank]];
            solveMatrix[j][i] = 0;
        }
    }

    for (int i = 0; i < matrixRank; i++) {
        gaussAns[i] = solveMatrix[i][matrixRank];
    }
    for (int i = 0; i < sumWrongPos; i++) {
        packets[wrongPos[i]/batchLength][offset + wrongPos[i]%batchLength] ^= gaussAns[i];
    }
    isWrong = 0;
    for (int i = 0; i < rsCodeLength; i++) {
        int tempAns = 0;
        for (int j = rsCodeLength + messageLength - 1; j >= 0; j --) {
            int word = packets[j / batchLength][j % batchLength + offset];
            if (word == 0) {
                continue;
            }
            tempAns ^= multiTable[num2field[modnn(j * i)]][word];
        }
        polynomialValue[i] = tempAns;
        isWrong |= tempAns;
    }
    return !isWrong;
}

void RSHelper::init() {
    memset(multiTable, 0, sizeof(multiTable));
    for (int i = 0; i < gwSize; i ++) {
        for (int j = 0; j < gwSize; j ++) {
            auto p = field2num[i] + field2num[j];
            p %= (gwSize - 1);
            multiTable[i][j] = num2field[p];
        }
    }
}
