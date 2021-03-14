//
// Created by liuze on 2021/2/2.
//

#include "PackageProcessWorker.h"
#include "PackageProcessTask.h"
#include <iostream>


void PackageProcessWorker::work(SimpleTask *task) {
    auto packageTask = dynamic_cast<PackageProcessTask*>(task);
    auto temp = packageTask->bytes;
   // helper->attachRSCode(temp, 3000, 1000);
    free(packageTask);
}

PackageProcessWorker::PackageProcessWorker() {
    helper = new RSHelper();
}
