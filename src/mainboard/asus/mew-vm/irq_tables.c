#include <arch/pirq_routing.h>

static const struct irq_routing_table intel_irq_routing_table = {
	PIRQ_SIGNATURE,  /* u32 signature */
	PIRQ_VERSION,    /* u16 version   */
	32+16*CONFIG_IRQ_SLOT_COUNT,	 /* there can be total CONFIG_IRQ_SLOT_COUNT devices on the bus */
	0x00,		 /* Where the interrupt router lies (bus) */
	(0x11 << 3)|0x0,   /* Where the interrupt router lies (dev) */
	0xe20,		 /* IRQs devoted exclusively to PCI usage */
	0x8086,		 /* Vendor */
	0x7120,		 /* Device */
	0,		 /* Miniport data */
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, /* u8 rfu[11] */
	0x89,		 /*  u8 checksum , this has to set to some value
that would give 0 after the sum of all bytes for this structure (including checksum) */
	{
		/* bus,       dev|fn,   {link, bitmap}, {link, bitmap}, {link, bitmap}, {link, bitmap},  slot, rfu */
		{0x00,(0x08 << 3)|0x0, {{0x02, 0xdea0}, {0x03, 0xdea0}, {0x04, 0xdea0}, {0x01, 0x0dea0}}, 0x1, 0x0},
		{0x00,(0x09 << 3)|0x0, {{0x03, 0xdea0}, {0x04, 0xdea0}, {0x01, 0xdea0}, {0x02, 0x0dea0}}, 0x2, 0x0},
		{0x00,(0x0a << 3)|0x0, {{0x04, 0xdea0}, {0x01, 0xdea0}, {0x02, 0xdea0}, {0x03, 0x0dea0}}, 0x3, 0x0},
		{0x00,(0x0b << 3)|0x0, {{0x04, 0xdea0}, {0x01, 0xdea0}, {0x02, 0xdea0}, {0x03, 0x0dea0}}, 0x4, 0x0},
		{0x00,(0x0c << 3)|0x0, {{0x01, 0xdea0}, {0x02, 0xdea0}, {0x03, 0xdea0}, {0x04, 0x0dea0}}, 0x5, 0x0},
		{0x00,(0x0d << 3)|0x0, {{0x01, 0xdea0}, {0x02, 0xdea0}, {0x03, 0xdea0}, {0x04, 0x0dea0}}, 0x6, 0x0},
		{0x00,(0x11 << 3)|0x0, {{0x00, 0xdea0}, {0x00, 0xdea0}, {0x03, 0xdea0}, {0x04, 0x0dea0}}, 0x0, 0x0},
		{0x00,(0x0f << 3)|0x0, {{0x01, 0xdea0}, {0x02, 0xdea0}, {0x03, 0xdea0}, {0x04, 0x0dea0}}, 0x0, 0x0},
		{0x00,(0x01 << 3)|0x0, {{0x01, 0xdea0}, {0x02, 0xdea0}, {0x03, 0xdea0}, {0x04, 0x0dea0}}, 0x0, 0x0},
		{0x00,(0x10 << 3)|0x0, {{0x01, 0xdea0}, {0x02, 0xdea0}, {0x03, 0xdea0}, {0x04, 0x0dea0}}, 0x0, 0x0},
		{0x00,(0x12 << 3)|0x0, {{0x01, 0xdea0}, {0x00, 0xdea0}, {0x00, 0xdea0}, {0x00, 0x0dea0}}, 0x0, 0x0},
	}
};

unsigned long write_pirq_routing_table(unsigned long addr)
{
	return copy_pirq_routing_table(addr, &intel_irq_routing_table);
}
