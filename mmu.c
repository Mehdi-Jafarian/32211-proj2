#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE   256
#define TLB_SIZE    16
#define NUM_PAGES   256

typedef struct {
    int page_number;
    int frame_number;
    int valid;
} TLBEntry;

typedef struct {
    int frame_number;
    int valid;
} PageTableEntry;

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <memory_size> <backing_store> <addresses_file>\n", argv[0]);
        return 1;
    }

    int num_frames   = atoi(argv[1]);
    const char *backing_store_path = argv[2];
    const char *addresses_path     = argv[3];

    /* ---------- allocate physical memory ---------- */
    signed char *physical_memory = calloc(num_frames * PAGE_SIZE, 1);
    if (!physical_memory) { perror("calloc"); return 1; }

    /* ---------- page table ---------- */
    PageTableEntry page_table[NUM_PAGES];
    for (int i = 0; i < NUM_PAGES; i++) {
        page_table[i].valid        = 0;
        page_table[i].frame_number = -1;
    }

    /* ---------- TLB (FIFO) ---------- */
    TLBEntry tlb[TLB_SIZE];
    for (int i = 0; i < TLB_SIZE; i++) tlb[i].valid = 0;
    int tlb_next = 0;   /* next FIFO slot to overwrite */

    /* ---------- per-frame metadata ---------- */
    int       *frame_page      = malloc(num_frames * sizeof(int));
    long long *frame_last_used = malloc(num_frames * sizeof(long long));
    if (!frame_page || !frame_last_used) { perror("malloc"); return 1; }
    for (int i = 0; i < num_frames; i++) {
        frame_page[i]      = -1;
        frame_last_used[i] = -1;
    }

    int next_free_frame = 0;
    long long access_counter = 0;

    int page_faults = 0;
    int tlb_hits    = 0;
    int total       = 0;

    /* ---------- open files ---------- */
    FILE *backing_store = fopen(backing_store_path, "rb");
    if (!backing_store) { perror(backing_store_path); return 1; }

    FILE *addr_file = fopen(addresses_path, "r");
    if (!addr_file)  { perror(addresses_path); return 1; }

    char output_filename[64];
    snprintf(output_filename, sizeof(output_filename), "output%d.csv", num_frames);
    FILE *out = fopen(output_filename, "w");
    if (!out) { perror(output_filename); return 1; }

    /* ---------- main translation loop ---------- */
    int logical_address;
    while (fscanf(addr_file, "%d", &logical_address) == 1) {
        total++;
        access_counter++;

        /* mask to lower 16 bits, extract page and offset */
        int addr16      = logical_address & 0xFFFF;
        int page_number = (addr16 >> 8) & 0xFF;
        int offset      =  addr16       & 0xFF;

        int frame_number = -1;

        /* 1. check TLB */
        for (int i = 0; i < TLB_SIZE; i++) {
            if (tlb[i].valid && tlb[i].page_number == page_number) {
                frame_number = tlb[i].frame_number;
                tlb_hits++;
                break;
            }
        }

        /* 2. TLB miss */
        if (frame_number == -1) {

            if (page_table[page_number].valid) {
                /* page table hit */
                frame_number = page_table[page_number].frame_number;
            } else {
                /* page fault */
                page_faults++;

                if (next_free_frame < num_frames) {
                    /* free frame available */
                    frame_number = next_free_frame++;
                } else {
                    /* LRU eviction */
                    int       lru_frame = 0;
                    long long min_time  = frame_last_used[0];
                    for (int i = 1; i < num_frames; i++) {
                        if (frame_last_used[i] < min_time) {
                            min_time  = frame_last_used[i];
                            lru_frame = i;
                        }
                    }
                    frame_number = lru_frame;

                    /* invalidate evicted page in page table */
                    int evicted_page = frame_page[frame_number];
                    if (evicted_page != -1) {
                        page_table[evicted_page].valid = 0;
                    }

                    /* invalidate matching TLB entries */
                    for (int i = 0; i < TLB_SIZE; i++) {
                        if (tlb[i].valid && tlb[i].page_number == evicted_page) {
                            tlb[i].valid = 0;
                        }
                    }
                }

                /* load page from backing store */
                fseek(backing_store, (long)page_number * PAGE_SIZE, SEEK_SET);
                fread(physical_memory + (long)frame_number * PAGE_SIZE,
                      1, PAGE_SIZE, backing_store);

                /* update page table and frame map */
                page_table[page_number].frame_number = frame_number;
                page_table[page_number].valid        = 1;
                frame_page[frame_number]             = page_number;
            }

            /* update TLB with FIFO */
            tlb[tlb_next].page_number  = page_number;
            tlb[tlb_next].frame_number = frame_number;
            tlb[tlb_next].valid        = 1;
            tlb_next = (tlb_next + 1) % TLB_SIZE;
        }

        /* update LRU timestamp for the resolved frame */
        frame_last_used[frame_number] = access_counter;
        /* keep frame_page consistent on TLB-hit path too */
        frame_page[frame_number] = page_number;

        /* compute physical address and byte value */
        int physical_address = frame_number * PAGE_SIZE + offset;
        signed char value    = physical_memory[physical_address];

        fprintf(out, "%d,%d,%d\n", logical_address, physical_address, (int)value);
    }

    /* ---------- statistics ---------- */
    double page_fault_rate = (double)page_faults / total * 100.0;
    double tlb_hit_rate    = (double)tlb_hits    / total * 100.0;
    fprintf(out, "Page Faults Rate,%.2f%%,\n", page_fault_rate);
    fprintf(out, "TLB Hits Rate,%.2f%%,\n",    tlb_hit_rate);

    /* ---------- clean up ---------- */
    fclose(out);
    fclose(addr_file);
    fclose(backing_store);
    free(physical_memory);
    free(frame_page);
    free(frame_last_used);

    return 0;
}
