/*
 * wpadebug - wpa_supplicant and Wi-Fi debugging app for Android
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

package w1.fi.wpadebug;

import java.util.ArrayList;
import java.util.ListIterator;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.InputStream;
import java.io.IOException;

import android.app.ListActivity;
import android.app.ActionBar;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.ListView;
import android.widget.ArrayAdapter;
import android.widget.Toast;
import android.widget.AdapterView.AdapterContextMenuInfo;

class Credential
{
    int id;
    String realm;
    String username;
    String domain;
    String imsi;

    public Credential(String entry)
    {
	String fields[] = entry.split("\t");
	id = Integer.parseInt(fields[0]);
	if (fields.length > 1)
	    realm = fields[1];
	else
	    realm = "";
	if (fields.length > 2)
	    username = fields[2];
	else
	    username = "";
	if (fields.length > 3 && fields[3].length() > 0)
	    domain = fields[3];
	else
	    domain = null;
	if (fields.length > 4 && fields[4].length() > 0)
	    imsi = fields[4];
	else
	    imsi = null;
    }

    public Credential(int _id, String _username, String _realm, String _domain,
		      String _imsi)
    {
	id = _id;
	username = _username;
	realm = _realm;
	domain = _domain;
	imsi = _imsi;
    }


    @Override
    public String toString()
    {
	String res = id + " - " + username + "@" + realm;
	if (domain != null)
	    res += " (domain=" + domain + ")";
	if (imsi != null)
	    res += " (imsi=" + imsi + ")";
	return res;
    }
}

public class WpaCredActivity extends ListActivity
{
    private static final String TAG = "wpadebug";
    static final int CRED_EDIT_REQ = 0;
    private ArrayList<Credential> mList;
    private ArrayAdapter<Credential> mListAdapter;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

	mList = new ArrayList<Credential>();

	String res = run("LIST_CREDS");
	if (res == null) {
	    Toast.makeText(this, "Could not get credential list",
			   Toast.LENGTH_LONG).show();
	    finish();
	    return;
	}

	String creds[] = res.split("\n");
	for (String cred: creds) {
	    if (Character.isDigit(cred.charAt(0)))
		mList.add(new Credential(cred));
	}

	mListAdapter = new ArrayAdapter<Credential>(this, android.R.layout.simple_list_item_1, mList);

	setListAdapter(mListAdapter);
	registerForContextMenu(getListView());

	ActionBar abar = getActionBar();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
	menu.add(0, 0, 0, "Add credential");
	return true;
    }

    protected void onActivityResult(int requestCode, int resultCode,
				    Intent data)
    {
	if (requestCode == CRED_EDIT_REQ) {
	    if (resultCode != RESULT_OK)
		return;

	    String username = data.getStringExtra("username");

	    String realm = data.getStringExtra("realm");

	    String domain = data.getStringExtra("domain");
	    if (domain != null && domain.length() == 0)
		domain = null;

	    String imsi = data.getStringExtra("imsi");
	    if (imsi != null && imsi.length() == 0)
		imsi = null;

	    String password = data.getStringExtra("password");
	    if (password != null && password.length() == 0)
		password = null;

	    String res = run("ADD_CRED");
	    if (res == null || res.contains("FAIL")) {
		Toast.makeText(this, "Failed to add credential",
			       Toast.LENGTH_LONG).show();
		return;
	    }

	    int id = -1;
	    String lines[] = res.split("\n");
	    for (String line: lines) {
		if (Character.isDigit(line.charAt(0))) {
		    id = Integer.parseInt(line);
		    break;
		}
	    }

	    if (id < 0) {
		Toast.makeText(this, "Failed to add credential (invalid id)",
			       Toast.LENGTH_LONG).show();
		return;
	    }

	    if (!set_cred_quoted(id, "username", username) ||
		!set_cred_quoted(id, "realm", realm) ||
		(password != null &&
		 !set_cred_quoted(id, "password", password)) ||
		(domain != null && !set_cred_quoted(id, "domain", domain)) ||
		(imsi != null && !set_cred_quoted(id, "imsi", imsi))) {
		run("REMOVE_CRED " + id);
		Toast.makeText(this, "Failed to set credential field",
			       Toast.LENGTH_LONG).show();
		return;
	    }

	    mListAdapter.add(new Credential(id, username, realm, domain, imsi));
	}
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
	if (item.getTitle().equals("Add credential")) {
	    startActivityForResult(new Intent(this, WpaCredEditActivity.class),
				   CRED_EDIT_REQ);
	    return true;
	}
	return false;
    }

    public void onCreateContextMenu(android.view.ContextMenu menu, View v,
				    android.view.ContextMenu.ContextMenuInfo menuInfo)
    {
	menu.add(0, v.getId(), 0, "Delete");
    }

    @Override
    public boolean onContextItemSelected(MenuItem item)
    {
	if (item.getTitle().equals("Delete")) {
	    AdapterContextMenuInfo info =
		(AdapterContextMenuInfo) item.getMenuInfo();
	    Credential cred = (Credential) getListAdapter().getItem(info.position);
	    String res = run("REMOVE_CRED " + cred.id);
	    if (res == null || !res.contains("OK")) {
		Toast.makeText(this, "Failed to delete credential",
			       Toast.LENGTH_LONG).show();
	    } else
		mListAdapter.remove(cred);
	    return true;
	}
	return super.onContextItemSelected(item);
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id)
    {
	Credential item = (Credential) getListAdapter().getItem(position);
	Toast.makeText(this, "Credential selected: " + item,
		       Toast.LENGTH_SHORT).show();
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

    private boolean set_cred(int id, String field, String value)
    {
	String res = run("SET_CRED " + id + " " + field + " " + value);
	return res != null && res.contains("OK");
    }

    private boolean set_cred_quoted(int id, String field, String value)
    {
	String value2 = "'\"" + value + "\"'";
	return set_cred(id, field, value2);
    }
}
