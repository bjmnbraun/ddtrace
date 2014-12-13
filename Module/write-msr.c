/*  
 *  Read PMC in kernel mode.
 */
#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */

#define PERFEVTSELx_MSR_BASE   0x00000186
#define PMCx_MSR_BASE          0x000000c1      /* NB: write when evt disabled*/

#define PERFEVTSELx_USR        (1U << 16)      /* count in rings 1, 2, or 3 */
#define PERFEVTSELx_OS         (1U << 17)      /* count in ring 0 */
#define PERFEVTSELx_EN         (1U << 22)      /* enable counter */

#define DEFINE_SPINLOCK(x)      spinlock_t x = __SPIN_LOCK_UNLOCKED(x)

#define NUM_COUNTERS 3

static int clear = 0;
module_param(clear, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

static void
histar_write_msr(uint32_t msr, uint64_t val)
{
    uint32_t lo = val & 0xffffffff;
    uint32_t hi = val >> 32;
    __asm __volatile("wrmsr" : : "c" (msr), "a" (lo), "d" (hi));
}

static uint64_t
histar_read_msr(uint32_t msr)
{
    uint32_t hi, lo;
    __asm __volatile("rdmsr" : "=d" (hi), "=a" (lo) : "c" (msr));
    return ((uint64_t) lo) | (((uint64_t) hi) << 32);
}

static uint64_t old_value_perfsel[NUM_COUNTERS][8];

DEFINE_SPINLOCK(mr_lock);
static unsigned long flags;

static void wrapper(void* ptr) {
    int id;
    uint64_t value;
    int i;

    spin_lock_irqsave(&mr_lock, flags);

    id = smp_processor_id();

    // Save the old values before we do something stupid.
    for(i = 0; i < NUM_COUNTERS; i++){
        old_value_perfsel[i][id] = histar_read_msr(PERFEVTSELx_MSR_BASE + i);
    }
    
    // Clear out the existing counters
    for(i = 0; i < NUM_COUNTERS; i++){
        histar_write_msr(PERFEVTSELx_MSR_BASE + i, 0);
        histar_write_msr(PMCx_MSR_BASE + i, 0);
    }
    if (clear){
        spin_unlock_irqrestore(&mr_lock, flags);
        return;
    }

    // Reference: Page 590 of 325384.pdf
    // Table 19-1 in the most recent Intel Manual - Architectural 
    // Last Level Cache Miss — Event select 2EH, Umask 41H
    value = 0x2E | (0x41 << 8) |PERFEVTSELx_EN | PERFEVTSELx_USR;
    histar_write_msr(PERFEVTSELx_MSR_BASE, value);
    
    // Reference: Page 590 of 325384.pdf
    // Table 19-1 in the most recent Intel Manual - Architectural 
    // Last Level Cache Reference — Event select 2EH, Umask 4FH
    value = 0x2E | (0x4F << 8) |PERFEVTSELx_EN | PERFEVTSELx_USR;
    histar_write_msr(PERFEVTSELx_MSR_BASE + 1, value);
    
    // Reference: Page 708 of 325384.pdf
    // Table 19-1 in the most recent Intel Manual - Architectural 
    // Unhalted core cycles — Event select 3CH, Umask 00H
    // Can OR on _OS if you want OS cycles too
    value = 0x3C | (0x00 << 8) |PERFEVTSELx_EN | PERFEVTSELx_USR;
    histar_write_msr(PERFEVTSELx_MSR_BASE + 2, value);

    spin_unlock_irqrestore(&mr_lock, flags);
}

static void restore_wrapper(void* ptr) {
    int id = smp_processor_id();
    int i;
    if (clear) return;
    for(i = 0; i < NUM_COUNTERS; i++){
        histar_write_msr(PERFEVTSELx_MSR_BASE + i, old_value_perfsel[i][id]);
    }
}

int init_module(void)
{
    printk(KERN_INFO "Entering write-msr!\n");
    on_each_cpu(wrapper, NULL, 0);
    /* 
     * A non 0 return means init_module failed; module can't be loaded. 
     */
    return 0;
}

void cleanup_module(void)
{
    on_each_cpu(restore_wrapper, NULL, 0);
    printk(KERN_INFO "Exiting write-msr!\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stanford RAMCloud");
MODULE_DESCRIPTION("Simple Intel Performance Counter Module\n");
MODULE_VERSION("proc");
