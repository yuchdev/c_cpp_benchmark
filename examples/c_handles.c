// IRQ number, DMA channel, MMIO
// base address - all plain int
// Nothing stops mixing them up
int irq_num;
int dma_channel;
unsigned long mmio_base;

// Typical driver init call:
int setup_device(int irq,
                 int dma,
                 unsigned long base,
                 int priority);

// Called correctly? Hard to tell
setup_device(irq_num,
             dma_channel,
             mmio_base,
             priority);

// Silently compiles. Silently wrong
setup_device(dma_channel,  // swapped!
             irq_num,      // swapped!
             mmio_base,
             priority);

// Accidentally pass IRQ as address
request_irq(mmio_base,  // wrong!
            handler, 0,
            "mydev", dev);

// All three calls compile cleanly
// Bug surfaces at runtime only!
