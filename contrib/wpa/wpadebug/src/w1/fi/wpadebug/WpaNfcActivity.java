/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.os.Parcelable;
import android.view.MenuItem;
import android.content.Intent;
import android.content.DialogInterface;
import android.widget.TextView;
import android.widget.Toast;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;

public class WpaNfcActivity extends Activity
{
    private static final String TAG = "wpadebug";

    String byteArrayHex(byte[] a) {
	StringBuilder sb = new StringBuilder();
	for (byte b: a)
	    sb.append(String.format("%02x", b));
	return sb.toString();
    }

    private void show_alert(String title, String message)
    {
	AlertDialog.Builder alert = new AlertDialog.Builder(this);
	alert.setTitle(title);
	alert.setMessage(message);
	alert.setPositiveButton("OK", new DialogInterface.OnClickListener() {
		public void onClick(DialogInterface dialog, int id)
		{
		    finish();
		}
	    });
	alert.create().show();
    }

    private String wpaCmd(String cmd)
    {
	try {
	    Log.d(TAG, "Executing wpaCmd: " + cmd);
	    Process proc = Runtime.getRuntime().exec(new String[]{"/system/bin/mksh-su", "-c", "wpa_cli " + cmd});
	    BufferedReader reader = new BufferedReader(new InputStreamReader(proc.getInputStream()));
	    StringBuffer output = new StringBuffer();
	    int read;
	    char[] buffer = new char[1024];
	    while ((read = reader.read(buffer)) > 0)
		output.append(buffer, 0, read);
	    reader.close();
	    proc.waitFor();
	    Log.d(TAG, "External process completed - exitValue " +
		  proc.exitValue());
	    return output.toString();
	} catch (IOException e) {
	    show_alert("Could not run external program",
		       "Execution of an external program failed. " +
		       "Maybe mksh-su was not installed.");
	    return null;
	} catch (InterruptedException e) {
	    throw new RuntimeException(e);
	}
    }

    public boolean report_tag_read(byte[] payload)
    {
	String res = wpaCmd("WPS_NFC_TAG_READ " + byteArrayHex(payload));
	if (res == null)
	    return false;
	if (!res.contains("OK")) {
	    Toast.makeText(this, "Failed to report WSC tag read to " +
			   "wpa_supplicant", Toast.LENGTH_LONG).show();
	} else {
	    Toast.makeText(this, "Reported WSC tag read to wpa_supplicant",
			   Toast.LENGTH_LONG).show();
	}
	finish();
	return true;
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
	super.onCreate(savedInstanceState);

	Intent intent = getIntent();
	String action = intent.getAction();
	Log.d(TAG, "onCreate: action=" + action);

	if (NfcAdapter.ACTION_NDEF_DISCOVERED.equals(action)) {
	    Log.d(TAG, "NDEF discovered");
	    Parcelable[] raw = intent.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES);
	    if (raw != null) {
		Log.d(TAG, "NDEF message count: " + raw.length);
		NdefMessage[] msgs = new NdefMessage[raw.length];
		for (int i = 0; i < raw.length; i++) {
		    msgs[i] = (NdefMessage) raw[i];
		    NdefRecord rec = msgs[i].getRecords()[0];
		    Log.d(TAG, "MIME type: " + rec.toMimeType());
		    byte[] a = rec.getPayload();
		    Log.d(TAG, "NDEF record: " + byteArrayHex(a));
		    if (rec.getTnf() == NdefRecord.TNF_MIME_MEDIA &&
			rec.toMimeType().equals("application/vnd/wfa.wsc")) {
			Log.d(TAG, "WSC tag read");
		    }

		    if (!report_tag_read(a))
			return;
		}
	    }
	}

	finish();
    }
}
