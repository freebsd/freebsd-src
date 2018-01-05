/*
 * top of the
 * definition block
 */
DefinitionBlock(
    "converterSample.aml", /* These comments          */
    "DSDT",               /* within the              */
    0x02,                 /* definition block header */
    "Intel",              /* are not retained.      */
    "Many",               /* They will be            */
    0x00000001            /* Discarded               */)
{

    /* first comment of named object b */
    Name (b, 5)
    Name(p008, Package()
    {
	0, 0,
	0, 0xffffffff,
	0x00012345, 0x00007abc,
	0x00000012, 0x00000034,
	0x00000001, 0x000000ff,
	0x00000001, 0x0000ffff,
	0x00000001, 0xffffffff,

	// bit-size of multiplicand
	0x67812345, 2,

	// bit-size of multiplier
	3, 0x45678123,

	0xffffffff, 0xffffffff,

	// ACPI: Overflow conditions are ignored and results are undefined.
    })

    Method(MAIN) {
        /**********************************************************************
         *                                                                    *
         *                     This is a long                                 *
         *                       multi-line                                   *
         *                         comment                                    *
         *                                                                    *
         **********************************************************************/
        //c12
        if(1==1)//c13
        {   //c14
            Name(b,0);
        }  //c15
    }

   //c16
   Name (a,
       Package(3)
       {/*c20*/
            0x04, /*c21*/
            /*c22*/
            0x05, /*c23*/
            0x06  /*c24*/
       }/*c25*/
   )/*c26*/


    //c34
    Method(SCOP)
    {
        //c35
        Name (a1, 0x04)
    }

    OperationRegion(GNVS,SystemMemory,0xFFFF0000,0xAA55)

    Field(GNVS,AnyAcc,Lock,Preserve)
    {
        //c36
        Offset(0),//c37
        OSYS, 8//c38
    }


} //c39
/*ending
  comment*/
