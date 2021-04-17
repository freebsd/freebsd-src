/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.EditText;

public class WpaCredEditActivity extends Activity
{
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.cred_edit);
    }

    public void credSave(View view)
    {
	Intent data = new Intent();
	EditText edit;

	edit = (EditText) findViewById(R.id.cred_edit_username);
	data.putExtra("username", edit.getText().toString());

	edit = (EditText) findViewById(R.id.cred_edit_realm);
	data.putExtra("realm", edit.getText().toString());

	edit = (EditText) findViewById(R.id.cred_edit_password);
	data.putExtra("password", edit.getText().toString());

	edit = (EditText) findViewById(R.id.cred_edit_domain);
	data.putExtra("domain", edit.getText().toString());

	edit = (EditText) findViewById(R.id.cred_edit_imsi);
	data.putExtra("imsi", edit.getText().toString());

	setResult(Activity.RESULT_OK, data);
	finish();
    }

    public void credCancel(View view)
    {
	setResult(Activity.RESULT_CANCELED);
	finish();
    }
}
