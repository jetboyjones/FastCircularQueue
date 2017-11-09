#include "FastCircularQueue.h"

#include <iostream>
#include <future>
#include <vector>
#include <queue>


using std::shared_ptr;

/*************************************
 * A test item to enqueue and deque
 */
struct TestObject {
    int refcount;
    int id;
    std::mutex mutex;
    enum Op {
        None, Add, Rem, Purg
    } lastOp;

    TestObject() : refcount(0), id(-1), lastOp(None) {};
    TestObject(int recId) : refcount(0), id(recId), lastOp(None) {};
};

using TObj = shared_ptr<TestObject>;

/**************************************
 * Helper functions for testing
 */

// Record to store in history
struct LastOp {
    int id;
    TestObject::Op op;

    LastOp() : id(0), op(TestObject::Op::None) {};
};

using History = std::deque<LastOp *>;

//Add an Item to a history queue - history is constant size
void updateHistory(const TObj& rec, History& history) {
    //Remove record from front - alter it - put it on the back
    LastOp *op = history.front();
    history.pop_front();
    op->id = rec->id;
    op->op = rec->lastOp;
    history.push_back(std::move(op));
}

//Mutex for printing
static std::mutex cout_mutex;

//Print out a history queue
void dumpHistory(const History &history) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    for (LastOp *op: history) {
        std::cout << "(" << op->id << "," << op->op << ")";
    }
    std::cout << std::endl;
}

//Setup a history queue - queue is constant size
void initHistory(History &history, size_t size) {
    for (int i = 0; i < size; ++i) {
        LastOp *p = new LastOp;
        history.push_back(p);
    }
}


/*****************************************
 * Thread Loops
 */
//Only the writer thread writes to this
thread_local History writeHistory;

//Global done
bool done = false;

//Writer Thread Loop
void writerTask(el::FastCircularQueue<TObj> &queue, std::vector<TObj> &values, int reps) {

    initHistory(writeHistory, 15);
    size_t idx = 0;
    int iterations = 0;

    while (!done) {
        auto rec = values.data()[idx];
        queue.push(rec);

        {
            //This critical section maintains the integrity of the record
            //but this thread could have previously yielded to a reader thread handling
            //the same record so refecout could go from -1 to 0 instead of 0 to 1.
            std::lock_guard<std::mutex> lock(rec->mutex);
            ++rec->refcount;
            rec->lastOp = rec->Add;
            if (rec->refcount > 1) {
                std::cout << "Refcount > 1 = " << rec->refcount;
                std::cout << " (" << rec->id << ")" << std::endl;
            }
            updateHistory(rec, writeHistory);
        }

        idx = (idx + 1) % values.size();

        done = (++iterations) >= values.size() * reps;
    }

    dumpHistory(writeHistory);
}

//Metric for counting the number of purge events
static int gPurgeCount = 0;

//Callback for handling items removed via purge
void dropHandler(TObj& rec) {

    {
        //This critical section maintains the integrity of the record
        //but could result in a refcount < 0 because the write thread may
        //have yielded before it updated the refcount
        std::lock_guard<std::mutex> lock(rec->mutex);
        rec->lastOp = rec->Purg;
        --rec->refcount;
        if (rec->refcount < -1) {
            std::cout << "Refcount < -1 = " << rec->refcount;
            std::cout << " (" << rec->id << ")" << std::endl;
        }
    }

    ++gPurgeCount;
    updateHistory(rec, writeHistory);
}

//Reader Thread Loop
void readerTask(el::FastCircularQueue<TObj> &queue) {
    History history;
    initHistory(history, 15);

    while (!done || !queue.isEmpty()) {
        auto rec = queue.pop();
        //This critical section maintains the integrity of the record
        //but could result in a refcount < 0 because the write thread may
        //have yielded before it updated the refcount
        std::lock_guard<std::mutex> lock(rec->mutex);
        rec->lastOp = rec->Rem;
        --rec->refcount;
        if (rec->refcount < 0) {
            std::cout << "Refcount < 0 = " << rec->refcount;
            std::cout << " (" << rec->id << ")" << std::endl;
        }

        updateHistory(rec, history);
    }

    dumpHistory(history);
}


int main() {

    //Set up the queue
    int queueSize = 10000;
    int purgeWindow = 100;
    el::FastCircularQueue<TObj> fastQueue(
            queueSize,
            purgeWindow,
            dropHandler);

    //Allocate some test objects
    std::vector<TObj> testObjectVec;
    testObjectVec.reserve(queueSize * 2); //Double the queue size
    for (int i = 0; i < queueSize * 2; ++i) {
        testObjectVec.emplace_back(new TestObject(i));
    }

    //Start up the writer thread
    //Number of time to enqueue the test buffer
    //repitions * testObjectVec.size() needs to fit into an int
    const int repitions = 1000;
    auto writer = std::async(std::launch::async,
                             writerTask,
                             std::ref(fastQueue),
                             std::ref(testObjectVec),
                             repitions);

    //Start up the reader threads
    const int nWorkers = 6; //Number of worker threads
    std::vector<std::future<void>> wrkrs;
    for (int i = 0; i < nWorkers; ++i) {
        auto wrkr = std::async(std::launch::async, readerTask, std::ref(fastQueue));
        wrkrs.push_back(move(wrkr));
    }

    //Wait for the writer thread to finish
    writer.get();
    std::cout << "Write Thread Done" << std::endl;

    //Wait for the reader threads to finish
    for (int i = 0; i < nWorkers; ++i) {
        wrkrs[i].get();
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Worker " << i << " Thread Done" << std::endl;
    }

    //Print out run metrics
    int maxRef = 0;
    int minRef = 0;
    for (TObj& rec: testObjectVec) {
        if (rec->refcount > maxRef) maxRef = rec->refcount;
        if (rec->refcount < minRef) minRef = rec->refcount;
    }

    size_t count = fastQueue.countElements();
    std::cout << std::endl;
    std::cout << "Records left in queue = " << count << std::endl;
    std::cout << "Purge Count = " << gPurgeCount << std::endl;
    std::cout << "Max Ref = " << maxRef << ", Min Ref = " << minRef << std::endl;

    return 0;
}