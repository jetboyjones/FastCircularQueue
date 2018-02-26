#include "gtest/gtest.h"
#include "../FastCircularQueue.h"

using std::shared_ptr;
using namespace el;

/*************************************
 * A test item to enqueue and deque
 */
struct TestObject {
    int refcount;
    int id;
    enum Op {
        None, Add, Rem, Purg
    } lastOp;

    TestObject() : refcount(0), id(-1), lastOp(None) {};
    TestObject(int recId) : refcount(0), id(recId), lastOp(None) {};
};

using TObj = shared_ptr<TestObject>;

static int cNumPurged = 0;

void dropHandler(TObj& rec) {
    ++cNumPurged;
}

/*************************************
 * Google test fixture
 */
class FSQTest : public ::testing::Test {
protected:
    static const size_t cBufferSize = 20;
    static const size_t cPurgeLength = 2;

protected:
    FSQTest() : mQueue(cBufferSize, cPurgeLength, dropHandler) {}

    virtual void SetUp(){
    }

    FastCircularQueue<TObj> mQueue;
};

/************************************
 * Tests
 */

TEST_F(FSQTest, TestInitialization) {
    EXPECT_TRUE(mQueue.isEmpty());
    EXPECT_EQ(mQueue.countElements(), 0);
}

TEST_F(FSQTest, TestPushOne) {
    TObj testObj(new TestObject());
    mQueue.push(testObj);

    EXPECT_FALSE(mQueue.isEmpty());
    EXPECT_EQ(mQueue.countElements(), 1);
}


TEST_F(FSQTest, TestPushMulti) {
    TObj testObj(new TestObject());
    const int n = cBufferSize;
    for (int i = 0; i < n; ++i) {
        mQueue.push(testObj);
    }

    EXPECT_FALSE(mQueue.isEmpty());
    EXPECT_EQ(mQueue.countElements(), n);
}

TEST_F(FSQTest, TestPushExpireAndCallback) {
    TObj testObj(new TestObject());
    const int n = cBufferSize+1;
    for (int i = 0; i < n; ++i) {
        mQueue.push(testObj);
    }

    EXPECT_FALSE(mQueue.isEmpty());
    EXPECT_EQ(mQueue.countElements(), n - cPurgeLength);
    int np = cPurgeLength;
    EXPECT_EQ(cNumPurged, np);
}

TEST_F(FSQTest, TestPopOne) {
    TObj testObj(new TestObject());
    mQueue.push(testObj);

    TObj ele = mQueue.pop();

    EXPECT_TRUE(mQueue.isEmpty());
    EXPECT_EQ(mQueue.countElements(), 0);
}

TEST_F(FSQTest, TestPopMult) {
    TObj testObj(new TestObject());
    const int n = cBufferSize;
    for (int i = 0; i < n; ++i) {
        mQueue.push(testObj);
        EXPECT_FALSE(mQueue.isEmpty());
        EXPECT_EQ(mQueue.countElements(), i+1);
    }

    for (int i = 0; i < n; ++i) {
        TObj ele = mQueue.pop();
        EXPECT_EQ(mQueue.countElements(), n-i-1);
    }

    EXPECT_TRUE(mQueue.isEmpty());
    EXPECT_EQ(mQueue.countElements(), 0);
}

