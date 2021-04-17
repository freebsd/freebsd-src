/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.http.SslError;
import android.os.Bundle;
import android.util.Log;
import android.view.Window;
import android.webkit.SslErrorHandler;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

public class WpaWebViewActivity extends Activity
{
    private static final String TAG = "wpadebug";
    private static final String EXTRA_MESSAGE = "w1.fi.wpadebug.URL";
    private WebView mWebView;
    final Activity activity = this;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
	Log.d(TAG, "WpaWebViewActivity::onCreate");
        super.onCreate(savedInstanceState);

	Intent intent = getIntent();
	String url = intent.getStringExtra(EXTRA_MESSAGE);
	Log.d(TAG, "url=" + url);
	if (url.equals("FINISH")) {
	    finish();
	    return;
	}

	mWebView = new WebView(this);
	mWebView.getSettings().setJavaScriptEnabled(true);
	mWebView.setWebViewClient(new WpaWebViewClient());

	getWindow().requestFeature(Window.FEATURE_PROGRESS);

	mWebView.setWebChromeClient(new WebChromeClient()
	    {
		public void onProgressChanged(WebView view, int progress)
		{
		    Log.d(TAG, "progress=" + progress);
		    activity.setProgress(progress * 1000);
		}
	    });

        setContentView(mWebView);

	mWebView.loadUrl(url);
    }

    @Override
    public void onResume()
    {
	Log.d(TAG, "WpaWebViewActivity::onResume");
        super.onResume();
    }

    @Override
    protected void onNewIntent(Intent intent)
    {
	Log.d(TAG, "WpaWebViewActivity::onNewIntent");
	super.onNewIntent(intent);
	String url = intent.getStringExtra(EXTRA_MESSAGE);
	Log.d(TAG, "url=" + url);
	setIntent(intent);
	if (url.equals("FINISH")) {
	    finish();
	    return;
	}
	mWebView.loadUrl(url);
    }

    private class WpaWebViewClient extends WebViewClient {
	@Override
	public boolean shouldOverrideUrlLoading(WebView view, String url)
	{
	    Log.d(TAG, "shouldOverrideUrlLoading: url=" + url);
	    Intent intent = getIntent();
	    intent.putExtra(EXTRA_MESSAGE, url);

	    view.loadUrl(url);
	    return true;
	}

	@Override
	public void onPageFinished(WebView view, String url)
	{
	    Log.d(TAG, "onPageFinished: url=" + url);
	}

	public void onReceivedError(WebView view, int errorCode,
				    String description, String failingUrl)
	{
	    Log.d(TAG, "Failed to load page: errorCode=" +
		  errorCode + " description=" + description +
		  " URL=" + failingUrl);
	    Toast.makeText(activity, "Failed to load page: " +
			   description + " (URL=" + failingUrl + ")",
			   Toast.LENGTH_LONG).show();
	}

	@Override
	public void onReceivedSslError(WebView view, SslErrorHandler handler,
				       SslError error)
	{
	    Log.d(TAG, "SSL error: " + error);

	    final SslErrorHandler h = handler;
	    AlertDialog.Builder alert = new AlertDialog.Builder(activity);
	    alert.setTitle("SSL error - Continue?");
	    alert.setMessage(error.toString())
		.setCancelable(false)
		.setPositiveButton("Yes", new DialogInterface.OnClickListener()
		    {
			public void onClick(DialogInterface dialog, int id)
			{
			    h.proceed();
			}
		    })
		.setNegativeButton("No", new DialogInterface.OnClickListener()
		    {
			public void onClick(DialogInterface dialog, int id)
			{
			    h.cancel();
			}
		    });
	    alert.show();
	}
    }
}
