// Define each resource kind as a distinct type
// Zero runtime cost
struct IrqNumber { int val; };
struct DmaChannel { int val; };
struct MmioBase { unsigned long val; };

// Signature now self-documented
int setup_device(IrqNumber  irq,
                 DmaChannel dma,
                 MmioBase   base,
                 int        priority);

IrqNumber  irq { 43 };
DmaChannel dma { 2  };
MmioBase   base{ 0xFED00000UL };

// Correct call - compiles fine
setup_device(irq, dma, base, 1);

// Transposition - compile error: 
// cannot convert DmaChannel to IrqNumber
setup_device(dma, irq, base, 1);

// Wrong type to request_irq - compile error
request_irq(base.val, handler, 0, "mydev", dev);

