/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2018, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import android.app.Activity;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;

public class InputUri extends Activity {

    private EditText mEditText;
    private Button mSubmitButton;
    private String mUriText;
    private static final String FILE_NAME = "wpadebug_qrdata.txt";
    private static final String TAG = "wpadebug";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.input_uri);
        mEditText = (EditText)findViewById(R.id.edit_uri);
        mSubmitButton = (Button)findViewById(R.id.submit_uri);

	mEditText.addTextChangedListener(new TextWatcher() {
		@Override
		public void onTextChanged(CharSequence s, int start, int before,
					  int count) {
		    mUriText = mEditText.getText().toString();
		    if (mUriText.startsWith("DPP:") &&
			mUriText.endsWith(";;")) {
			writeToFile(mUriText);
			finish();
		    }
		}

		@Override
		public void beforeTextChanged(CharSequence s, int start,
					      int count, int after) {
		}

		@Override
		public void afterTextChanged(Editable s) {
		}
	    });
    }

    @Override
    protected void onResume() {
        super.onResume();
        mSubmitButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                mUriText = mEditText.getText().toString();
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        writeToFile(mUriText);

                        InputUri.this.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                finish();
                            }
                        });
                    }
                }).start();

            }

        });
    }

    public void writeToFile(String data)
    {
        File file = new File("/sdcard", FILE_NAME);
        try
        {
            file.createNewFile();
            FileOutputStream fOut = new FileOutputStream(file);
            OutputStreamWriter myOutWriter = new OutputStreamWriter(fOut);
            myOutWriter.append(mUriText);
            myOutWriter.close();

            fOut.flush();
            fOut.close();
        }
        catch (IOException e)
        {
            Log.e(TAG, "File write failed: " + e.toString());
        }
    }
}
