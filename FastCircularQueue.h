/*
FastCircularQueue
Copyright (C)  2017  Earthlight Research, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details
(see <http://www.gnu.org/licenses/>).
*/

#ifndef _FAST_CIRCULAR_QUEUE_H
#define _FAST_CIRCULAR_QUEUE_H

#include <functional>
#include <atomic>
#include <thread>

namespace el {
    using namespace std;

    /**
     * Callback definition for dropped entry handling.
     */
    template<class T>
    using DropCallback = std::function<void(T&)>;

    /**
     * A Proof of Concept Thread Safe queue template class template based on a circular buffer
     * (backed by an array) using atomic operation instead of a global mutex.
     * In this implementation a single writer thread and multiple reader threads are supported.
     * If the write index overruns the read index, packets from the front of the queue
     * (the oldest) are purged to make room for new entries. The queue stores copies of T.
     *
     * See the formal spec (TLA+) for algorithm verification.
     *
     * @tparam T Type of object stored in the Queue - ideally use a smart pointer of some sort.
     */
    template<class T>
    class FastCircularQueue final {

    public:
        /**
         * One and only constructor for the PoC Queue.
         *
         * @param size - The size of the backing array for the queue
         * @param expireSize - The number of items to purge if the writer overtakes the readers
         * @param dropCallback - Routine to call for purged items - ideally this should be quick.
         */
        FastCircularQueue(size_t size, size_t expireSize, DropCallback<T> dropCallback);

        /**
         * Note: This class is final so we don't need a vtable. If this class is changed to
         * be inheritable then this needs to become virtual.
         */
        ~FastCircularQueue();

        /**
         * Put an element onto the end of the queue - there can be only one thread doing
         * this.
         *
         * @param element - An object of type T
         */
        void push(const T& element);

        /**
         * Pops and element off the front of the queue - this is thread-safe and there can
         * be multiple readers.
         *
         * @return An element of type T* from the from of the queue.
         */
        T pop();

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
        const uint mReadIdxLock = -1;

        uint mBufferSize;
        uint mExpireSize;
        uint mWriteIdx;
        std::atomic<uint> mReadIdx; //Used as read index and to lock reader critical section
        std::atomic<uint> mRWIndexOffset; //Semaphore for read/write collision (also buffer count)
        DropCallback<T> mDropCallback;
        T* mBuffer;

    };

/* ************************
 * Implementation
 */

/*
 * Set up the queue
 */
    template<class T>
    FastCircularQueue<T>::FastCircularQueue(size_t size,
                                            size_t expireSize,
                                            DropCallback<T> dropCallback):
            mBufferSize(size),
            mExpireSize(expireSize),
            mWriteIdx(0), mReadIdx(0), mRWIndexOffset(0),
            mDropCallback(dropCallback),
            mBuffer(new T[size]) {
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
    void FastCircularQueue<T>::push(const T& element) {//Only one thread allowed

        //Check the Read/Write semaphore
        if (mRWIndexOffset >= mBufferSize) {
            //We've overrun the end of the queue - drop oldest entries to make room;
            expireOldEntries();
        }

        //Add the element;
        mBuffer[mWriteIdx] = element;

        //Increment the write index and wrap if necessary
        mWriteIdx = (mWriteIdx + 1) % mBufferSize;

        //Update the read index/write index offset - this needs to be atomic
        ++mRWIndexOffset;

    }

/*
* Pop and element off the queue
*/
    template<class T>
    T el::FastCircularQueue<T>::pop() {

        T result;
        bool done = false;
        while (!done) {
            //Load the current read index
            uint currentReadIdx = mReadIdx;

            //If the current read index is not held by another thread - lock it.
            //This starts a critical section
            if ((currentReadIdx != mReadIdxLock) &&
                mReadIdx.compare_exchange_weak(currentReadIdx,
                                               mReadIdxLock,
                                               memory_order_release,
                                               memory_order_relaxed)) {
                // CS: Start of Critical Section for readers
                // Check Read/Write Semaphore
                if (mRWIndexOffset < 1) {
                    //If we're here we've caught up to the front of the queue. Reset the read
                    //index and try again.
                    mReadIdx = currentReadIdx;
                    continue;
                }

                //Fetch the value from the buffer
                result = mBuffer[currentReadIdx];

                //Decrement the read/write index offset - this must be atomic and come before
                //update to mReadIdx
                --mRWIndexOffset;

                //Calculate a new read index - does not have to be atomic
                mReadIdx = (currentReadIdx + 1) % mBufferSize;

                done = true;
                // CS: End of Critical Section
            } else {
                std::this_thread::yield(); //Yield to the next thread
                if (isEmpty()) break; //If the queue is empty, break out.
            }
        }

        return result;
    }

//Helper function to drop packets from the tail of the queue. It's intended to
//be called from the the writer thread.
    template<class T>
    inline void FastCircularQueue<T>::expireOldEntries() {
        while (mRWIndexOffset > mBufferSize - mExpireSize) {
            T dropMe = pop();
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
        return mRWIndexOffset;
    }
}

#endif // _FAST_CIRCULAR_QUEUE_H
