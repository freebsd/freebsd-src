// $Id: Ssh.java,v 1.3 2002/05/22 04:24:02 djm Exp $
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
    // Change this when the applet changes; hi byte is major, low byte is minor
    static final short applet_version = (short)0x0102;

    /* constants declaration */
    // code of CLA byte in the command APDU header
    static final byte Ssh_CLA =(byte)0x05;

    // codes of INS byte in the command APDU header
    static final byte DECRYPT = (byte) 0x10;
    static final byte GET_KEYLENGTH = (byte) 0x20;
    static final byte GET_PUBKEY = (byte) 0x30;
    static final byte GET_VERSION = (byte) 0x32;
    static final byte GET_RESPONSE = (byte) 0xc0;

    static final short keysize = 1024;
    static final short root_fid = (short)0x3f00;
    static final short privkey_fid = (short)0x0012;
    static final short pubkey_fid = (short)(('s'<<8)|'h');

    /* instance variables declaration */
    AsymKey rsakey;
    CyberflexFile file;
    CyberflexOS os;

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
	byte buffer[] = apdu.getBuffer();
	short size, st;

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
	    size = (short) (buffer[ISO.OFFSET_LC] & 0x00FF);

	    if (apdu.setIncomingAndReceive() != size)
		ISOException.throwIt (ISO.SW_WRONG_LENGTH);

	    // check access; depends on bit 2 (x/a)
	    file.selectFile(root_fid);
	    file.selectFile(privkey_fid);
	    st = os.checkAccess(ACL.EXECUTE);
	    if (st != ST.ACCESS_CLEARED) {
		CyberflexAPDU.prepareSW1SW2(st);
		ISOException.throwIt(CyberflexAPDU.getSW1SW2());
	    }

	    rsakey.cryptoUpdate (buffer, (short) ISO.OFFSET_CDATA, size,
				 buffer, (short) ISO.OFFSET_CDATA);

	    apdu.setOutgoingAndSend ((short) ISO.OFFSET_CDATA, size);
	    break;
	case GET_PUBKEY:
	    file.selectFile(root_fid); // select root
	    file.selectFile(pubkey_fid); // select public key file
	    size = (short)(file.getFileSize() - 16);
	    st = os.readBinaryFile(buffer, (short)0, (short)0, size);
	    if (st == ST.SUCCESS)
		apdu.setOutgoingAndSend((short)0, size);
	    else {
		CyberflexAPDU.prepareSW1SW2(st);
		ISOException.throwIt(CyberflexAPDU.getSW1SW2());
	    }
	    break;
	case GET_KEYLENGTH:
	    Util.setShort(buffer, (short)0, keysize);
	    apdu.setOutgoingAndSend ((short)0, (short)2);
	    break;
	case GET_VERSION:
	    Util.setShort(buffer, (short)0, applet_version);
	    apdu.setOutgoingAndSend ((short)0, (short)2);
	    break;
	case GET_RESPONSE:
	    break;
	default:
	    ISOException.throwIt (ISO.SW_INS_NOT_SUPPORTED);
	}

    } // end of process method

} // end of class Ssh
