/*
 * Copyright 2010, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef WINDOW_STACK_H
#define WINDOW_STACK_H


#include <Window.h>


class BWindowStack {
public:
								BWindowStack(BWindow* window);
								~BWindowStack();

			status_t			AddWindow(const BWindow* window);
			status_t			AddWindow(const BMessenger& window);
			status_t			AddWindowAt(const BWindow* window,
									int32 position);
			status_t			AddWindowAt(const BMessenger& window,
									int32 position);

			status_t			RemoveWindow(const BWindow* window);
			status_t			RemoveWindow(const BMessenger& window);
			status_t			RemoveWindowAt(int32 position,
									BMessenger* window = NULL);

			int32				CountWindows();

			status_t			WindowAt(int32 position,
								BMessenger& messenger);
			bool				HasWindow(const BWindow* window);
			bool				HasWindow(const BMessenger& window);

private:
			status_t			_AttachMessenger(const BMessenger& window);
			status_t			_ReadMessenger(BMessenger& window);
			status_t			_StartMessage(int32 what);

			BPrivate::PortLink*	fLink;
};


#endif
