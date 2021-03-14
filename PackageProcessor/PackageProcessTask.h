//
// Created by liuze on 2021/2/2.
//

#ifndef UDPSPEEDER_PACKAGEPROCESSTASK_H
#define UDPSPEEDER_PACKAGEPROCESSTASK_H


#include "../SimpleWorkQueue/SimpleTask.h"
#include "../RSHelper.h"
#include <atomic>

class PackageProcessTask : public SimpleTask {
public:
    unsigned char* bytes{};
    unsigned char* bytesCopy{};
    int messageLength, rsCodeLength;
    std::atomic_bool isFinished{};
    PackageProcessTask(unsigned char* message, unsigned char* messageCopy,int messageLength, int rsCodeLength) {
        this->bytes = message;
        this->messageLength = messageLength;
        this->rsCodeLength = rsCodeLength;
        this->isFinished.store(false);
    };
    ~PackageProcessTask() override {
        free(bytes);
    }
};


#endif //UDPSPEEDER_PACKAGEPROCESSTASK_H
