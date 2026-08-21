#include "paging.h"
#include "klib.h"

static paging_page_entry_t s_pages[PAGING_PROCESS_COUNT][PAGING_VIRTUAL_PAGES];
static paging_frame_t s_frames[PAGING_FRAME_COUNT];
static paging_stats_t s_stats;
static uint32_t s_clock;

void paging_init(void)
{
    kmemset(s_pages, 0, sizeof(s_pages));
    kmemset(s_frames, 0, sizeof(s_frames));
    kmemset(&s_stats, 0, sizeof(s_stats));
    s_clock = 0;
}

static uint8_t choose_frame(void)
{
    for (uint8_t frame = 0; frame < PAGING_FRAME_COUNT; ++frame) {
        if (!s_frames[frame].occupied) return frame;
    }
    uint8_t victim = 0;
    uint32_t oldest = 0xFFFFFFFFu;
    for (uint8_t frame = 0; frame < PAGING_FRAME_COUNT; ++frame) {
        if (s_frames[frame].loaded_at < oldest) {
            oldest = s_frames[frame].loaded_at;
            victim = frame;
        }
    }
    return victim;
}

bool paging_translate(uint8_t process, uint16_t virtual_address, bool write, paging_result_t *result)
{
    const uint16_t virtual_limit = PAGING_VIRTUAL_PAGES * PAGING_PAGE_SIZE;
    if (process >= PAGING_PROCESS_COUNT || virtual_address >= virtual_limit || result == NULL) return false;

    kmemset(result, 0, sizeof(*result));
    result->process = process;
    result->page = (uint8_t)(virtual_address / PAGING_PAGE_SIZE);
    result->offset = (uint16_t)(virtual_address % PAGING_PAGE_SIZE);
    result->write = write;
    ++s_clock;
    ++s_stats.accesses;

    paging_page_entry_t *entry = &s_pages[process][result->page];
    if (entry->present) {
        ++s_stats.hits;
        result->frame = entry->frame;
    } else {
        ++s_stats.faults;
        result->page_fault = true;
        result->frame = choose_frame();
        paging_frame_t *frame = &s_frames[result->frame];
        if (frame->occupied) {
            result->evicted = true;
            result->victim_process = frame->process;
            result->victim_page = frame->page;
            result->victim_dirty = frame->dirty;
            ++s_stats.evictions;
            if (frame->dirty) ++s_stats.writebacks;
            s_pages[frame->process][frame->page] = (paging_page_entry_t){0};
        }
        *frame = (paging_frame_t){
            .occupied = true,
            .dirty = false,
            .process = process,
            .page = result->page,
            .loaded_at = s_clock,
            .last_used = s_clock,
        };
        *entry = (paging_page_entry_t){
            .present = true,
            .dirty = false,
            .frame = result->frame,
        };
    }

    paging_frame_t *frame = &s_frames[result->frame];
    frame->last_used = s_clock;
    if (write) {
        frame->dirty = true;
        entry->dirty = true;
    }
    result->physical_address = (uint16_t)(result->frame * PAGING_PAGE_SIZE + result->offset);
    return true;
}

paging_stats_t paging_get_stats(void) { return s_stats; }

paging_page_entry_t paging_get_page(uint8_t process, uint8_t page)
{
    if (process >= PAGING_PROCESS_COUNT || page >= PAGING_VIRTUAL_PAGES) return (paging_page_entry_t){0};
    return s_pages[process][page];
}

paging_frame_t paging_get_frame(uint8_t frame)
{
    if (frame >= PAGING_FRAME_COUNT) return (paging_frame_t){0};
    return s_frames[frame];
}

bool paging_self_test(void)
{
    paging_result_t result;
    paging_init();
    if (!paging_translate(0, 42, false, &result) || !result.page_fault || result.frame != 0) return false;
    if (!paging_translate(0, 42, true, &result) || result.page_fault || !paging_get_frame(0).dirty) return false;
    for (uint8_t page = 1; page < PAGING_FRAME_COUNT; ++page) {
        if (!paging_translate(0, (uint16_t)(page * PAGING_PAGE_SIZE), false, &result)) return false;
    }
    if (!paging_translate(1, 0, false, &result) || !result.evicted ||
        result.victim_process != 0 || result.victim_page != 0 || !result.victim_dirty) return false;
    const paging_stats_t stats = paging_get_stats();
    const bool passed = stats.accesses == 8 && stats.hits == 1 && stats.faults == 7 &&
                        stats.evictions == 1 && stats.writebacks == 1;
    paging_init();
    return passed;
}
