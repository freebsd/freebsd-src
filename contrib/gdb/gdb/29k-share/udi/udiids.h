/* This file contains the DFE and TIP IDs to be used by AMD products for
   the UDICapabilities call.
   
   Copyright 1993 Free Software Foundation, Inc.
   
   This file is part of GDB.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

	/* Company Codes -- AMD assigns these */
#define UDICompanyCode_AMD 1

	/* Build a UDIID given a CompanyProdCode and 3 version pieces */
#define UDIID(CompanyProdCode, v1,v2,v3) ((((CompanyProdCode) & 0xfffff)<<12)+\
				  (((v1)&0xf)<<8) + (((v2)&0xf)<<4) + ((v3)&0xf)) 


	/* Extract a CompanyProdCode or a Version from a UDIID */
#define UDIID_CompanyProdCode(id) (((id)>>12) & 0xfffff)
#define UDIID_Version(id) ((id)&0xfff)


#define UDIAMDProduct(ProdCode) ((UDICompanyCode_AMD<<4) + (ProdCode&0xf))

	/* AMD DFE Product Codes */
#define UDIProductCode_Mondfe UDIAMDProduct(0)
#define UDIProductCode_XRAY   UDIAMDProduct(1)
#define UDIProductCode_TIPTester  UDIAMDProduct(2)

	/* AMD TIP Product Codes (need not be distinct from DFE Product Codes) */
#define UDIProductCode_Montip UDIAMDProduct(0)
#define UDIProductCode_Isstip UDIAMDProduct(1)


#define UDILatestVersion 0x120	/* UDI 1.2.0, can be used in DFE and TIP desired UDI params */

