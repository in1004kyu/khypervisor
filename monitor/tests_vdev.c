
#include "tests_vdev.h"
#include "vdev/vdev_sample.h"
#include "vdev/vdev_gicd.h"
#include "print.h"
#include <gic_regs.h>
#include <cfg_platform.h>
#include <asm-arm_inline.h>
#include <gic.h>

/* return the bit position of the first bit set from msb
 * for example, firstbit32(0x7F = 111 1111) returns 7
 */
#define firstbit32(word) ( 31 - asm_clz(word) )
#define NUM_MAX_VIRQS   128
#define NUM_STATUS_WORDS    (NUM_MAX_VIRQS / 32)

static uint32_t ostatus[NUM_STATUS_WORDS] = {0, };        // old status

void callback_inject(uint32_t irq, void *regs, void *pdata)
{
    vmid_t vmid;
    uint32_t virq, pirq;
    pirq = irq;
    virq = priq2virq(vmid, pirq);
    vmid = context_current_vmid();
    virq_inject(vmid, virq, pirq, 1); 
}

void _my_vgicd_changed_istatus( vmid_t vmid, uint32_t istatus, uint8_t word_offset )
{
    uint32_t cstatus;                          // changed bits only
    uint32_t minirq;
    int bit;

    minirq = word_offset * 32;                 /* irq range: 0~31 + word_offset * size_of_istatus_in_bits */
    cstatus = ostatus[word_offset] ^ istatus;   // find changed bits

    while(cstatus) {
        bit = firstbit32(cstatus);

        /* changed bit */
        if ( istatus & (1 << bit) ) {
            // enabled irq: bit + minirq;
            printh("[%s : %d] enabled irq num is %d\n", __FUNCTION__, __LINE__, bit + minirq);
            pirq = virq2pirq(vmid, bit + minirq);
            gic_test_set_irq_handler(pirq, &callback_inject, 0);
            gic_test_configure_irq(pirq, GIC_INT_POLARITY_LEVEL, gic_cpumask_current(), GIC_INT_PRIORITY_DEFAULT );
            
        } else {
            printh("[%s : %d] disabled irq num is %d\n",__FUNCTION__, __LINE__, bit + minirq);
            // disabled irq: bit + minirq;
             gic_disable_irq(virq2pirq(vmid, bit + minirq));
        }

        cstatus &= ~(1<< bit);
    }

    ostatus[word_offset] = istatus;
}


hvmm_status_t hvmm_tests_vdev(void)
{
    hvmm_status_t result = HVMM_STATUS_UNKNOWN_ERROR;

    printh( "tests: Registering sample vdev:'sample' at 0x3FFFF000\n");
    result = vdev_sample_init(0x3FFFF000);

    return result;
}

hvmm_status_t hvmm_tests_vdev_gicd(void)
{
    hvmm_status_t result = HVMM_STATUS_SUCCESS;

    printh( "[%s : %d] register vgicd_changed_istatus callback.\n", __FUNCTION__, __LINE__);
    vgicd_set_callback_changed_istatus(&_my_vgicd_changed_istatus);

    return result;
}

