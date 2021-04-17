/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import java.util.ArrayList;
import java.util.Scanner;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.InputStream;
import java.io.IOException;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Parcelable;
import android.view.View;
import android.widget.ListView;
import android.widget.ArrayAdapter;
import android.widget.Toast;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;

public class WpaCommandListActivity extends ListActivity
{
    private static final String TAG = "wpadebug";
    private static final String cmdfile = "/data/local/wpadebug.wpacmds";

    private void read_commands(ArrayList<CmdList> list, Scanner in)
    {
	in.useDelimiter("@");
	while (in.hasNext()) {
	    String title = in.next();
	    String cmd = in.nextLine().substring(1);
	    list.add(new CmdList(title, cmd));
	}
	in.close();
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

	ArrayList<CmdList> list = new ArrayList<CmdList>();

	FileReader in;
	try {
	    in = new FileReader(cmdfile);
	    read_commands(list, new Scanner(in));
	} catch (IOException e) {
	    Toast.makeText(this, "Could not read " + cmdfile,
			   Toast.LENGTH_SHORT).show();
	}

	InputStream inres;
	try {
	    inres = getResources().openRawResource(R.raw.wpa_commands);
	    read_commands(list, new Scanner(inres));
	} catch (android.content.res.Resources.NotFoundException e) {
	    Toast.makeText(this, "Could not read internal resource",
			   Toast.LENGTH_SHORT).show();
	}

	ArrayAdapter<CmdList> listAdapter;
	listAdapter = new ArrayAdapter<CmdList>(this, android.R.layout.simple_list_item_1, list);

	setListAdapter(listAdapter);
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id)
    {
	CmdList item = (CmdList) getListAdapter().getItem(position);
	Toast.makeText(this, "Running: " + item.command,
		       Toast.LENGTH_SHORT).show();
	String message = run(item.command);
	if (message == null)
	    return;
	Intent intent = new Intent(this, DisplayMessageActivity.class);
	intent.putExtra(MainActivity.EXTRA_MESSAGE, message);
	startActivity(intent);
    }

    private String run(String cmd)
    {
	try {
	    Process proc = Runtime.getRuntime().exec(new String[]{"/system/bin/mksh-su", "-c", "wpa_cli " + cmd});
	    BufferedReader reader = new BufferedReader(new InputStreamReader(proc.getInputStream()));
	    StringBuffer output = new StringBuffer();
	    int read;
	    char[] buffer = new char[1024];
	    while ((read = reader.read(buffer)) > 0)
		output.append(buffer, 0, read);
	    reader.close();
	    proc.waitFor();
	    return output.toString();
	} catch (IOException e) {
	    Toast.makeText(this, "Could not run command",
			   Toast.LENGTH_LONG).show();
	    return null;
	} catch (InterruptedException e) {
	    throw new RuntimeException(e);
	}
    }
}
