# FastCircularQueue
A simple proof of concept circular queue for one writer and multiple readers using atomic operations.

The way it works is that the writer adds entries using a "write index" and the readers read
entries using a "read index" which the various readers may first lock down via CAS and a special
marker. The writer is prevented from passing up the readers using a read index/write index offset
counter as a sort of semaphore: if the counter approaches the size of the circular buffer then the 
writer start purging entries from the tail of the queue.

It is quick a dirty code and I haven't compared it's performance to using a normal mutex but I have used
something similar on a project with success.

This PoC is not tuned and I'm sure it could be improved but I think it's valid and without 
race-conditions.

There are three files: 
  - FastCircularQueue.h (it's a template class)
  - main.cpp            (code for running and testing and usage example)
  - CMakeList.txt     
  
  I used CLion and you may be able to just import the project if you use CLion.
