/*
 * Copyright 2008 mackerintel
 */

#ifndef __LIBSAIO_ACPI_PATCHER_H
#define __LIBSAIO_ACPI_PATCHER_H

#include "libsaio.h"
//Slice - it's bullshit to define variables in header file
//uint64_t acpi10_p;
//uint64_t acpi20_p;
//uint64_t smbios_p;

extern void *new_dsdt;
extern uint64_t smbios_p;

extern int setupAcpi();

extern EFI_STATUS addConfigurationTable();
//extern int search_and_get_acpi_fd(const char *, const char **);

extern EFI_GUID gEfiAcpiTableGuid;
extern EFI_GUID gEfiAcpi20TableGuid;

struct p_state 
{
	union 
	{
		uint16_t Control;
		struct 
		{
			uint8_t VID;	// Voltage ID
			uint8_t FID;	// Frequency ID
		};
	};
	
	uint8_t		CID;		// Compare ID
	uint32_t	Frequency;
};

#endif /* !__LIBSAIO_ACPI_PATCHER_H */
