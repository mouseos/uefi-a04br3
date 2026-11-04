/** A04BR3 experimental ACPI 5.0 table headers. SPDX-License-Identifier: BSD-2-Clause-Patent **/
#ifndef A04BR3_ACPI_TABLES_H
#define A04BR3_ACPI_TABLES_H
#include <IndustryStandard/Acpi.h>
#define A04_ACPI_HEADER(Signature, Type, Revision) { Signature, sizeof(Type), Revision, 0, {'B','E','N','E','S','S'}, SIGNATURE_64('A','0','4','B','R','3',' ',' '), 1, SIGNATURE_32('E','D','K','2'), 1 }
#endif
