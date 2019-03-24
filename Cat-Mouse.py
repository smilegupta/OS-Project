from threading import Lock, Thread, Event
import random 
from time import sleep

lock = Lock()
NumBowls = 5
NumCats = 4
NumMice = 6
Bowls = []
NumBites = 5
for i in range(NumBowls):
  Bowls.append(random.randrange(1, 20))
  
def Cat(lock):
  global Bowls
  for _ in range(NumBites):
    print("Cat sleeping\n")
    Event().wait(0.1)
    print("Cat eating\n")
    lock.acquire()
    Event().wait(0.1)
    for i, Bowl in enumerate(Bowls):
      if(Bowl != 0):
        Bowls[i] -= 1
        pass
    lock.release()
    
def Mouse(lock):
  global Bowls
  for _ in range(NumBites):
    print("Mouse sleeping\n")
    Event().wait(0.1)
    print("Mouse eating\n")
    lock.acquire()
    Event().wait(0.1)
    for i, Bowl in enumerate(Bowls):
      if(Bowl != 0):
        Bowls[i] -= 1
        pass
    lock.release()
    
Cats = []
for _Cat in range(NumCats):
  Cats.append(Thread(target=Cat, args=(lock,)))
 
Mice = []
for _Mouse in range(NumCats):
  Mice.append(Thread(target=Mouse, args=(lock,)))

for Cat in Cats:
  Cat.start()

for Mouse in Mice:
  Mouse.start()

for Cat in Cats:
    Cat.join()

for Mouse in Mice:
    Mouse.join()
    
print(Bowls)
