// $Id: Ssh.java,v 1.2 2001/07/30 20:08:14 rees Exp $
//
// Ssh.java
// SSH / smartcard integration project, smartcard side
//
// Tomoko Fukuzawa, created, Feb., 2000
//
// Naomaru Itoi, modified, Apr., 2000
//

// copyright 2000
// the regents of the university of michigan
// all rights reserved
//
// permission is granted to use, copy, create derivative works
// and redistribute this software and such derivative works
// for any purpose, so long as the name of the university of
// michigan is not used in any advertising or publicity
// pertaining to the use or distribution of this software
// without specific, written prior authorization.  if the
// above copyright notice or any other identification of the
// university of michigan is included in any copy of any
// portion of this software, then the disclaimer below must
// also be included.
//
// this software is provided as is, without representation
// from the university of michigan as to its fitness for any
// purpose, and without warranty by the university of
// michigan of any kind, either express or implied, including
// without limitation the implied warranties of
// merchantability and fitness for a particular purpose. the
// regents of the university of michigan shall not be liable
// for any damages, including special, indirect, incidental, or
// consequential damages, with respect to any claim arising
// out of or in connection with the use of the software, even
// if it has been or is hereafter advised of the possibility of
// such damages.

import javacard.framework.*;
import javacardx.framework.*;
import javacardx.crypto.*;

public class Ssh extends javacard.framework.Applet
{
    /* constants declaration */
    // code of CLA byte in the command APDU header
    static final byte Ssh_CLA =(byte)0x05;

    // codes of INS byte in the command APDU header
    static final byte DECRYPT = (byte) 0x10;
    static final byte GET_KEYLENGTH = (byte) 0x20;
    static final byte GET_PUBKEY = (byte) 0x30;
    static final byte GET_RESPONSE = (byte) 0xc0;

    /* instance variables declaration */
    static final short keysize = 1024;

    //RSA_CRT_PrivateKey rsakey;
    AsymKey rsakey;
    CyberflexFile file;
    CyberflexOS os;

    byte buffer[];

    static byte[] keyHdr = {(byte)0xC2, (byte)0x01, (byte)0x05};

    private Ssh()
    {
	file = new CyberflexFile();
	os = new CyberflexOS();

	rsakey = new RSA_CRT_PrivateKey (keysize);

	if ( ! rsakey.isSupportedLength (keysize) )
	    ISOException.throwIt (ISO.SW_WRONG_LENGTH);

	register();
    } // end of the constructor

    public boolean select() {
	if (!rsakey.isInitialized())
	    rsakey.setKeyInstance ((short)0xc8, (short)0x10);

	return true;
    }

    public static void install(APDU apdu)
    {
	new Ssh();	// create a Ssh applet instance (card)
    } // end of install method

    public static void main(String args[]) {
	ISOException.throwIt((short) 0x9000);
    }

    public void process(APDU apdu)
    {
	// APDU object carries a byte array (buffer) to
	// transfer incoming and outgoing APDU header
	// and data bytes between card and CAD
	buffer = apdu.getBuffer();

	// verify that if the applet can accept this
	// APDU message
	// NI: change suggested by Wayne Dyksen, Purdue
	if (buffer[ISO.OFFSET_INS] == ISO.INS_SELECT)
	    ISOException.throwIt(ISO.SW_NO_ERROR);

	switch (buffer[ISO.OFFSET_INS]) {
	case DECRYPT:
	    if (buffer[ISO.OFFSET_CLA] != Ssh_CLA)
		ISOException.throwIt(ISO.SW_CLA_NOT_SUPPORTED);
	    //decrypt (apdu);
	    short size = (short) (buffer[ISO.OFFSET_LC] & 0x00FF);

	    if (apdu.setIncomingAndReceive() != size)
		ISOException.throwIt (ISO.SW_WRONG_LENGTH);

	    rsakey.cryptoUpdate (buffer, (short) ISO.OFFSET_CDATA, size,
				 buffer, (short) ISO.OFFSET_CDATA);

	    apdu.setOutgoingAndSend ((short) ISO.OFFSET_CDATA, size);
	    return;
	case GET_PUBKEY:
	    file.selectFile((short)(0x3f<<8)); // select root
	    file.selectFile((short)(('s'<<8)|'h')); // select public key file
	    os.readBinaryFile (buffer, (short)0, (short)0, (short)(keysize/8));
	    apdu.setOutgoingAndSend((short)0, (short)(keysize/8));
	    return;
	case GET_KEYLENGTH:
	    buffer[0] = (byte)((keysize >> 8) & 0xff);
	    buffer[1] = (byte)(keysize & 0xff);
	    apdu.setOutgoingAndSend ((short)0, (short)2);
	    return;
	case GET_RESPONSE:
	    return;
	default:
	    ISOException.throwIt (ISO.SW_INS_NOT_SUPPORTED);
	}

    } // end of process method

} // end of class Ssh
