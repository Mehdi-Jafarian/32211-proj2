# Mega Prompt for GitHub Copilot

You are helping me complete an **Operating Systems course project in C**. I already have the **starter kit files in my current Git repo**. Your job is to generate a correct solution that **fully respects the project specification, test script, and grading constraints**.

Do **not** invent requirements. Do **not** simplify the assignment. Do **not** hardcode behavior that only works for the provided filenames unless the spec explicitly requires fixed output filenames.

Follow the specification exactly. Do not skip any step or simplify any behavior.

I want you to act like a careful systems programming assistant and produce code that is correct, explainable, and robust enough to pass hidden grading variations.

---

## 1. What the project is

This is a **virtual memory / MMU simulation**.

The program must translate **logical addresses** into **physical addresses** using:
- a **TLB**
- a **page table**
- a **backing store**
- **physical memory**

The logical address space size is:
- `2^16 = 65,536 bytes`

Each logical address read from the address file is a **32-bit integer**, but the program must only use the **lower 16 bits**.

Always mask the logical address to 16 bits before extracting page number and offset.

Those 16 bits are split into:
- **page number** = upper 8 bits of the low 16 bits
- **offset** = lower 8 bits

So:
- bits `31–16` are ignored
- bits `15–8` = page number
- bits `7–0` = offset

Core constants:
- page table entries: `256`
- page size: `256 bytes`
- frame size: `256 bytes`
- TLB entries: `16`
- virtual pages: `256`

The offset is **never modified during translation**.
Only the page number is translated into a frame number.

The exact physical address formula is:

```c
physical_address = frame_number * 256 + offset;
```

---

## 2. Program invocation and required outputs

The executable name must be:
- `mmu`

The program is run as:

```bash
./mmu <memory_size> <backing_store_file> <addresses_file>
```

Examples:

### Phase 1 run
```bash
./mmu 256 BACKING_STORE.bin addresses.txt
```
Must generate:
- `output256.csv`

### Phase 2 run
```bash
./mmu 128 BACKING_STORE.bin addresses.txt
```
Must generate:
- `output128.csv`

Important:
- **Do not hardcode input file names** like `BACKING_STORE.bin` or `addresses.txt`
- Use the command-line arguments passed to the program
- TAs may modify `test.sh` and pass **different filenames**
- However, the **output file names are fixed by mode**:
  - memory size `256` → `output256.csv`
  - memory size `128` → `output128.csv`

---

## 3. Phase 1 requirements (memory size = 256)

Phase 1 has:
- `256` physical frames
- frame size `256`
- total physical memory `65,536 bytes`

Because physical memory can hold all pages, **no page replacement is required in Phase 1**.

### Phase 1 translation flow
For each logical address:
1. Read the integer
2. Mask to the lower 16 bits
3. Extract:
   - `page_number = (logical_address >> 8) & 0xFF`
   - `offset = logical_address & 0xFF`
4. Check the TLB first
5. If TLB hit:
   - get frame number directly
6. If TLB miss:
   - check page table
7. If page table hit:
   - get frame number
   - **insert `(page_number, frame_number)` into the TLB using FIFO**
8. If page fault:
   - load the page from backing store into the next free frame
   - update page table
   - **insert `(page_number, frame_number)` into the TLB using FIFO**
9. Compute physical address with:
   - `frame_number * 256 + offset`
10. Read the **signed byte value** from physical memory at that physical address
11. Write one CSV row

### Important TLB clarification
After **every TLB miss**, once a frame number is obtained, the TLB must be updated using **FIFO replacement**, regardless of whether the frame came from:
- a page table hit, or
- a page fault

So every successful page table lookup or page fault resolution must result in TLB insertion.

### Backing store access
On page fault for page `N`:
- seek to byte offset `N * 256` in the backing store file
- read exactly `256` bytes
- store that page into a frame in physical memory

Use standard C I/O like:
- `fopen`
- `fseek`
- `fread`
- `fclose`

---

## 4. Phase 2 requirements (memory size = 128)

Phase 2 changes only the physical memory capacity:
- number of physical frames = `128`

