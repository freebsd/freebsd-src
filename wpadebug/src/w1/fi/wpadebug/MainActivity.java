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
import android.view.View;
import android.content.Intent;
import android.content.Context;
import android.content.DialogInterface;
import android.widget.EditText;
import android.widget.Toast;
import android.util.Log;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiConfiguration;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;

public class MainActivity extends Activity
{
    public final static String EXTRA_MESSAGE = "w1.fi.wpadebug.MESSAGE";
    private static final String TAG = "wpadebug";

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
    }

    public void runCommands(View view)
    {
	Intent intent = new Intent(this, CommandListActivity.class);
	startActivity(intent);
    }

    public void runQrScan(View view)
    {
	Intent intent = new Intent(this, QrCodeScannerActivity.class);
	startActivity(intent);
    }

    public void runQrInput(View view)
    {
	Intent intent = new Intent(this, InputUri.class);
	startActivity(intent);
    }

    public void runQrDisplay(View view)
    {
	Intent intent = new Intent(this, QrCodeDisplayActivity.class);
	startActivity(intent);
    }

    public void runWpaCommands(View view)
    {
	Intent intent = new Intent(this, WpaCommandListActivity.class);
	startActivity(intent);
    }

    public void runWpaCredentials(View view)
    {
	Intent intent = new Intent(this, WpaCredActivity.class);
	startActivity(intent);
    }

    public void runWpaCliCmd(View view)
    {
	Intent intent = new Intent(this, DisplayMessageActivity.class);
	EditText editText = (EditText) findViewById(R.id.edit_cmd);
	String cmd = editText.getText().toString();
	if (cmd.trim().length() == 0) {
	    show_alert("wpa_cli command", "Invalid command");
	    return;
	}
	wpaCmd(view, cmd);
    }

    public void wpaLogLevelInfo(View view)
    {
	wpaCmd(view, "LOG_LEVEL INFO 1");
    }

    public void wpaLogLevelDebug(View view)
    {
	wpaCmd(view, "LOG_LEVEL DEBUG 1");
    }

    public void wpaLogLevelExcessive(View view)
    {
	wpaCmd(view, "LOG_LEVEL EXCESSIVE 1");
    }

    private void wpaCmd(View view, String cmd)
    {
	Intent intent = new Intent(this, DisplayMessageActivity.class);
	String message = run("wpa_cli " + cmd);
	if (message == null)
	    return;
	intent.putExtra(EXTRA_MESSAGE, message);
	startActivity(intent);
    }

    private String run(String cmd)
    {
	try {
	    Log.d(TAG, "Running external process: " + cmd);
	    Process proc = Runtime.getRuntime().exec(new String[]{"/system/bin/mksh-su", "-c", cmd});
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

    private void show_alert(String title, String message)
    {
	AlertDialog.Builder alert = new AlertDialog.Builder(this);
	alert.setTitle(title);
	alert.setMessage(message);
	alert.setPositiveButton("OK", new DialogInterface.OnClickListener() {
		public void onClick(DialogInterface dialog, int id)
		{
		}
	    });
	alert.create().show();
    }

    public void wifiManagerInfo(View view)
    {
	Intent intent = new Intent(this, DisplayMessageActivity.class);
	WifiManager manager = (WifiManager) getSystemService(Context.WIFI_SERVICE);
	String message = "WifiState: " + manager.getWifiState() + "\n" +
	    "WifiEnabled: " + manager.isWifiEnabled() + "\n" +
	    "pingSupplicant: " + manager.pingSupplicant() + "\n" +
	    "DhcpInfo: " + manager.getDhcpInfo().toString() + "\n";
	intent.putExtra(EXTRA_MESSAGE, message);
	startActivity(intent);
    }

    public void wifiInfo(View view)
    {
	Intent intent = new Intent(this, DisplayMessageActivity.class);
	WifiManager manager = (WifiManager) getSystemService(Context.WIFI_SERVICE);
	WifiInfo wifi = manager.getConnectionInfo();
	String message = wifi.toString() + "\n" + wifi.getSupplicantState();
	intent.putExtra(EXTRA_MESSAGE, message);
	startActivity(intent);
    }

    public void wifiConfiguredNetworks(View view)
    {
	Intent intent = new Intent(this, DisplayMessageActivity.class);
	WifiManager manager = (WifiManager) getSystemService(Context.WIFI_SERVICE);
	StringBuilder sb = new StringBuilder();
	for (WifiConfiguration n: manager.getConfiguredNetworks())
	    sb.append(n.toString() + "\n");
	intent.putExtra(EXTRA_MESSAGE, sb.toString());
	startActivity(intent);
    }

    public void nfcWpsHandoverRequest(View view)
    {
	NfcAdapter nfc;
	nfc = NfcAdapter.getDefaultAdapter(this);
	if (nfc == null) {
	    Toast.makeText(this, "NFC is not available",
			   Toast.LENGTH_LONG).show();
	    return;
	}

	NdefMessage msg;
	msg = new NdefMessage(new NdefRecord[] {
		NdefRecord.createMime("application/vnd.wfa.wsc",
				      new byte[0])
	    });

	nfc.setNdefPushMessage(msg, this);
	Toast.makeText(this, "NFC push message (WSC) configured",
		       Toast.LENGTH_LONG).show();
    }
}
