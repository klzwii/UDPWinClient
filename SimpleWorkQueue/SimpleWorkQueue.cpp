//
// Created by liuze on 2021/1/24.
//

#include "SimpleWorkQueue.h"
#include <iostream>

const SimpleTask* SimpleWorkQueue::finishTask = new SimpleTask();

SimpleWorkQueue::SimpleWorkQueue(int workerSize, SimpleWorker **simpleWorkers) {
    finished = false;
    this->workerSize = workerSize;
    queue = new SimpleThreadsSafeQueue();
    for (int i = 0; i < workerSize; i ++) {
        std::thread t1(simpleWorkFunc, simpleWorkers[i], queue);
        t1.detach();
    }
}

void SimpleWorkQueue::simpleWorkFunc(SimpleWorker *simpleWorker, SimpleThreadsSafeQueue* queue) {
    while (true) {
        SimpleTask *task = queue->offer();
        if (task == finishTask) {
            break;
        }
        simpleWorker->work(task);
    }
}

void SimpleWorkQueue::submitTask(SimpleTask *task) {
    if (finished) {
        return;
    }
    queue->push(task);
}

void SimpleWorkQueue::finish() {
    finished.store(true);
    for (int i = 0; i < workerSize; i ++) {
        queue->push(finishTask);
    }
}

