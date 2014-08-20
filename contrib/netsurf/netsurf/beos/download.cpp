/*
 * Copyright 2012 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define __STDBOOL_H__	1
#include <stdbool.h>
#include <sys/types.h>

extern "C" {
#include "desktop/download.h"
#include "desktop/gui.h"
#include "utils/utils.h"
}
#include "beos/download.h"

#include <File.h>
#include <FilePanel.h>
#include <Locker.h>
#include <Messenger.h>
#include <StatusBar.h>
#include <Window.h>

class NSDownloadWindow: public BWindow
{
	public:
		NSDownloadWindow(download_context* ctx);
		~NSDownloadWindow();

		void MessageReceived(BMessage* message);

		void Progress(int size);
		void Failure(const char* error);
		void Success();
	private:
		download_context* ctx;
		BStatusBar* bar;
		unsigned long progress;
		bool success;
};


struct gui_download_window {
	download_context* ctx;
	NSDownloadWindow* window;

	BLocker* storageLock;
	BDataIO* storage;
};


NSDownloadWindow::NSDownloadWindow(download_context* ctx)
	: BWindow(BRect(30, 30, 400, 200), "Downloads", B_TITLED_WINDOW,
		B_NOT_RESIZABLE)
	, ctx(ctx)
	, progress(0)
	, success(false)
{
	unsigned long dlsize = download_context_get_total_length(ctx);
	char* buffer = human_friendly_bytesize(dlsize);

	// Create the status bar
	BRect rect = Bounds();
	rect.InsetBy(3, 3);
	bar = new BStatusBar(rect, "progress",
		download_context_get_filename(ctx), buffer);
	bar->SetMaxValue(dlsize);

	// Create the backgroundview (just so that the area around the progress bar
	// is B_PANEL_BACKGROUND_COLOR instead of white)
	BView* back = new BView(Bounds(), "back", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
	back->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	// Add the views to the window
	back->AddChild(bar);
	AddChild(back);

	// Resize the window to leave a margin around the progress bar
	BRect size = bar->Bounds();
	ResizeTo(size.Width() + 6, size.Height() + 6);
	Show();
}


NSDownloadWindow::~NSDownloadWindow()
{
	download_context_abort(ctx);
	download_context_destroy(ctx);
}


void
NSDownloadWindow::MessageReceived(BMessage* message)
{
	switch(message->what)
	{
		case B_SAVE_REQUESTED:
		{
			entry_ref directory;
			const char* name;
			struct gui_download_window* dw;
			BFilePanel* source;

			message->FindRef("directory", &directory);
			message->FindString("name", &name);
			message->FindPointer("dw", (void**)&dw);

			BDirectory dir(&directory);
			BFile* storage = new BFile(&dir, name,
				B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
			dw->storageLock->Lock();

			BMallocIO* tempstore = dynamic_cast<BMallocIO*>(dw->storage);

			storage->Write(tempstore->Buffer(), tempstore->BufferLength());
			delete dw->storage;

			if (success)
				delete storage; // File is already finished downloading !
			else
				dw->storage = storage;
			dw->storageLock->Unlock();

			message->FindPointer("source", (void**)&source);
			delete source;			

			break;
		}
		default:
			BWindow::MessageReceived(message);
	}
}


void
NSDownloadWindow::Progress(int size)
{
	progress += size;

	char* buffer = human_friendly_bytesize(progress);
	strcat(buffer, "/");

	bar->LockLooper();
	bar->Update(size, NULL, buffer);
	bar->Invalidate();
	bar->UnlockLooper();
}


void
NSDownloadWindow::Success()
{
	bar->LockLooper();
	bar->SetBarColor(ui_color(B_SUCCESS_COLOR));
	bar->UnlockLooper();

	success = true;
}


void
NSDownloadWindow::Failure(const char* error)
{
	bar->LockLooper();
	bar->Update(0, NULL, error);
	bar->SetBarColor(ui_color(B_FAILURE_COLOR));
	bar->UnlockLooper();
}


static struct gui_download_window *gui_download_window_create(download_context *ctx,
		struct gui_window *parent)
{
	struct gui_download_window *download = (struct gui_download_window*)malloc(sizeof *download);
	if (download == NULL)
		return NULL;

	download->storageLock = new BLocker("storage_lock");
	download->storage = new BMallocIO();
	download->ctx = ctx;

	download->window = new NSDownloadWindow(ctx);

	// Also ask the user where to save the file
	BMessage* msg = new BMessage(B_SAVE_REQUESTED);

	BFilePanel* panel = new BFilePanel(B_SAVE_PANEL,
		new BMessenger(download->window), NULL, 0, false);

	panel->SetSaveText(download_context_get_filename(ctx));

	msg->AddPointer("source", panel);
	msg->AddPointer("dw", download);
	panel->SetMessage(msg);
	
	panel->Show();

	return download;
}


static nserror gui_download_window_data(struct gui_download_window *dw, 
		const char *data, unsigned int size)
{
	dw->window->Progress(size);

	dw->storageLock->Lock();
	dw->storage->Write(data, size);
	dw->storageLock->Unlock();

	return NSERROR_OK;
}


static void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
	dw->window->Failure(error_msg);

	delete dw->storageLock;
	delete dw->storage;
}


static void gui_download_window_done(struct gui_download_window *dw)
{
	dw->window->Success();

	dw->storageLock->Lock();

	// Only delete if the storage is already a file. Else, we must wait for the
	// user to select something in the BFilePanel!
	BFile* file = dynamic_cast<BFile*>(dw->storage);
	delete file;
	if (file)
		delete dw->storageLock;
	else
		dw->storageLock->Unlock();
}

static struct gui_download_table download_table = {
	gui_download_window_create,
	gui_download_window_data,
	gui_download_window_error,
	gui_download_window_done,
};

struct gui_download_table *beos_download_table = &download_table;

