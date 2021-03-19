/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.NetworkInfo;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiInfo;
import android.os.Bundle;
import android.util.Log;

public class WifiReceiver extends BroadcastReceiver
{
    private static final String TAG = "wpadebug";

    @Override
    public void onReceive(Context c, Intent intent)
    {
	String act = intent.getAction();
	Log.d(TAG, "Received broadcast intent: action=" + act);

	Bundle bundles = intent.getExtras();
	if (bundles == null)
	    return;

	if (bundles.containsKey("bssid")) {
	    String val;
	    val = intent.getStringExtra("bssid");
	    if (val != null)
		Log.d(TAG, "  bssid: " + val);
	}

	if (bundles.containsKey("networkInfo")) {
	    NetworkInfo info;
	    info = (NetworkInfo) intent.getParcelableExtra("networkInfo");
	    if (info != null)
		Log.d(TAG, "  networkInfo: " + info);
	}

	if (bundles.containsKey("newRssi")) {
	    int val;
	    val = intent.getIntExtra("newRssi", -1);
	    Log.d(TAG, "  newRssi: " + val);
	}

	if (bundles.containsKey("newState")) {
	    SupplicantState state;
	    state = (SupplicantState) intent.getParcelableExtra("newState");
	    if (state != null)
		Log.d(TAG, "  newState: " + state);
	}

	if (bundles.containsKey("previous_wifi_state")) {
	    int wifi_state;
	    wifi_state = intent.getIntExtra("previous_wifi_state", -1);
	    if (wifi_state != -1)
		Log.d(TAG, "  previous_wifi_state: " + wifi_state);
	}

	if (bundles.containsKey("connected")) {
	    boolean connected;
	    connected = intent.getBooleanExtra("connected", false);
	    Log.d(TAG, "  connected: " + connected);
	}

	if (bundles.containsKey("supplicantError")) {
	    int error;
	    error = intent.getIntExtra("supplicantError", -1);
	    if (error != -1)
		Log.d(TAG, "  supplicantError: " + error);
	}

	if (bundles.containsKey("wifiInfo")) {
	    WifiInfo info;
	    info = (WifiInfo) intent.getParcelableExtra("wifiInfo");
	    if (info != null)
		Log.d(TAG, "  wifiInfo: " + info);
	}

	if (bundles.containsKey("wifi_state")) {
	    int wifi_state;
	    wifi_state = intent.getIntExtra("wifi_state", -1);
	    if (wifi_state != -1)
		Log.d(TAG, "  wifi_state: " + wifi_state);
	}
    }
}
