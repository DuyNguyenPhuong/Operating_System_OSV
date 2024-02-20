# Final Project Proposal

**Project Type:** Individual

**Project Idea:**
The project aims to measure the Translation-Lookaside Buffer (TLB) and Cache sizes. Two separate graphs will be generated to analyze the behavior of accessing data as the number of pages and cache size increase.

My ambitious goal is to vary values to test it on another computer besides mine.

**Functionality to Implement:**

For TLB:
1. Set PAGESIZE to 4 KiB (determined for MacBook).
2. Develop a program to access elements in an array with a customizable range of NUMPAGES from 1 to 1024.

Sample Code

```{c}
int jump = PAGESIZE / sizeof(int);
for (i = 0; i < NUMPAGES * jump; i += jump)
a[i] += 1;
```

3. Implement a timer using gettimeofday() to measure access times for each test.
4. Graph the results using Excel.

For Cache:
1. Develop a C program to iteratively access elements of a small array, measuring access times for each iteration (similar to TLB)

2. Implement functionality to vary the size of the array to observe changes in access times

3. Implement a timer using gettimeofday() to measure access times for each test.

4. Visualize results using Excel and graphing tools to illustrate the relationship between array size and access time.

5. Comparing the behavior of TLB and cache

**Tests to be Conducted:**

For TLB:
- `TLB Access Test`: Measure access time for an array with increasing page numbers.
  
For Cache:
- `Cache Access Test`: Measure access time for varying array sizes to observe cache behavior.

**References:**
**References:**

1. Arpaci-Dusseau, R. H., & Arpaci-Dusseau, A. C. **Three Easy Pieces: Operating Systems Concepts**. Chapter 19: Paging: Faster Translations (TLBs).

2. Bryant, R. E., & O'Hallaron, D. R. (Year of publication). **Computer Systems: A Programmer's Perspective**. [Online]. Available: http://csapp.cs.cmu.edu/2e/perspective.html

**Information Needed:**
- Is there a more efficient way to draw a graph besides copying value to Excel

- Edges cases of Cache, TLB perfomance such as at the beginning, to infer the behaviors more correctly

**Foreseen Issues:**
- **Creating efficient test**: How we can create test so that we know the accessing time is resonable. On test, I can think of is too making sure the array is accessing the correct element

- **Compiler optimization**: Compilers do all sorts of clever things, including removing loops which increment values that no other part of the program subsequently uses. How can we ensure the compiler does not remove the main loop
above from your TLB size estimator?

- **Multi Core**: Most systems today ship with multiple CPUs, and each CPU, of course, has its own TLB hierarchy. To really get good measurements, you have to run your code on just one CPU, instead of letting the scheduler bounce it
from one CPU to the next.

- **Initialization**: If we don’t initialize the array a above before accessing it, the first time you access it will be very expensive, due to initial access costs such as demand zeroing. Will this affect our code and its timing? What
can we do to counterbalance these potential costs?