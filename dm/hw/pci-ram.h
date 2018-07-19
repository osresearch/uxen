#ifndef _HW_PCI_RAM_H_
#define _HW_PCI_RAM_H_

void pci_ram_config_write(PCIDevice *d, uint32_t addr, uint32_t val, int len);
void pci_ram_pre_save(PCIDevice *d);
void pci_ram_post_load(PCIDevice *d, int version_id);
void pci_ram_post_save(PCIDevice *d);

#endif /* _HW_PCI_RAM_H_ */
