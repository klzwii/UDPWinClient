//
// Created by liuze on 2021/1/24.
//

#ifndef UDPSPEEDER_SIMPLEWORKER_H
#define UDPSPEEDER_SIMPLEWORKER_H

#include "SimpleTask.h"

class SimpleWorker {
public:
    virtual void work(SimpleTask* task) = 0;
};


#endif //UDPSPEEDER_SIMPLEWORKER_H
