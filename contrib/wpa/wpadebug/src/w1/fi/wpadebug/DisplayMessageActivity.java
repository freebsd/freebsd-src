/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import android.app.Activity;
import android.os.Bundle;
import android.os.Parcelable;
import android.view.MenuItem;
import android.content.Intent;
import android.widget.TextView;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;

public class DisplayMessageActivity extends Activity
{
    private static final String TAG = "wpadebug";

    String byteArrayHex(byte[] a) {
	StringBuilder sb = new StringBuilder();
	for (byte b: a)
	    sb.append(String.format("%02x", b));
	return sb.toString();
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
	Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);

	// Get the message from the intent
	Intent intent = getIntent();
	String action = intent.getAction();
	Log.d(TAG, "onCreate: action=" + action);

	String message = intent.getStringExtra(MainActivity.EXTRA_MESSAGE);

	TextView textView = new TextView(this);
	textView.setText(message);
	textView.setMovementMethod(new ScrollingMovementMethod());
        setContentView(textView);
    }
}
