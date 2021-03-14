//
// Created by liuze on 2021/1/24.
//

#ifndef UDPSPEEDER_SIMPLEWORKQUEUE_H
#define UDPSPEEDER_SIMPLEWORKQUEUE_H


#include "SimpleWorker.h"
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

class SimpleWorkQueue {
public:
    class SimpleThreadsSafeQueue {
    private:
        std::queue<SimpleTask*>que;
        std::mutex mMutex;
        std::condition_variable mCondVar;
    public:
        SimpleTask* offer() {
            std::unique_lock <std::mutex> uniqueLock(mMutex);
            while(que.empty()) {
                mCondVar.wait(uniqueLock);
            }
            auto ret = que.front();
            que.pop();
            return ret;
        }
        void push(const SimpleTask* task) {
            std::unique_lock <std::mutex> uniqueLock(mMutex);
            que.push(const_cast<SimpleTask *>(task));
            if (que.size() == 1) {
                mCondVar.notify_all();
            }
        }
    };
    SimpleWorkQueue(int workerSize, SimpleWorker** simpleWorkers);
    void submitTask(SimpleTask *task);
    void finish();
private:
    SimpleWorker **workers{};
    unsigned int workerSize;
    const static SimpleTask *finishTask;
    std::atomic<bool>finished{};
    static void simpleWorkFunc(SimpleWorker *simpleWorker, SimpleThreadsSafeQueue* queue);
    SimpleThreadsSafeQueue *queue;

};


#endif //UDPSPEEDER_SIMPLEWORKQUEUE_H
