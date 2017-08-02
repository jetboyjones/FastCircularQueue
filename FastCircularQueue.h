#ifndef _FAST_CIRCULAR_QUEUE_H
#define _FAST_CIRCULAR_QUEUE_H

#include <iostream>
#include <thread>
#include <future>
#include <vector>
#include <queue>
#include <list>

namespace el {
    using namespace std;

    /**
     * Callback definition for dropped entry handling.
     */
    template<class T>
    using DropCallback = std::function<void(T *)>;

    /**
     * A Proof of Concept Thread Safe queue class based on a circular buffer
     * (backed by an array) and using atomic operation instead of a global mutex.
     * Only a single thread can write but there can be multiple reader
     * threads. It stores pointers to type T.
     *
     * @tparam T
     */
    template<class T>
    class FastCircularQueue final {
        using QueueElement = std::atomic<T *>;

    public:
        /**
         * One and only constructor for the PoC Queue.
         *
         * @param size - The size of the backing array for the queue
         * @param purgeWindow - The number of items to purge if the writer overtakes the readers
         * @param callback - Routine to call for purged items - ideally this should be quick.
         */
        FastCircularQueue(size_t size, size_t purgeWindow, DropCallback<T> callback);

        /**
         * Note: This class is final so we don't need a vtable. If this class is changed to
         * be inheritable then this needs to become virtual.
         */
        ~FastCircularQueue();

        /**
         * Put an element onto the end of the queue - there can be only one thread doing
         * this.
         *
         * @param element - A pointer to type T
         */
        void push(T *element);

        /**
         * Pops and element off the front of the queue - this is thread-safe and there can
         * be multiple readers.
         *
         * @return An element of type T* from the from of the queue.
         */
        T *pop();

        /**
         * Reports if there are any elements currently in the queue - ephemeral if
         * writer and readers are active.
         *
         * @return true or false
         */
        bool isEmpty();

        /**
         * Reports the number of elements in the queue by counting non-null entries.
         * Only really valid if there are no active readers or writer.
         *
         * @return Number of non-null entries
         */
        size_t countElements();

    private: //Defaults
        //No reason to copy so disable
        FastCircularQueue(const FastCircularQueue &) = delete;

        FastCircularQueue &operator=(FastCircularQueue &) = delete;

    private: //Helpers
        // Helper function to remove old entries from the end of the queue.
        void expireOldEntries();

    private: //State
        const size_t mReadIdxLock = -1;

        int mBufferSize;
        int mPurgeWindow;
        int mWriteIdx;
        std::atomic<int> mReadIdx;
        std::atomic<int> mRWIndexOffset;
        DropCallback<T> mDropCallback;
        QueueElement *mBuffer;

    };

/* ************************
 * Implementation
 */

/*
 * Set up the queue
 */
    template<class T>
    FastCircularQueue<T>::FastCircularQueue(size_t size,
                                            size_t purgeWindow,
                                            DropCallback<T> dropCallback):
            mBufferSize(size),
            mPurgeWindow(purgeWindow),
            mWriteIdx(0), mReadIdx(0), mRWIndexOffset(0),
            mDropCallback(dropCallback),
            mBuffer(new QueueElement[size]) {

        for (int i = 0; i < size; ++i) {
            mBuffer[i] = nullptr;
        }

    }

/*
 * Delete the buffer
 */
    template<class T>
    FastCircularQueue<T>::~FastCircularQueue() {
        delete[] mBuffer;
    }

/*
 * Push an element onto the queue
 **/
    template<class T>
    void FastCircularQueue<T>::push(T *element) {//Only one thread allowed
        T *expected = nullptr;
        //The only thing we need to check in the writer is that we're not going
        //to overwrite the tail. A non-null entry means we've hit the tail. This
        //should actually not happen as a buffer is maintained by purging the tail.
        while (!mBuffer[mWriteIdx].compare_exchange_strong(expected, element)) {
            //We've overrun the readers;
            expected = nullptr;
        }
        //Increment the write index and wrap if necessary
        mWriteIdx = (mWriteIdx + 1) % mBufferSize;
        //Update the read index/write index offset
        ++mRWIndexOffset;
        //If we've caught up to the tail then start purging.
        if (mRWIndexOffset > mBufferSize - 10) { //10 is an arbitrary padding
            //Catching up to readers - Stop writing and purge the tail
            expireOldEntries();
        }
    }

/*
* Pop and element off the queue
*/
    template<class T>
    T *el::FastCircularQueue<T>::pop() {

        T *result = nullptr;
        while (result == nullptr) {
            //Load the current read index
            int currentReadIdx = mReadIdx.load();
            //If the current read index is not held by another thread - lock it.
            //This starts a critical section
            if ((currentReadIdx != mReadIdxLock) &&
                mReadIdx.compare_exchange_weak(currentReadIdx,
                                               mReadIdxLock,
                                               memory_order_release,
                                               memory_order_relaxed)) {
                //Only one thread should be here at any one time.
                if (mRWIndexOffset < 1) {
                    //If we're here we've caught up to the front of the queue. Reset the read
                    //index and try again.
                    mReadIdx = currentReadIdx;
                    continue;
                }

                //Grab the value from the buffer and set it to null.
                //If the writer thread is assured of not overwriting the tail
                //(which I think it is) then this could probably be replaced with
                // a non-atomic swap. This is mostly just a sanity check.
                T *current = mBuffer[currentReadIdx].load();
                if ((current != nullptr) &&
                    !mBuffer[currentReadIdx].compare_exchange_strong(current,
                                                                     nullptr,
                                                                     memory_order_release,
                                                                     memory_order_relaxed)) {
                    cout << "This should never fail!" << endl;
                }
                //Calculate a new read index
                int nextIdx = (currentReadIdx + 1) % mBufferSize;
                //Decrement the read/write index offset
                --mRWIndexOffset;
                //Just a sanity check
                if (mRWIndexOffset < 0) {
                    cout << "Ooops" << endl;
                }
                result = current;
                int test = mReadIdxLock;
                //Set the new read index.
                //This unlocks the the critical section. It should never fail so
                //the test is a sanity check and we could probably replace the atomic
                //swap it with an assignment.
                if (!mReadIdx.compare_exchange_strong(test, nextIdx)) {
                    std::cout << "This should never fail!" << std::endl;
                }
            } else {
                std::this_thread::yield(); //Yield to the next thread
                if (isEmpty()) break; //If the queue is empty, break out.
            }
        }

        return result;
    }

//Helper function to drop packets from the tail fo the queue. It's intended to
//be called from the the writer thread.
    template<class T>
    inline void FastCircularQueue<T>::expireOldEntries() {
        while (mRWIndexOffset > mBufferSize - mPurgeWindow) {
            T *dropMe = pop();
            if (mDropCallback != nullptr) {
                mDropCallback(dropMe);
            }
        }
    }

/*
 * Return a true if the read/write index is 0
 */
    template<class T>
    bool FastCircularQueue<T>::isEmpty() {
        return mRWIndexOffset <= 0;
    }

/*
 * Return a count of the non-null element in the queue - a fairly
 * bogus number if readers & writers are active.
 */
    template<class T>
    size_t FastCircularQueue<T>::countElements() {
        size_t result = 0;
        for (int i = 0; i < mBufferSize; ++i) {
            if (mBuffer[i] != nullptr) ++result;
        }
        return result;
    }
}

#endif // _FAST_CIRCULAR_QUEUE_H
