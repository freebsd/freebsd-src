/*
 *
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 *
 * $Id$
 */

import javax.security.auth.login.*;
import javax.security.auth.callback.*;

public class KerberosInit {

    private class TestCallBackHandler implements CallbackHandler {
	
	public void handle(Callback[] callbacks)
	    throws UnsupportedCallbackException {
	    for (int i = 0; i < callbacks.length; i++) {
		if (callbacks[i] instanceof TextOutputCallback) {
		    TextOutputCallback toc = (TextOutputCallback)callbacks[i];
		    System.out.println(toc.getMessage());
		} else if (callbacks[i] instanceof NameCallback) {
		    NameCallback nc = (NameCallback)callbacks[i];
		    nc.setName("lha");
		} else if (callbacks[i] instanceof PasswordCallback) {
		    PasswordCallback pc = (PasswordCallback)callbacks[i];
		    pc.setPassword("foo".toCharArray());
		} else {
		    throw new
			UnsupportedCallbackException(callbacks[i],
						     "Unrecognized Callback");
		}
	    }
	}
    }
    private TestCallBackHandler getHandler() {
	return new TestCallBackHandler();
    }

    public static void main(String[] args) {

        LoginContext lc = null;
        try {
            lc = new LoginContext("kinit", new KerberosInit().getHandler());
        } catch (LoginException e) {
            System.err.println("Cannot create LoginContext. " + e.getMessage());
	    e.printStackTrace();
            System.exit(1);
        } catch (SecurityException e) {
            System.err.println("Cannot create LoginContext. " + e.getMessage());
	    e.printStackTrace();
            System.exit(1);
        } 

        try {
            lc.login();
        } catch (LoginException e) {
            System.err.println("Authentication failed:" + e.getMessage());
	    e.printStackTrace();
            System.exit(1);
        }

	System.out.println("lc.login ok");
	System.exit(0);
    }
}

