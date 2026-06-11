// Copyright (c) Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#import <AppKit/AppKit.h>

enum
{
	ViewerCameraKey_Forward,
	ViewerCameraKey_Back,
	ViewerCameraKey_Left,
	ViewerCameraKey_Right,
	ViewerCameraKey_Up,
	ViewerCameraKey_Down,
	ViewerCameraKey_Fast,
	ViewerCameraKey_Slow,
	ViewerCameraKey_Count,
};

struct JoltCViewer_MacOSInputState
{
	bool keys[ViewerCameraKey_Count];
	bool rightMouseDown;
	bool focusRequested;
	float mouseDeltaX;
	float mouseDeltaY;
	float wheelDelta;
};

static void JoltCViewer_SetMacOSKey(JoltCViewer_MacOSInputState* state, NSString* characters, bool down)
{
	if (characters.length == 0)
		return;

	unichar key = [[characters lowercaseString] characterAtIndex:0];
	switch (key)
	{
	case 'w': state->keys[ViewerCameraKey_Forward] = down; break;
	case 's': state->keys[ViewerCameraKey_Back] = down; break;
	case 'a': state->keys[ViewerCameraKey_Left] = down; break;
	case 'd': state->keys[ViewerCameraKey_Right] = down; break;
	case 'e': state->keys[ViewerCameraKey_Up] = down; break;
	case 'q': state->keys[ViewerCameraKey_Down] = down; break;
	case 'f':
		if (down)
			state->focusRequested = true;
		break;
	default:
		break;
	}
}

static void JoltCViewer_UpdateMacOSModifiers(JoltCViewer_MacOSInputState* state, NSEventModifierFlags flags)
{
	state->keys[ViewerCameraKey_Fast] = (flags & NSEventModifierFlagShift) != 0;
	state->keys[ViewerCameraKey_Slow] = (flags & (NSEventModifierFlagControl | NSEventModifierFlagOption)) != 0;
}

bool JoltCViewer_MacOSPollEvents(JoltCViewer_MacOSInputState* state)
{
	@autoreleasepool
	{
		NSApplication* app = [NSApplication sharedApplication];
		[app finishLaunching];
		JoltCViewer_UpdateMacOSModifiers(state, [NSEvent modifierFlags]);

		for (;;)
		{
			NSEvent* event = [app nextEventMatchingMask:NSEventMaskAny
											 untilDate:[NSDate distantPast]
												inMode:NSDefaultRunLoopMode
											   dequeue:YES];
			if (event == nil)
				break;

			switch (event.type)
			{
			case NSEventTypeKeyDown:
				if (!event.isARepeat)
					JoltCViewer_SetMacOSKey(state, event.charactersIgnoringModifiers, true);
				break;
			case NSEventTypeKeyUp:
				JoltCViewer_SetMacOSKey(state, event.charactersIgnoringModifiers, false);
				break;
			case NSEventTypeFlagsChanged:
				JoltCViewer_UpdateMacOSModifiers(state, event.modifierFlags);
				break;
			case NSEventTypeRightMouseDown:
				state->rightMouseDown = true;
				break;
			case NSEventTypeRightMouseUp:
				state->rightMouseDown = false;
				break;
			case NSEventTypeRightMouseDragged:
				if (state->rightMouseDown)
				{
					state->mouseDeltaX += event.deltaX;
					state->mouseDeltaY += event.deltaY;
				}
				break;
			case NSEventTypeScrollWheel:
				state->wheelDelta += event.scrollingDeltaY * 0.1f;
				break;
			default:
				break;
			}

			[app sendEvent:event];
		}

		[app updateWindows];

		for (NSWindow* window in [app windows])
		{
			if ([window isVisible])
				return true;
		}

		return false;
	}
}
