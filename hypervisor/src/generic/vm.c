#include <k-hypervisor-config.h>
#include <gic_regs.h>
#include <vm.h>
#include <memory.h>
#include <interrupt.h>
#include <print.h>

/* Maximum is 2 now. Need to be dynamic setting */
#define MAX_NUM_VMID 2
#define INIT_VMID 0
#define MEMORY_DESCRIPT 2
#define MEMORY_START 0
//static vm vms[2];
static vmid_t current_vmid = VMID_INVALID;

#define IRQ_ENABLED 1
#define IRQ_DISABLED 0
#define END_OF_MD -1

/*
 *  vimm-0, pirq-69, virq-69 = pwm timer driver
 *  vimm-0, pirq-32, virq-32 = WDT: shared driver
 *  vimm-0, pirq-34, virq-34 = SP804: shared driver
 *  vimm-0, pirq-35, virq-35 = SP804: shared driver
 *  vimm-0, pirq-36, virq-36 = RTC: shared driver
 *  vimm-0, pirq-38, virq-37 = UART: dedicated driver IRQ 37 for guest 0
 *  vimm-1, pirq-39, virq-37 = UART: dedicated driver IRQ 37 for guest 1
 *  vimm-2, pirq,40, virq-37 = UART: dedicated driver IRQ 37 for guest 2
 *  vimm-3, pirq,48, virq-37 = UART: dedicated driver IRQ 38 for guest 3 -ch
 *  vimm-0, pirq-43, virq-43 = ACCI: shared driver
 *  vimm-0, pirq-44, virq-44 = KMI: shared driver
 *  vimm-0, pirq-45, virq-45 = KMI: shared driver
 *  vimm-0, pirq-47, virq-47 = SMSC 91C111, Ethernet - etc0
 *  vimm-0, pirq-41, virq-41 = MCI - pl180
 *  vimm-0, pirq-42, virq-42 = MCI - pl180
 */

static struct virqmap_entry guest0_virqmapd[] = {
    { IRQ_ENABLED, 1, 1 },
    { IRQ_ENABLED, 16, 16 },
    { IRQ_ENABLED, 17, 17 },
    { IRQ_ENABLED, 18, 18 },
    { IRQ_ENABLED, 19, 19 },
    { IRQ_ENABLED, 31, 31 },
    { IRQ_ENABLED, 32, 32 },
    { IRQ_ENABLED, 33, 33 },
    { IRQ_ENABLED, 34, 34 },
    { IRQ_ENABLED, 35, 35 },
    { IRQ_ENABLED, 36, 36 },
    { IRQ_ENABLED, 37, 38 },
    { IRQ_ENABLED, 38, 37 },
    { IRQ_ENABLED, 41, 41 },
    { IRQ_ENABLED, 42, 42 },
    { IRQ_ENABLED, 43, 43 },
    { IRQ_ENABLED, 44, 44 },
    { IRQ_ENABLED, 45, 45 },
    { IRQ_ENABLED, 46, 46 },
    { IRQ_ENABLED, 47, 47 },
    { IRQ_ENABLED, 69, 69 },
    { END_OF_MD, 0, 0}
};

static struct virqmap_entry guest1_virqmapd[] = {
    { IRQ_ENABLED, 39, 37 },
    { END_OF_MD, 0, 0}
};

static struct virqmap_entry guest2_virqmapd[] = {
    { IRQ_ENABLED, 40, 37 },
    { END_OF_MD, 0, 0}
};

static struct virqmap_entry guest3_virqmapd[] = {
    { IRQ_ENABLED, 48, 37 },
    { END_OF_MD, 0, 0}
};

static struct virqmap_entry *guest_virqmapds[] = {
    guest0_virqmapd,
    guest1_virqmapd,
    guest2_virqmapd,
    guest3_virqmapd
};

/**
 * \defgroup Guest_memory_map_descriptor
 *
 * Descriptor setting order
 * - label
 * - Intermediate Physical Address (IPA)
 * - Physical Address (PA)
 * - Size of memory region
 * - Memory Attribute
 * @{
 */
