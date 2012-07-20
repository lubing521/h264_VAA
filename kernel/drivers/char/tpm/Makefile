#
# Makefile for the kernel tpm device drivers.
#
obj-$(CONFIG_TCG_TPM) += tpm.o
ifdef CONFIG_ACPI
	obj-$(CONFIG_TCG_TPM) += tpm_bios.o
endif
obj-$(CONFIG_TCG_TIS) += tpm_tis.o
obj-$(CONFIG_IRQ) += sep0611_enable_irq60.o
obj-$(CONFIG_PMU) += sep0611_pmu.o
obj-$(CONFIG_TCG_NSC) += tpm_nsc.o
obj-$(CONFIG_TCG_ATMEL) += tpm_atmel.o
obj-$(CONFIG_TCG_INFINEON) += tpm_infineon.o