This means:
- not all pages can be resident at once
- page replacement is required when memory is full
- at most `128` page table entries are valid at one time

### Page replacement policy
When a page fault occurs:
- if a free frame exists, use it
- otherwise, use **Least Recently Used (LRU)** page replacement

LRU applies to **physical memory frame replacement**, not TLB replacement.

You must maintain a mapping from frame → page number, so that when a frame is selected for replacement, you can identify which page is currently stored in that frame.

### TLB policy in Phase 2
The TLB still has:
- `16` entries
- **FIFO replacement policy**

TLB policy does **not** change between phases.

### Phase 2 page fault with replacement
If no free frame exists:
1. Select the **least recently used frame**
2. Identify which page is currently stored in that frame
3. Update the page table:
   - mark the evicted page as invalid
4. Remove or invalidate any TLB entry that refers to the evicted page
5. Load the new page from the backing store into the freed frame
6. Update the page table:
   - map the new page to that frame
7. Insert `(page_number, frame_number)` into the TLB using FIFO
8. Update the LRU tracking for that frame

### Usage tracking requirement
You must correctly track how recently each frame was used so that LRU replacement behaves correctly.
A common approach is to maintain a global counter that increments on each address access and store a "last used time" for each frame. The frame with the smallest timestamp is the least recently used.
A frame counts as “used” whenever it is the resolved frame for the current address reference, including:
- TLB hit
- page table hit
- page fault load

---

## 5. Required output format

### Data rows
There is **no header row**.

Each translated address must produce a CSV row in exactly this format:

```text
logical_address,physical_address,value
```

where:
- `logical_address` is the original logical address value read from file
- `physical_address` is the translated physical address
- `value` is the **signed byte** stored at that physical address

Values must be treated as signed bytes (for example, use `signed char` in C).

### Statistics rows
At the end of the CSV file, append these exact statistics rows.
The labels must match exactly.
The percentage must be printed with **2 decimal places** and a `%` sign.
Each line must end with a trailing comma.

Exact required labels:

```text
Page Faults Rate,XX.XX%,
TLB Hits Rate,YY.YY%,
```

This exact label spelling matters:
- `Page Faults Rate`
- `TLB Hits Rate`

Examples from the provided correct files use this style:

```text
Page Faults Rate,24.40%,
TLB Hits Rate,5.40%,
```

So do **not** output alternative labels like:
- `Page Fault Rate`
- `Page Faults`
- `TLB Hit Rate`
- `TLB Hits`

Use the exact required strings.

### Floating point formatting
Statistics must be printed with **exactly 2 decimal places**.

For example:

```c
fprintf(out, "Page Faults Rate,%.2f%%,\n", page_fault_rate);
fprintf(out, "TLB Hits Rate,%.2f%%,\n", tlb_hit_rate);
```

---

## 6. Testing and grading constraints

The project is graded using the provided `test.sh` script.

The script will:
- compile the program using the provided Makefile
- run the program in both modes
- compare generated output files against the provided correct CSV files

### Commands used by testing

Phase 1:
```bash
./mmu 256 BACKING_STORE.bin addresses.txt
```
Expected generated file:
- `output256.csv`

Phase 2:
```bash
./mmu 128 BACKING_STORE.bin addresses.txt
```
Expected generated file:
- `output128.csv`

The provided script uses `diff -wB`, but you should still try to match the output exactly.

### Important hidden-test constraint
TAs may modify `test.sh` to use different filenames for:
- the address input file
- the correct output reference files

Therefore:
- **never hardcode input file names**
- rely on command-line arguments

### What must be tested before submission
You must ensure:
- the program compiles with the Makefile
- the program runs correctly in both modes
- `test.sh` passes
- outputs match the provided correct files

---

## 7. Deliverables and packaging constraints

The submission zip must be named:
- `mehdi_mmu.zip`

The zip must contain **one main folder**.
All required files must be directly inside that folder.
Do **not** create extra nested folders like a `StarterKit/` folder.

Required files in the submission:
1. `Makefile` (editable)
2. all `.c` and `.h` files you write
3. `BACKING_STORE.bin` (unchanged)
4. `addresses.txt` (unchanged)
5. `test.sh` (unchanged)
6. `correct128.csv` (unchanged)
7. `correct256.csv` (unchanged)

