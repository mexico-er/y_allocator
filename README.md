# yallocator

a `malloc` alternative.

## Questions
  - Is `yalloc` faster than `malloc`?
    - It is, but only when allocating larger amounts of data at once, with less allocations in total as seen in the benchmark program here: https://codeishot.com/4gbbTuJw.
  - Why is it named "yallocator"?
    - It has the word "y'all" in it.
  - Why are the files named `y_allocator` instead of yallocator?
    - Because it sounded like a better name for a file for some reason and I just kind of named it like that.
