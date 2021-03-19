/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2018, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import android.app.Activity;
import android.util.Log;
import android.content.Intent;
import android.hardware.Camera;
import android.os.Bundle;

public class QrCodeReadActivity extends Activity {

    private static final String TAG = "wpadebug";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int numberOfCameras = Camera.getNumberOfCameras();

        if (numberOfCameras > 0) {
            Log.e(TAG, "Number of cameras found: " + numberOfCameras);
            Intent QrCodeScanIntent = new Intent(QrCodeReadActivity.this,
						 QrCodeScannerActivity.class);
            QrCodeReadActivity.this.startActivity(QrCodeScanIntent);
            finish();
        } else {
            Log.e(TAG, "No cameras found, input the QR Code");
            Intent QrCodeInputIntent = new Intent(QrCodeReadActivity.this,
						  InputUri.class);
            QrCodeReadActivity.this.startActivity(QrCodeInputIntent);
            finish();
        }
    }
}