Do **not** include:
- object files like `*.o`
- compiled executables

---

## 8. What not to do

Do **not** do any of the following:

1. **Do not hardcode input file names**
   - backing store path must come from argv
   - address file path must come from argv

2. **Do not hardcode translation results**
   - no precomputed outputs
   - no logic that only works for the provided `addresses.txt`

3. **Do not ignore the TLB update rule**
   - after every TLB miss, if a frame number is obtained, insert into TLB using FIFO

4. **Do not use LRU for the TLB**
   - TLB replacement is FIFO only

5. **Do not use FIFO for physical memory replacement in Phase 2**
   - physical memory replacement must be LRU

6. **Do not modify the offset**
   - it must remain unchanged from logical to physical address

7. **Do not compute the physical address incorrectly**
   - use exactly:
     `frame_number * 256 + offset`

8. **Do not forget to invalidate TLB entries for evicted pages in Phase 2**

9. **Do not output a CSV header row**

10. **Do not change the required statistics labels**

11. **Do not print floating-point stats with the wrong precision**
   - must be 2 decimal places

12. **Do not produce code that only works in one mode**
   - the same program must correctly handle memory size `256` and `128`

13. **Do not assume the Makefile must remain unchanged**
   - it may be edited if needed
   - but the build must still succeed through Makefile

14. **Do not create unnecessary complexity**
   - prefer a simple, correct, explainable implementation

---

## 9. Recommended implementation expectations

I want a clean, robust C implementation that is easy to explain.
Prefer correctness and clarity over cleverness.

A good solution will likely include:
- a page table structure with valid bits and frame numbers
- a TLB structure storing page-to-frame entries plus validity
- FIFO bookkeeping for TLB replacement
- physical memory as a byte array or 2D array by frame/page size
- a way to know which page is currently stored in each frame
- LRU bookkeeping per frame, using a monotonically increasing access counter / timestamp or equivalent
- careful page fault counting
- careful TLB hit counting
- handling of signed byte values correctly

Possible implementation approach:
- parse `memory_size` from argv
- allocate / initialize physical memory and metadata based on frame count
- initialize page table entries as invalid
- initialize TLB entries as invalid
- for each logical address:
  - parse address
  - derive page and offset
  - try TLB
  - if miss, try page table
  - if page fault, load from backing store and possibly evict via LRU
  - update TLB on every miss once frame is known
  - update LRU usage for the resolved frame
  - compute physical address
  - get signed byte value
  - write CSV row
- after processing all addresses:
  - compute page fault rate = page_faults / total_addresses * 100
  - compute TLB hit rate = tlb_hits / total_addresses * 100
  - append the two required statistics rows

---

## 10. How I want you to help me right now

Please do the following in order:

1. **Inspect the current repo files first**
   - check the existing Makefile
   - check `test.sh`
   - check whether `mmu.c` exists already
   - do not assume starter contents blindly

2. **Summarize the implementation plan briefly**
   - list the core data structures you will use
   - explain how TLB FIFO will work
   - explain how LRU frame replacement will work

3. **Then generate the actual code**
   - preferably in `mmu.c`
   - or split into multiple source/header files only if truly useful
   - if multiple files are used, update the Makefile accordingly

4. **Make sure the code compiles cleanly with `-Wall`**

5. **Self-audit the result before stopping**
   - verify Phase 1 logic separately
   - verify Phase 2 logic separately
   - verify TLB update rules
   - verify eviction invalidates page table and TLB entries correctly
   - verify stats formatting and labels
   - verify output filenames
   - verify there is no input filename hardcoding

6. **If you make assumptions, state them clearly**
   - but do not invent assumptions that contradict the requirements above

---

## 11. Final instruction

Produce a solution that is likely to pass the provided `test.sh` and hidden filename variations used by TAs.

Be strict about:
- exact output behavior
- TLB FIFO
- Phase 2 LRU replacement
- signed byte values
- 2-decimal percentage formatting
- no hardcoded input filenames

Do not stop at a vague outline. I want the real implementation.
