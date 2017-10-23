/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TPS650x30 PMIC register map.
 */

#ifndef __CROS_EC_PMIC_TPS650X30_H
#define __CROS_EC_PMIC_TPS650X30_H

/* I2C interface */
#define TPS650X30_I2C_ADDR1		(0x30 << 1)
#define TPS650X30_I2C_ADDR2		(0x32 << 1)
#define TPS650X30_I2C_ADDR3		(0x34 << 1)

/* TPS650X30 registers */
#define TPS650X30_REG_VENDORID		0x00
#define TPS650X30_REG_PBCONFIG		0x14
#define TPS650X30_REG_PGMASK1		0x18
#define TPS650X30_REG_VCCIOCNT		0x30
#define TPS650X30_REG_V5ADS3CNT		0x31
#define TPS650X30_REG_V33ADSWCNT	0x32
#define TPS650X30_REG_V18ACNT		0x34
#define TPS650X30_REG_V1P2UCNT		0x36
#define TPS650X30_REG_VRMODECTRL	0x3B
#define TPS650X30_REG_DISCHCNT1		0x3C
#define TPS650X30_REG_DISCHCNT2		0x3D
#define TPS650X30_REG_DISCHCNT3		0x3E
#define TPS650X30_REG_DISCHCNT4		0x3F
#define TPS650X30_REG_PWFAULT_MASK1	0xE5

/* TPS650X30 register values */
#define TPS650X30_VENDOR_ID		0x22

#endif	/* __CROS_EC_PMIC_TPS650X30_H */
