# Problem Set 2 - ECE391 Fall 2023

### Logistics
Problem Set 2 is due **Tuesday 9/19 at 11:59:59 PM** in the master branch. Only one
person per group should have a `partners.txt` with **all** the netids of your partners on separate lines;
An example `partners.txt` would look like this if `aamirh2` was submitting the group's solution.
> aamirh2
>
> hagrawa3
>
> scui8
>
> yiranli6
>
> ch5

Note that if your netid is part of more than one `partners.txt`, you will recieve a 0 for the problem set.


### Problem 1: Reading VGA Documentation (6 pt)
As preparation for MP2, you need to figure out how to do a couple of things
with the VGA.  There's some free documentation available
[here](www.osdever.net/FreeVGA/vga/vga.htm), but you may use any documentation
that you like to learn the necessary changes to registers and so forth.  Write
your solution in p1/solution.md (Note this is a markdown file so you can use
markdown formatting if you like)

1. You must use VGA support to add a non-scrolling status bar. Figure out how
to use VGA registers to separate the screen into two distinct regions. Explain
the necessary register value settings, how the VGA acts upon them, and any
relevant constraints that must be obeyed when setting up the status bar.
2. You must change the VGA's color palette. Figure out how to do so, and
explain the sequence of register operations necessary to set a given color to a
given 18-bit RGB (red, green, blue) value.

Write your solution in **`p1/solution.md`**.

### Problem 2: Tux Driver Code/Documentation (9 pt)
As part of MP2, you will also write a device driver for the Tux controller
boards in the lab. The documentation for this board can be found in the file
*mtcp.h* in the class directory under mp2.  You will need to read it for the
following questions.  Write your solution in p2/solution.md (Note this is a
markdown file so you can use markdown formatting if you like)

1. For each of the following messages sent from the computer to the Tux
controller, briefly explain when it should be sent, what effect it has on the
device, and what message or messages are returned to the computer as a result:
**MTCP\_BIOC\_ON**, **MTCP\_LED\_SET**.
2. For each of the following messages sent from the Tux controller to the
computer, briefly explain when the device sends the message and what
information is conveyed by the message: **MTCP\_ACK**, **MTCP\_BIOC\_EVENT**,
**MTCP\_RESET**.
3. Now read the function header for **tuxctl\_handle\_packet** in
**tuxctl-ioctl.c**---you will have to follow the pointer there to answer the
question, too. In some cases, you may want to send a message to the Tux
controller in response to a message just received by the computer (using
**tuxctl\_ldisc\_put**). However, if the output buffer for the device is full,
you cannot do so immediately. Nor can the code (executing in
**tuxctl\_handle\_packet**) wait (for example, go to sleep).  Explain in one
sentence why the code cannot wait.

Write your solution in **`p2/solution.md`**.

### Problem 3: Back to the Lab (10 pt)
In an alternate universe, the COVID-19 pandemic did not occur. Hence, ECE392 (the ECE391 of that universe) did not shut down its computer laboratory like ECE391 did. Instead they implemented the following strict set of rules for the lab that is shared by **students**, **TAs** and **professors**.

1. To not violate any fire hazard regulations there can only be a maximum of 20 people in the lab at a time. However, there is **NO** limit on the number of students, TAs or professors waiting in line to get into the room.

2. Students and TAs may occupy the lab at the same time, but neither students nor TAs are allowed in the lab at the same time as a professor. (If at least 1 Professor is in the lab then only professors can be in the lab)

3. At most one student, professor or TA may enter a lab when an **_enter** function is called. The **_enter** functions should only return once the caller has entered the lab.

4. If the lab is full, any person trying to enter must wait.

5. Professors have the highest priority followed by TAs followed by students (the lowest priority). 

6. In order for a professor to enter the lab, the professor must wait until there are no students or TAs left in the lab before entering. While the professor(s) are in the lab, students or TAs may not enter until there are no professors in the lab.

7. The **_exit** function removes one professor, TA or student from the lab.
   
8. Priority does not need to be enforced among groups of students, professors, or TAs. In other words, if student 1 arrives before student 2, student 1 does not necessarily need to enter the lab before student 2

You are to implement a thread safe synchronization library to enforce the lab occupancy policy described above. 

You may use only **ONE** spinlock in your design from **`spinlock_ece391.h`**, and no other synchronization primitives may be used. 

As these functions will be part of a thread safe library, they may be called simultaneously. 

Write the code for enter and exit of students, TAs and professors. A skeleton has been provided for you.

You may assume that any given thread has a single role—that of student, TA, or professor, and that any thread will make a sequence of alternating enter and exit calls corresponding to the thread’s role, but separated arbitrarily in time. Finally, the thread role’s enter function is always called first
by any thread, and exit is always called last. For example, a TA thread might call TA enter, then TA exit, and then repeat the same sequence three more times before terminating.

Write your solution in **`p3/solution.c`** and **`p3/solution.h`**. If you would like to test your code you should be to compile and run on the VM using **`make`** and **`./lab_occupancy`**. 

**Hint** You might want to define a few elements in your struct to keep track of who is in the lab and waiting in the line to get in. A flag that keeps track of how many professors there are in the line would help massively.

