#ifndef HW_HALUCINATOR_IRQ_MEMORY_H
#define HW_HALUCINATOR_IRQ_MEMORY_H

typedef struct HalucinatorIRQState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    uint64_t address;
    uint32_t size;
    uint64_t request_id;
    unsigned char * memory;
    qemu_irq irq;
} HalucinatorIRQState;

#endif
