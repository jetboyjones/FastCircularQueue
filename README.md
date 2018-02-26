FastCircularQueue

A simple proof of concept circular queue C++-11 template for one writer and multiple readers using C++-11 atomic operations.

The way it works is that the writer adds entries using a "write index" and the readers read entries using a "read index" which the various readers may first lock down via CAS and a special marker. The writer is prevented from passing up the readers using a read index/write index offset counter as a sort of semaphore: if the counter approaches the size of the circular buffer then the writer start purging entries from the tail of the queue.

It is quick a dirty code and I haven't compared it's performance to using a normal mutex but I have used something similar on a project with success.

This PoC is not tuned and I'm sure it could be improved but I think it's valid and without race-conditions.

Also included is a tla "formal specification" that I used to validate
the concurrent state space.

These are the files:
  - CMakeList.txt     
  - FastCircularQueue.h (it's a template class)
  - FCQ.tla (A tla formal spec for concurency state validation)
  - test
		- CMakeList.txt     
		- UnitTest.cpp      (some basic GTest based unit tests)
		- FunctionTest.cpp  (A functional test - which is also a good usage example)

I used CMake and To build and run the tests:1
  1) mkdir build - Create a build subdirectory
  2) cd build
  3) cmake ../
     (Note that cmake will auto downlaod the GTest stuff)
  4) make all
  5) make test
  
Also, I used CLion and you may be able to just import the project if you use CLion.