static struct memmap_desc guest_md_empty[] = {
    {       0, 0, 0, 0,  0},
};
/*  label, ipa, pa, size, attr */
static struct memmap_desc guest0_device_md[] = {
    { "sysreg", 0x1C010000, 0x1C010000, SZ_4K, MEMATTR_DM },
    { "sysctl", 0x1C020000, 0x1C020000, SZ_4K, MEMATTR_DM },
    { "aaci", 0x1C040000, 0x1C040000, SZ_4K, MEMATTR_DM },
    { "mmci", 0x1C050000, 0x1C050000, SZ_4K, MEMATTR_DM },
    { "kmi", 0x1C060000, 0x1C060000,  SZ_64K, MEMATTR_DM },
    { "kmi2", 0x1C070000, 0x1C070000, SZ_64K, MEMATTR_DM },
    { "v2m_serial0", 0x1C090000, 0x1C0A0000, SZ_4K, MEMATTR_DM },
    { "v2m_serial1", 0x1C0A0000, 0x1C090000, SZ_4K, MEMATTR_DM },
    { "v2m_serial2", 0x1C0B0000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
    { "v2m_serial3", 0x1C0C0000, 0x1C0C0000, SZ_4K, MEMATTR_DM },
    { "wdt", 0x1C0F0000, 0x1C0F0000, SZ_4K, MEMATTR_DM },
    { "v2m_timer01(sp804)", 0x1C110000, 0x1C110000, SZ_4K,
            MEMATTR_DM },
    { "v2m_timer23", 0x1C120000, 0x1C120000, SZ_4K, MEMATTR_DM },
    { "rtc", 0x1C170000, 0x1C170000, SZ_4K, MEMATTR_DM },
    { "clcd", 0x1C1F0000, 0x1C1F0000, SZ_4K, MEMATTR_DM },
    { "gicc", CFG_GIC_BASE_PA | GIC_OFFSET_GICC,
            CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K,
            MEMATTR_DM },
    { "SMSC91c111i", 0x1A000000, 0x1A000000, SZ_16M, MEMATTR_DM },
    { "simplebus2", 0x18000000, 0x18000000, SZ_64M, MEMATTR_DM },
    { 0, 0, 0, 0, 0 }
};

static struct memmap_desc guest1_device_md[] = {
    { "uart", 0x1C090000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
    { "sp804", 0x1C110000, 0x1C120000, SZ_4K, MEMATTR_DM },
    { "gicc", 0x2C000000 | GIC_OFFSET_GICC,
       CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K, MEMATTR_DM },
    {0, 0, 0, 0, 0}
};


#if _SMP_
static struct memmap_desc guest2_device_md[] = {
    { "uart", 0x1C090000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
    { "sp804", 0x1C110000, 0x1C120000, SZ_4K, MEMATTR_DM },
    { "gicc", 0x2C000000 | GIC_OFFSET_GICC,
       CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K, MEMATTR_DM },
    {0, 0, 0, 0, 0}
};

static struct memmap_desc guest3_device_md[] = {
    { "uart", 0x1C090000, 0x1C0B0000, SZ_4K, MEMATTR_DM },
    { "sp804", 0x1C110000, 0x1C120000, SZ_4K, MEMATTR_DM },
    { "gicc", 0x2C000000 | GIC_OFFSET_GICC,
       CFG_GIC_BASE_PA | GIC_OFFSET_GICVI, SZ_8K, MEMATTR_DM },
    {0, 0, 0, 0, 0}
};
#endif


/**
 * @brief Memory map for guest 0.
 */
static struct memmap_desc guest0_memory_md[] = {
    {"start", 0x00000000, 0, 0,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};

/**
 * @brief Memory map for guest 1.
 */
static struct memmap_desc guest1_memory_md[] = {
    {"start", 0x00000000, 0, 0,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};


#if _SMP_
/**
 * @brief Memory map for guest 2.
 */
static struct memmap_desc guest2_memory_md[] = {
    /* 256MB */
    {"start", 0x00000000, 0, 0x10000000,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};

/**
 * @brief Memory map for guest 3.
 */
static struct memmap_desc guest3_memory_md[] = {
    /* 256MB */
    {"start", 0x00000000, 0, 0x10000000,
     MEMATTR_NORMAL_OWB | MEMATTR_NORMAL_IWB
    },
    {0, 0, 0, 0,  0},
};
#endif

/* Memory Map for Guest 0 */
static struct memmap_desc *guest0_mdlist[] = {
    guest0_device_md,
    guest_md_empty,
    guest0_memory_md,
    guest_md_empty,
    0
};

/* Memory Map for Guest 1 */
static struct memmap_desc *guest1_mdlist[] = {
    guest1_device_md,
    guest_md_empty,
    guest1_memory_md,
    guest_md_empty,
    0
};


#if _SMP_
/* Memory Map for Guest 2 */
static struct memmap_desc *guest2_mdlist[] = {
    guest2_device_md,
    guest_md_empty,
    guest2_memory_md,
    guest_md_empty,
    0
};

/* Memory Map for Guest 3 */
static struct memmap_desc *guest3_mdlist[] = {
    guest3_device_md,
    guest_md_empty,
    guest3_memory_md,
    guest_md_empty,
    0
};
#endif

static struct memmap_desc **guest_mdlists[] =  {
    guest0_mdlist,
    guest1_mdlist
};

void setup_mdlist(vmid_t vmid, uint64_t start_addr, uint32_t offset) {
    guest_mdlists[vmid][MEMORY_DESCRIPT][MEMORY_START].pa = start_addr;
    guest_mdlists[vmid][MEMORY_DESCRIPT][MEMORY_START].size = offset;
}

vmid_t get_vmid() {
    if (current_vmid == VMID_INVALID)
        current_vmid = INIT_VMID;
    return current_vmid;
}

vmid_t get_next_vmid() {
    if (current_vmid == VMID_INVALID)
        current_vmid = INIT_VMID;
    return current_vmid++;
}

void set_vmid(vmid_t vmid) {
    current_vmid = vmid;
}

vmid_t create_vm(uint32_t num_vcpu, uint64_t start_addr,
        uint32_t offset, struct guest_virqmap* irqmaps) {

    vmid_t created_vmid = get_next_vmid();

    setup_mdlist(created_vmid, start_addr, offset);

    if (memory_guest_init(guest_mdlists[created_vmid], created_vmid))
        printh("[%s : %d] guest %d virtual memory init failed\n",
                __func__, __LINE__, get_vmid());

    set_virqmap(created_vmid, irqmaps, guest_virqmapds[created_vmid]);

	return created_vmid;
}
vm_state_t delete_vm(vmid_t vmid) {
	return UNDEFINED;
}
vm_state_t start_vm(vmid_t vmid) {
	return SCHEDULING;
}
vm_state_t shutdown_vm(vmid_t vmid) {
	return DEFINED;
}
