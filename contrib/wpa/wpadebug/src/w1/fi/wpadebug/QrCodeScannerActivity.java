/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2018, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;

public class QrCodeScannerActivity extends Activity {

    private static final String TAG = "wpadebug";
    private static final String RESULT = "SCAN_RESULT";
    private static final String FILE_NAME = "wpadebug_qrdata.txt";
    private static final String ACTION = "com.google.zxing.client.android.SCAN";

    private static final int QRCODE = 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent intent = new Intent();
        intent.setAction(ACTION);
	intent.putExtra("SCAN_MODE", "QR_CODE_MODE");
	intent.putExtra("PROMPT_MESSAGE",
			"Place a QR Code inside the viewfinder rectangle to scan it.");
        try {
            startActivityForResult(intent, QRCODE);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "No QR code scanner found with name=" + ACTION);
            Toast.makeText(QrCodeScannerActivity.this, "QR code scanner not found", Toast.LENGTH_SHORT).show();
            finish();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
	Log.d(TAG, "onActivityResult: requestCode=" + requestCode + " resultCode=" + resultCode);
        if (requestCode == QRCODE && resultCode == RESULT_OK) {
	    String contents = data.getStringExtra(RESULT);
	    writeToFile(contents);
	    Log.d(TAG, "onActivityResult: QRCODE RESULT_OK: " + contents);
	    finishActivity(requestCode);
            finish();
        }
    }

    public void writeToFile(String data)
    {
        File file = new File("/sdcard", FILE_NAME);
        try
        {
            file.createNewFile();
            FileOutputStream fOut = new FileOutputStream(file);
            OutputStreamWriter myOutWriter = new OutputStreamWriter(fOut);
            myOutWriter.append(data);

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
